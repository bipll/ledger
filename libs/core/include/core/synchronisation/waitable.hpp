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

#include "core/synchronisation/protected.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace fetch {

template <typename T, typename M = std::mutex>
class Waitable : Protected<T, M>
{
private:
  mutable std::condition_variable condition_{};
  using ProtectedPayload = Protected<T, M>;
  using ProtectedPayload::payload_;
  using ProtectedPayload::mutex_;

  template <class F, class... Args>
  using EnableIfVoidT =
      std::enable_if_t<std::is_void<decltype(std::declval<F>()(std::declval<Args>()...))>::value>;
  template <class F, class... Args>
  using EnableIfNonVoidT =
      std::enable_if_t<!std::is_void<decltype(F(std::declval<Args>()...))>::value,
                       decltype(std::declval<F>()(std::declval<Args>()...))>;
  using ArgT      = std::add_lvalue_reference_t<T>;
  using ConstArgT = std::add_lvalue_reference_t<std::add_const_t<T>>;

public:
  template <typename... Args>
  explicit Waitable(Args &&... args);
  Waitable(Waitable const &) = delete;
  Waitable(Waitable &&)      = delete;
  ~Waitable()                = default;

  Waitable &operator=(Waitable const &) = delete;
  Waitable &operator=(Waitable &&) = delete;

  template <typename Handler>
  constexpr EnableIfVoidT<Handler, ArgT> Apply(Handler &&handler)
  {
    return ProtectedPayload::Apply([this, handler](auto &payload) {
      handler(payload);
      condition_.notify_all();
    });
  }

  template <typename Handler>
  constexpr EnableIfNonVoidT<Handler, ArgT> Apply(Handler &&handler)
  {
    return ProtectedPayload::Apply([this, handler](auto &payload) {
      auto &&result = handler(payload);
      condition_.notify_all();

      return std::forward<decltype(result)>(result);
    });
  }

  template <typename Handler>
  constexpr EnableIfVoidT<Handler, ConstArgT> Apply(Handler &&handler) const
  {
    return ProtectedPayload::Apply([this, handler](auto const &payload) {
      handler(payload);
      condition_.notify_all();
    });
  }

  template <typename Handler>
  constexpr EnableIfNonVoidT<Handler, ConstArgT> Apply(Handler &&handler) const
  {
    return ProtectedPayload::Apply([this, handler](auto const &payload) {
      auto &&result = handler(payload);
      condition_.notify_all();

      return std::forward<decltype(result)>(result);
    });
  }

  template <typename Predicate>
  void Wait(Predicate &&predicate) const;
  template <typename Predicate, typename R, typename P>
  bool Wait(Predicate &&predicate, std::chrono::duration<R, P> const &max_wait_time) const;
};

template <typename T, typename M>
template <typename... Args>
Waitable<T, M>::Waitable(Args &&... args)
  : ProtectedPayload(std::forward<Args>(args)...)
{}

template <typename T, typename M>
template <typename Predicate>
void Waitable<T, M>::Wait(Predicate &&predicate) const
{
  std::unique_lock<M> lock{mutex_};

  condition_.wait(lock, [this, predicate = std::forward<Predicate>(predicate)]() -> bool {
    return predicate(payload_);
  });
}

template <typename T, typename M>
template <typename Predicate, typename R, typename P>
bool Waitable<T, M>::Wait(Predicate &&                       predicate,
                          std::chrono::duration<R, P> const &max_wait_time) const
{
  std::unique_lock<M> lock{mutex_};

  return condition_.wait_for(lock, max_wait_time,
                             [this, predicate = std::forward<Predicate>(predicate)]() -> bool {
                               return predicate(payload_);
                             });
}

}  // namespace fetch
