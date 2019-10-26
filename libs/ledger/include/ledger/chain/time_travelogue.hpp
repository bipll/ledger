#pragma once
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

#include "core/serializers/base_types.hpp"
#include "ledger/chain/block.hpp"
#include "chain/constants.hpp"

namespace fetch {
namespace ledger {

/**
 * Packet format used to convey heaviest chain for node sync.
 */
template <class B>
struct TimeTravelogue
{
  using BlockType = B;
  using Blocks    = std::vector<BlockType>;

  Blocks blocks;
  Digest next_hash;
  bool   proceed;
};

}  // namespace ledger

namespace serializers {

template <class B, class D>
struct MapSerializer<ledger::TimeTravelogue<B>, D>
  : MapSerializerTemplate<ledger::TimeTravelogue<B>, D,
                          SERIALIZED_STRUCT_FIELD(1, ledger::TimeTravelogue<B>::blocks),
                          SERIALIZED_STRUCT_FIELD(2, ledger::TimeTravelogue<B>::next_hash),
                          SERIALIZED_STRUCT_FIELD(3, ledger::TimeTravelogue<B>::proceed)>
{
};

}  // namespace serializers
}  // namespace fetch
