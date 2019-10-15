//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "core/byte_array/encoders.hpp"
#include "core/logging.hpp"
#include "core/serializers/counter.hpp"
#include "core/serializers/main_serializer.hpp"
#include "core/service_ids.hpp"
#include "crypto/fetch_identity.hpp"
#include "ledger/chain/block_coordinator.hpp"
#include "ledger/chain/main_chain.hpp"
#include "ledger/chain/transaction_layout_rpc_serializers.hpp"
#include "ledger/protocols/main_chain_rpc_service.hpp"
#include "meta/value_util.hpp"
#include "muddle/packet.hpp"
#include "telemetry/counter.hpp"
#include "telemetry/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

static const uint32_t MAX_CHAIN_REQUEST_SIZE = 10000;
static const uint64_t MAX_SUB_CHAIN_SIZE     = 1000;

namespace fetch {
namespace ledger {
namespace {

using fetch::muddle::Packet;
using fetch::byte_array::ToBase64;

using BlockSerializer        = fetch::serializers::MsgPackSerializer;
using BlockSerializerCounter = fetch::serializers::SizeCounter;
using PromiseState           = fetch::service::PromiseState;
using State                  = MainChainRpcService::State;
using Mode                   = MainChainRpcService::Mode;

/**
 * Map the initial state of the state machine to the particular mode that is being configured.
 *
 * @param mode The mode for the main chain
 * @return The initial state for the state machine
 */
constexpr State GetInitialState(Mode mode) noexcept
{
  switch (mode)
  {
  case Mode::STANDALONE:
    return State::SYNCHRONISED;
  case Mode::PRIVATE_NETWORK:
  case Mode::PUBLIC_NETWORK:
    break;
  }

  return State::REQUEST_HEAVIEST_CHAIN;
}

}  // namespace

MainChainRpcService::MainChainRpcService(MuddleEndpoint &endpoint, MainChain &chain,
                                         TrustSystem &trust, Mode mode)
  : muddle::rpc::Server(endpoint, SERVICE_MAIN_CHAIN, CHANNEL_RPC)
  , mode_(mode)
  , endpoint_(endpoint)
  , chain_(chain)
  , trust_(trust)
  , block_subscription_(endpoint.Subscribe(SERVICE_MAIN_CHAIN, CHANNEL_BLOCKS))
  , main_chain_protocol_(chain_)
  , rpc_client_("R:MChain", endpoint, SERVICE_MAIN_CHAIN, CHANNEL_RPC)
  , state_machine_{std::make_shared<StateMachine>("MainChain", GetInitialState(mode_),
                                                  [](State state) { return ToString(state); })}
  , recv_block_count_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_recv_block_total",
        "The number of received blocks from the network")}
  , recv_block_valid_count_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_recv_block_valid_total",
        "The total number of valid blocks received")}
  , recv_block_loose_count_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_recv_block_loose_total",
        "The total number of loose blocks received")}
  , recv_block_duplicate_count_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_recv_block_duplicate_total",
        "The total number of duplicate blocks received from the network")}
  , recv_block_invalid_count_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_recv_block_invalid_total",
        " The total number of invalid blocks received from the network")}
  , state_request_heaviest_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_state_request_heaviest_total",
        "The number of times in the requested heaviest state")}
  , state_wait_heaviest_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_state_wait_heaviest_total",
        "The number of times in the wait heaviest state")}
  , state_synchronising_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_state_synchronising_total",
        "The number of times in the synchronisiing state")}
  , state_wait_response_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_state_wait_response_total",
        "The number of times in the wait response state")}
  , state_synchronised_{telemetry::Registry::Instance().CreateCounter(
        "ledger_mainchain_service_state_synchronised_total",
        "The number of times in the sychronised state")}
{
  // register the main chain protocol
  Add(RPC_MAIN_CHAIN, &main_chain_protocol_);

  // configure the state machine
  // clang-format off
  state_machine_->RegisterHandler(State::REQUEST_HEAVIEST_CHAIN,  this, &MainChainRpcService::OnRequestHeaviestChain);
  state_machine_->RegisterHandler(State::WAIT_FOR_HEAVIEST_CHAIN, this, &MainChainRpcService::OnWaitForHeaviestChain);
  state_machine_->RegisterHandler(State::REQUEST_FROM_TIP,  this, &MainChainRpcService::OnRequestFromTip);
  state_machine_->RegisterHandler(State::WAIT_FROM_TIP, this, &MainChainRpcService::OnWaitFromTip);
  state_machine_->RegisterHandler(State::FURTHER_FROM_TIP, this, &MainChainRpcService::OnFurtherFromTip);
  state_machine_->RegisterHandler(State::SYNCHRONISING,           this, &MainChainRpcService::OnSynchronising);
  state_machine_->RegisterHandler(State::WAITING_FOR_RESPONSE,    this, &MainChainRpcService::OnWaitingForResponse);
  state_machine_->RegisterHandler(State::SYNCHRONISED,            this, &MainChainRpcService::OnSynchronised);
  // clang-format on

  state_machine_->OnStateChange([](State current, State previous) {
    FETCH_LOG_INFO(LOGGING_NAME, "Changed state: ", ToString(previous), " -> ", ToString(current));
  });

  // set the main chain
  block_subscription_->SetMessageHandler([this](Address const &from, uint16_t, uint16_t, uint16_t,
                                                Packet::Payload const &payload,
                                                Address                transmitter) {
    FETCH_LOG_DEBUG(LOGGING_NAME, "Triggering new block handler");

    BlockSerializer serialiser(payload);

    // deserialize the block
    Block block;
    serialiser >> block;

    // recalculate the block hash
    block.UpdateDigest();

    // dispatch the event
    OnNewBlock(from, block, transmitter);
  });
}

void MainChainRpcService::BroadcastBlock(MainChainRpcService::Block const &block)
{
  // determine the serialised size of the block
  BlockSerializerCounter counter;
  counter << block;

  // allocate the buffer and serialise the block
  BlockSerializer serializer;
  serializer.Reserve(counter.size());
  serializer << block;

  // broadcast the block to the nodes on the network
  endpoint_.Broadcast(SERVICE_MAIN_CHAIN, CHANNEL_BLOCKS, serializer.data());
}

void MainChainRpcService::OnNewBlock(Address const &from, Block &block, Address const &transmitter)
{
  recv_block_count_->increment();

#ifdef FETCH_LOG_DEBUG_ENABLED
  // count how many transactions are present in the block
  for (auto const &slice : block.body.slices)
  {
    for (auto const &tx : slice)
    {
      FETCH_LOG_DEBUG(LOGGING_NAME, "Recv Ref TX: ", ToBase64(tx.digest()));
    }
  }
#endif  // FETCH_LOG_INFO_ENABLED

  if (IsBlockValid(block))
  {
    FETCH_LOG_INFO(LOGGING_NAME, "Recv Block: 0x", block.body.hash.ToHex(),
                   " (from peer: ", ToBase64(from), " num txs: ", block.GetTransactionCount(), ")");

    trust_.AddFeedback(transmitter, p2p::TrustSubject::BLOCK, p2p::TrustQuality::NEW_INFORMATION);

    // add the new block to the chain
    auto const status = chain_.AddBlock(block);

    switch (status)
    {
    case BlockStatus::ADDED:
      recv_block_valid_count_->increment();
      FETCH_LOG_INFO(LOGGING_NAME, "Added new block: 0x", block.body.hash.ToHex());
      break;
    case BlockStatus::LOOSE:
      recv_block_loose_count_->increment();
      FETCH_LOG_INFO(LOGGING_NAME, "Added loose block: 0x", block.body.hash.ToHex());
      break;
    case BlockStatus::DUPLICATE:
      recv_block_duplicate_count_->increment();
      FETCH_LOG_INFO(LOGGING_NAME, "Duplicate block: 0x", block.body.hash.ToHex());
      break;
    case BlockStatus::INVALID:
      recv_block_invalid_count_->increment();
      FETCH_LOG_INFO(LOGGING_NAME, "Attempted to add invalid block: 0x", block.body.hash.ToHex());
      break;
    }
  }
  else
  {
    recv_block_invalid_count_->increment();

    FETCH_LOG_WARN(LOGGING_NAME, "Invalid Block Recv: 0x", block.body.hash.ToHex(),
                   " (from: ", ToBase64(from), ")");
  }
}

MainChainRpcService::Address MainChainRpcService::GetRandomTrustedPeer() const
{
  static random::LinearCongruentialGenerator rng;

  auto const direct_peers = endpoint_.GetDirectlyConnectedPeers();

  if (!direct_peers.empty())
  {
    // generate a random peer index
    std::size_t const index = rng() % direct_peers.size();

    // select the address
    return direct_peers[index];
  }

  return Address{};
}

bool MainChainRpcService::HandleChainResponse(Address const &address, BlockList block_list)
{
  std::unordered_map<BlockStatus, std::size_t> block_stats;

  for (auto &block : block_list)
  {
    FETCH_LOG_WARN(LOGGING_NAME, "Adding a block ", block.body.hash.ToHex(), "  <--  ",
                   block.body.previous_hash.ToHex(), "  (", block.body.block_number);
    // skip the genesis block
    if (block.IsGenesis())
    {
      FETCH_LOG_WARN(LOGGING_NAME, "Looks like genesis block");
      if (block.body.hash != GENESIS_DIGEST)
      {
        FETCH_LOG_WARN(LOGGING_NAME, "Genesis hash mismatch: actual 0x", block.body.hash.ToHex(),
                       ", expected 0x", GENESIS_DIGEST.ToHex(), ", skipping the whole alien chain");
        return false;
      }
      continue;
    }

    // recompute the digest
    block.UpdateDigest();

    // add this block
    LogBuilder block_report(LogLevel::DEBUG, LOGGING_NAME, "Chain sync: ");
    if (block.proof())
    {
      auto const status = chain_.AddBlock(block);
      ++block_stats[status];
      block_report.Log(ToString(status));
    }
    else
    {
      ++block_stats[BlockStatus::INVALID];
      block_report.Log("Bad Proof");
    }
    block_report.Log(" block 0x", block.body.hash.ToHex(), " from muddle://", ToBase64(address));
  }

  LogBuilder sync_summary(LogLevel::INFO, LOGGING_NAME, "Synced Summary:");
  if (block_stats.count(BlockStatus::INVALID))
  {
    sync_summary.SetLevel(LogLevel::WARNING);
    sync_summary.Log(" Invalid: ", block_stats[BlockStatus::INVALID]);
  }
  for (auto status : {BlockStatus::ADDED, BlockStatus::LOOSE, BlockStatus::DUPLICATE})
  {
    sync_summary.Log(' ', ToString(status), ": ", block_stats[status]);
  }
  sync_summary.Log(" from muddle://", ToBase64(address));

  return true;
}

/**
 * Request from a random peer the heaviest chain, starting from genesis
 * and going forward. The client is free to return less blocks than requested.
 *
 */
MainChainRpcService::State MainChainRpcService::OnRequestHeaviestChain()
{
  state_request_heaviest_->increment();

  State next_state{State::REQUEST_HEAVIEST_CHAIN};

  auto const peer = GetRandomTrustedPeer();

  if (!peer.empty())
  {
    current_peer_address_ = peer;
    current_request_      = rpc_client_.CallSpecificAddress(
        current_peer_address_, RPC_MAIN_CHAIN, MainChainProtocol::TIME_TRAVEL, next_hash_requested_,
        MAX_CHAIN_REQUEST_SIZE);

    next_state = State::WAIT_FOR_HEAVIEST_CHAIN;
  }

  state_machine_->Delay(std::chrono::milliseconds{500});
  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnWaitForHeaviestChain()
{
  FETCH_LOG_WARN(LOGGING_NAME, "Waiting for heaviest chain");
  state_wait_heaviest_->increment();

  State next_state{State::WAIT_FOR_HEAVIEST_CHAIN};

  if (!current_request_)
  {
    FETCH_LOG_WARN(LOGGING_NAME, "Missing current request");
    // something went wrong we should attempt to request the chain again
    next_state = State::REQUEST_HEAVIEST_CHAIN;
  }
  else
  {
    // determine the status of the request that is in flight
    auto const status = current_request_->state();

    if (PromiseState::WAITING != status)
    {
      FETCH_LOG_WARN(LOGGING_NAME, "Status of no wait (we need more permanent solution)");
      if (PromiseState::SUCCESS == status)
      {
        FETCH_LOG_WARN(LOGGING_NAME, "Status of success (success gives you a status)");
        // The request was successful, simply hand off the blocks to be added to the chain.
        // Quite likely, we'll need more main chain blocks so preset the state.
        next_state = State::REQUEST_HEAVIEST_CHAIN;

        auto peer_response = current_request_->As<MainChainProtocol::Travelogue>();
        FETCH_LOG_WARN(LOGGING_NAME, "Received ", peer_response.blocks.size(),
                       " blocks, next hash of ", peer_response.next_hash.ToHex());
        for (auto const &b : peer_response.blocks)
        {
          FETCH_LOG_WARN(LOGGING_NAME, "Received a block ", b.body.hash.ToHex(), "  <--  ",
                         b.body.previous_hash.ToHex(), "  (", b.body.block_number);
        }
        if (HandleChainResponse(current_peer_address_, std::move(peer_response.blocks)))
        {
          FETCH_LOG_WARN(LOGGING_NAME, "Now what");
          auto &next_hash = peer_response.next_hash;
          if (next_hash.empty())
          {
            FETCH_LOG_WARN(LOGGING_NAME, "Next hash empty");
            // The remote chain could not resolve forward reference unambiguously.
            next_state           = State::REQUEST_FROM_TIP;
            next_hash_requested_ = Digest{};
            left_edge_           = chain_.GetHeaviestBlock();
            assert(left_edge_);
          }
          else
          {
            FETCH_LOG_WARN(LOGGING_NAME, "Next hash full");
            next_hash_requested_ = std::move(next_hash);
            if (next_hash_requested_ == GENESIS_DIGEST)
            {
              FETCH_LOG_WARN(LOGGING_NAME, "Next hash of genesis");
              // Genesis as next tip to retrieve indicates the whole chain has been received
              // and we can start synchronising.
              next_state = State::SYNCHRONISING;
              // clear the state
              value_util::ZeroAll(current_peer_address_, current_missing_block_);
            }
          }
        }
      }
      else
      {
        FETCH_LOG_INFO(LOGGING_NAME, "Heaviest chain request to: ", ToBase64(current_peer_address_),
                       " failed. Reason: ", service::ToString(status));

        // since we want to sync at least with one chain before proceeding we restart the state
        // machine back to the requesting
        next_state = State::REQUEST_HEAVIEST_CHAIN;
      }
    }
  }

  FETCH_LOG_WARN(LOGGING_NAME, "And I go back to ", ToString(next_state));
  return next_state;
}

/**
 * Request from a random peer the heaviest chain, starting from the newest block
 * and going backwards. The client is free to return less blocks than requested.
 *
 */
MainChainRpcService::State MainChainRpcService::OnRequestFromTip()
{
  state_request_heaviest_->increment();  // we're still retrieving the heaviest chain

  State next_state{State::REQUEST_FROM_TIP};
  retrieval_phase_ = State::REQUEST_FROM_TIP;

  auto const peer = GetRandomTrustedPeer();

  if (!peer.empty())
  {
    current_peer_address_ = peer;
    current_request_      = rpc_client_.CallSpecificAddress(
        current_peer_address_, RPC_MAIN_CHAIN, MainChainProtocol::HEAVIEST_CHAIN,
        left_edge_->body.block_number, MAX_CHAIN_REQUEST_SIZE);

    next_state = State::WAIT_FROM_TIP;
  }

  state_machine_->Delay(std::chrono::milliseconds{500});
  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnWaitFromTip()
{
  state_wait_heaviest_->increment();  // we're still retrieving the heaviest chain

  State next_state{State::WAIT_FROM_TIP};

  if (!current_request_)
  {
    // Something went wrong; we should attempt to request this subchain again.
    next_state = retrieval_phase_;
  }
  else
  {
    // Determine the status of the request that is in flight.
    auto const status = current_request_->state();

    if (PromiseState::SUCCESS == status)
    {
      // The request was successful, simply hand off the blocks to be added to the chain.
      auto  peer_response = current_request_->As<MainChainProtocol::Travelogue>();
      auto &blocks        = peer_response.blocks;
      if (blocks.empty())
      {
        // In this state handler, it indicates an error occured.
        return retrieval_phase_;
      }
      if (right_edge_)
      {
        auto const &expected_body{right_edge_->body};
        auto const &actual_body{blocks.back().body};
        if (expected_body.block_number != actual_body.block_number + 1 ||
            expected_body.previous_hash != actual_body.hash)
        {
          FETCH_LOG_ERROR(LOGGING_NAME,
                          "Error: remote subchain does not end at the block expected.");
          return retrieval_phase_;
        }
      }

      auto const &earliest_body{blocks.front().body};
      auto const &left_edge_body{left_edge_->body};
      assert(earliest_body.block_number > left_edge_body.block_number);
      assert(blocks.size() == 1 || earliest_body.block_number < blocks.back().body.block_number);

      const bool gap_closed{earliest_body.block_number == left_edge_body.block_number + 1};
      if (gap_closed)
      {
        if (earliest_body.previous_hash != left_edge_body.hash)
        {
          // This must be a wrong chain.
          FETCH_LOG_ERROR(LOGGING_NAME, "Gluepoint mismatch at block no. ",
                          earliest_body.block_number, ": actual 0x",
                          earliest_body.previous_hash.ToHex(), ", expected 0x",
                          left_edge_body.hash.ToHex());
          return retrieval_phase_;
        }
      }

      BlockHash earliest_hash;
      if (!gap_closed)
      {
        earliest_hash = earliest_body.hash;
      }

      if (HandleChainResponse(current_peer_address_, std::move(blocks)))
      {
        if (gap_closed)
        {
          // Blocks fit together and there are no more unretrieved blocks left.
          next_state = State::SYNCHRONISING;
          // clear the state
          value_util::ZeroAll(current_peer_address_, current_missing_block_, right_edge_);
        }
        else
        {
          right_edge_ = chain_.GetBlock(earliest_hash);
          assert(right_edge_);

          next_hash_requested_ = std::move(peer_response.next_hash);
          assert(right_edge_->body.previous_hash == next_hash_requested_);

          next_state = State::FURTHER_FROM_TIP;
        }
      }
      else
      {
        next_state = retrieval_phase_;
      }
    }
    else if (status != PromiseState::WAITING)
    {
      FETCH_LOG_INFO(LOGGING_NAME, "Heaviest chain request to: ", ToBase64(current_peer_address_),
                     " failed. Reason: ", service::ToString(status));

      // since we want to sync at least with one chain before proceeding we restart the state
      // machine back to the requesting
      next_state = retrieval_phase_;
    }
  }

  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnFurtherFromTip()
{
  state_request_heaviest_->increment();  // we're still retrieving the heaviest chain

  State next_state{State::FURTHER_FROM_TIP};
  retrieval_phase_ = State::FURTHER_FROM_TIP;

  auto const peer = GetRandomTrustedPeer();

  if (!peer.empty())
  {
    current_peer_address_    = peer;
    uint64_t       gap_width = right_edge_->body.block_number - left_edge_->body.block_number - 1;
    constexpr auto max_size  = static_cast<uint64_t>(MAX_CHAIN_REQUEST_SIZE);
    int64_t        requested_blocks = -static_cast<int64_t>(std::min(gap_width, max_size));

    assert(right_edge_->body.previous_hash == next_hash_requested_);

    current_request_ = rpc_client_.CallSpecificAddress(current_peer_address_, RPC_MAIN_CHAIN,
                                                       MainChainProtocol::TIME_TRAVEL,
                                                       next_hash_requested_, requested_blocks);
    next_state       = State::WAIT_FROM_TIP;
  }

  state_machine_->Delay(std::chrono::milliseconds{500});
  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnSynchronising()
{
  state_synchronising_->increment();

  State next_state{State::SYNCHRONISED};

  // get the next missing block
  auto const missing_blocks = chain_.GetMissingTips();

  if (!missing_blocks.empty())
  {
    current_missing_block_ = *missing_blocks.begin();
    current_peer_address_  = GetRandomTrustedPeer();

    // in the case that we don't trust any one we need to simply wait until we do
    if (current_peer_address_.empty())
    {
      return State::SYNCHRONISING;
    }

    FETCH_LOG_INFO(LOGGING_NAME, "Requesting chain from muddle://", ToBase64(current_peer_address_),
                   " for block ", ToBase64(current_missing_block_));

    // make the RPC call to the block source with a request for the chain
    current_request_ = rpc_client_.CallSpecificAddress(
        current_peer_address_, RPC_MAIN_CHAIN, MainChainProtocol::COMMON_SUB_CHAIN,
        current_missing_block_, chain_.GetHeaviestBlockHash(), MAX_SUB_CHAIN_SIZE);

    next_state = State::WAITING_FOR_RESPONSE;
  }

  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnWaitingForResponse()
{
  state_wait_response_->increment();

  State next_state{State::WAITING_FOR_RESPONSE};

  if (!current_request_)
  {
    next_state = State::SYNCHRONISED;
  }
  else
  {
    // determine the status of the request that is in flight
    auto const status = current_request_->state();

    if (PromiseState::WAITING != status)
    {
      if (PromiseState::SUCCESS == status)
      {
        // the request was successful, simply hand off the blocks to be added to the chain
        HandleChainResponse(current_peer_address_, current_request_->As<BlockList>());
      }
      else
      {
        FETCH_LOG_INFO(LOGGING_NAME, "Chain request to: ", ToBase64(current_peer_address_),
                       " failed. Reason: ", service::ToString(status));

        state_machine_->Delay(std::chrono::seconds{1});
        return State::REQUEST_HEAVIEST_CHAIN;
      }

      // clear the state
      value_util::ZeroAll(current_peer_address_, current_missing_block_);
      next_state = State::SYNCHRONISED;
    }
    else
    {
      FETCH_LOG_WARN(LOGGING_NAME, "Still waiting for heaviest chain response");
      state_machine_->Delay(std::chrono::seconds{1});
    }
  }

  return next_state;
}

MainChainRpcService::State MainChainRpcService::OnSynchronised(State current, State previous)
{
  state_synchronised_->increment();

  State next_state{State::SYNCHRONISED};

  FETCH_UNUSED(current);

  if (chain_.HasMissingBlocks())
  {
    FETCH_LOG_INFO(LOGGING_NAME, "Synchronisation Lost");

    next_state = State::SYNCHRONISING;
  }
  else if (previous != State::SYNCHRONISED)
  {
    FETCH_LOG_INFO(LOGGING_NAME, "Synchronised");
  }
  else
  {
    state_machine_->Delay(std::chrono::milliseconds{100});
  }

  return next_state;
}

bool MainChainRpcService::IsBlockValid(Block &block) const
{
  bool block_valid{false};

  // evaluate if this mining node is correct
  bool const is_valid_miner =
      (Mode::PUBLIC_NETWORK == mode_) ? crypto::IsFetchIdentity(block.body.miner.display()) : true;
  if (is_valid_miner && block.proof())
  {
    block_valid = true;
  }

  return block_valid;
}

}  // namespace ledger
}  // namespace fetch
