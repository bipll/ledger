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

#include "ledger/chain/main_chain.hpp"

namespace fetch {
namespace chain {
namespace consensus {

class ConsensusMinerInterface
{
public:
  using Block = ledger::Block;

  ConsensusMinerInterface()          = default;
  virtual ~ConsensusMinerInterface() = default;

  virtual void Mine(Block &block)                      = 0;
  virtual bool Mine(Block &block, uint64_t iterations) = 0;
};

}  // namespace consensus
}  // namespace chain
}  // namespace fetch