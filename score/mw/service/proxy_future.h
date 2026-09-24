/*********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_PROXY_FUTURE_H
#define SCORE_MW_SERVICE_PROXY_FUTURE_H

#include "score/concurrency/future/interruptible_future.h"
#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/result/result.h"

#include <score/utility.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace score::mw::service
{

/// @brief Move-only InterruptibleFuture which cannot be shared.
///
/// @details This is a thin wrapper around InterruptibleFuture that provides proxy-specific type information.
/// Service discovery lifecycle is managed separately via StopServiceDiscoveryAction held by ProxyContainer.
/// cf. score/concurrency/future/interruptible_future.h
///
/// @tparam ProxyType the type of the proxy(s) which are stored in the future's shared state
///
template <typename ProxyType>
class ProxyFuture final : protected score::concurrency::InterruptibleFuture<ProxyType>
{
    using BaseFuture = score::concurrency::InterruptibleFuture<ProxyType>;

  public:
    using ElementType = ProxyType;

    constexpr ProxyFuture() noexcept = default;
    // coverity[autosar_cpp14_a10_3_1_violation] Destructor is not virtual method
    ~ProxyFuture() noexcept = default;

    // NOLINTNEXTLINE(google-explicit-constructor): for implicit conversions from
    // score::concurrency::InterruptibleFuture
    constexpr ProxyFuture(BaseFuture&& future) noexcept : BaseFuture(std::move(future)) {}

    constexpr ProxyFuture(ProxyFuture&&) noexcept = default;
    constexpr ProxyFuture(const ProxyFuture&) noexcept = delete;
    constexpr ProxyFuture& operator=(ProxyFuture&&) & noexcept = default;
    constexpr ProxyFuture& operator=(const ProxyFuture&) & noexcept = delete;

    using BaseFuture::Get;

    /// @brief Register a continuation that can only execute while the scoped callback is active.
    /// @details This overload intentionally accepts only ScopedFunction variants to make continuation lifetime
    ///          assumptions explicit and reviewable.
    auto Then(score::safecpp::MoveOnlyScopedFunction<void(score::Result<ProxyType>&)> callback) noexcept
    {
        return BaseFuture::Then(std::move(callback));
    }

    template <typename Callback>
    auto Then(Callback&&) = delete;

    using BaseFuture::Valid;
    using BaseFuture::Wait;
    using BaseFuture::WaitFor;
    using BaseFuture::WaitUntil;

    /// @brief Checks whether the Future already holds a value or error
    bool Ready() const noexcept
    {
        return this->WaitFor({}, std::chrono::seconds{0}).has_value();
    }

  private:
    // deliberately declared private since it would otherwise bypass our cleanup mechanism in case users would call it!
    using BaseFuture::Share;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_FUTURE_H
