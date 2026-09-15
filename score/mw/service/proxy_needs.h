// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef SCORE_MW_SERVICE_PROXY_NEEDS_H
#define SCORE_MW_SERVICE_PROXY_NEEDS_H
#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/service/proxy_future.h"
#include <memory>
#include <tuple>
#include <utility>

namespace score
{
namespace cpp
{
class stop_token;
}  // namespace cpp

namespace mw
{
namespace service
{

/// @brief Wrapper for optional proxy instances
/// @tparam ProxyType The type of the proxy
template <typename ProxyType>
class Optional
{
  public:
    Optional() = default;

    /// @brief Wrap a proxy that is already available
    explicit Optional(std::unique_ptr<ProxyType> proxy)
    {
        score::concurrency::InterruptiblePromise<std::unique_ptr<ProxyType>> promise;
        future_ = std::move(promise.GetInterruptibleFuture().value());
        promise.SetValue(std::move(proxy));
    }

    /// @brief Wrap a future that will resolve once service discovery finds the proxy
    explicit Optional(ProxyFuture<std::unique_ptr<ProxyType>> future) : future_(std::move(future)) {}

    ~Optional() = default;

    Optional(Optional&&) noexcept = default;
    Optional& operator=(Optional&&) noexcept = default;

    Optional(const Optional&) = delete;
    Optional& operator=(const Optional&) = delete;

    /// @brief Implicit conversion to ProxyFuture for compatibility
    /// This allows Optional<T> to be used where ProxyFuture<std::unique_ptr<T>> is expected
    operator ProxyFuture<std::unique_ptr<ProxyType>>() &&
    {
        return std::move(*this).GetProxyFuture();
    }

    /// @brief Get the future that resolves to the wrapped proxy
    [[nodiscard]] ProxyFuture<std::unique_ptr<ProxyType>> GetProxyFuture()
    {
        return std::move(future_);
    }

    /// @brief No-op stub; this fake proxy has no ongoing service discovery to stop
    void StopServiceDiscovery() noexcept {}

  private:
    ProxyFuture<std::unique_ptr<ProxyType>> future_;
};

/// @brief Container for proxy instances
/// @tparam ProxySpec The proxy specification types
template <typename... ProxySpec>
class ProxyContainer
{
  public:
    ProxyContainer() = default;
    ~ProxyContainer() = default;

    ProxyContainer(ProxyContainer&&) noexcept = default;
    ProxyContainer& operator=(ProxyContainer&&) noexcept = default;

    ProxyContainer(const ProxyContainer&) = delete;
    ProxyContainer& operator=(const ProxyContainer&) = delete;

    /// @brief Extract a specific proxy from the container
    /// @tparam ProxyType The type of proxy to extract
    /// @return The extracted proxy instance (stub returns default-constructed)
    template <typename ProxyType>
    [[nodiscard]] ProxyType Extract()
    {
        return ProxyType{};  // Stub: return default-constructed instance
    }
};

/// @brief Stub for RequestedProxies
template <typename... ProxySpec>
class RequestedProxies
{
  public:
    using Container = ProxyContainer<ProxySpec...>;

    constexpr RequestedProxies() noexcept = default;
    ~RequestedProxies() noexcept = default;

    RequestedProxies(RequestedProxies&&) noexcept = default;
    RequestedProxies& operator=(RequestedProxies&&) noexcept = default;

    /// @brief Returns a populated ProxyContainer (stub implementation)
    [[nodiscard]] Container GetProxyContainer()
    {
        return Container{};
    }
};

/// @brief Stub for ProxyNeeds
template <typename... ProxySpec>
class ProxyNeeds
{
  public:
    using Container = typename RequestedProxies<ProxySpec...>::Container;

    constexpr ProxyNeeds() noexcept = default;
    ~ProxyNeeds() noexcept = default;

    ProxyNeeds(ProxyNeeds&&) noexcept = default;
    ProxyNeeds& operator=(ProxyNeeds&&) noexcept = default;

    /// @brief Initiates the service discovery with stop token support (stub implementation)
    /// @param stop_token Token for cancellation support
    /// @return RequestedProxies instance for waiting on proxies
    [[nodiscard]] RequestedProxies<ProxySpec...> InitiateServiceDiscovery(const score::cpp::stop_token& /*stop_token*/)
    {
        return RequestedProxies<ProxySpec...>{};
    }
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROXY_NEEDS_H
