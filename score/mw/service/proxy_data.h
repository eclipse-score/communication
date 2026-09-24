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

#ifndef SCORE_MW_SERVICE_PROXY_DATA_H
#define SCORE_MW_SERVICE_PROXY_DATA_H

#include "score/mw/service/multi_instance_holder.h"
#include "score/mw/service/proxy_future.h"
#include "score/mw/service/single_instance_holder.h"

#include <memory>
#include <variant>

namespace score::mw::service
{

// Forward declaration — full definition in proxy_spec_traits.h
template <typename... ProxyBase>
struct Variant;

namespace details
{
template <typename ProxySpec>
struct OptionalProxyFutureType
{
    using Type = mw::service::ProxyFuture<SingleInstanceHolder<ProxySpec>>;
};

template <typename... ProxyTypes>
struct OptionalProxyFutureType<Variant<ProxyTypes...>>
{
    using Type = mw::service::ProxyFuture<std::variant<SingleInstanceHolder<ProxyTypes>...>>;
};

template <typename ProxySpec>
using OptionalProxyFutureTypeT = typename OptionalProxyFutureType<ProxySpec>::Type;
}  // namespace details

/// @brief action to be executed for stopping a Proxy's service discovery
class StopServiceDiscoveryAction
{
  public:
    constexpr StopServiceDiscoveryAction() noexcept = default;
    StopServiceDiscoveryAction(const StopServiceDiscoveryAction&) = delete;
    StopServiceDiscoveryAction& operator=(const StopServiceDiscoveryAction&) = delete;
    virtual ~StopServiceDiscoveryAction() noexcept = default;

    virtual void Stop() noexcept = 0;

  protected:
    constexpr StopServiceDiscoveryAction(StopServiceDiscoveryAction&&) noexcept = default;
    StopServiceDiscoveryAction& operator=(StopServiceDiscoveryAction&&) & noexcept = default;
};

template <typename StoredType>
class ProxyData
{
  public:
    ProxyData(const ProxyData&) = delete;
    constexpr ProxyData(ProxyData&&) noexcept = default;
    ProxyData& operator=(const ProxyData&) = delete;
    ProxyData& operator=(ProxyData&& other) & noexcept
    {
        if (this != &other)
        {
            StopServiceDiscovery();
            stored_data_ = std::move(other.stored_data_);
            service_discovery_ = std::move(other.service_discovery_);
        }
        return *this;
    }
    ~ProxyData() noexcept
    {
        StopServiceDiscovery();
    }

    /// @brief Explicitly stops the service discovery associated with this proxy data.
    /// Idempotent: safe to call multiple times or after destruction of the underlying action.
    /// If not called explicitly, service discovery is stopped automatically in the destructor.
    void StopServiceDiscovery() noexcept
    {
        if (service_discovery_)
        {
            service_discovery_->Stop();
            service_discovery_.reset();
        }
    }

  protected:
    ProxyData() noexcept : stored_data_{}, service_discovery_{nullptr} {};
    explicit ProxyData(StoredType stored_data,
                       std::unique_ptr<StopServiceDiscoveryAction> service_discovery = nullptr) noexcept
        : stored_data_{std::move(stored_data)}, service_discovery_{std::move(service_discovery)}
    {
    }

    StoredType& Get() & noexcept
    {
        return stored_data_;
    }

    const StoredType& Get() const& noexcept
    {
        return stored_data_;
    }

    // Deleted to avoid moving out the `ProxyFuture` but leaving the associated service discovery
    // untouched. Reason is that decoupling these two can have unforeseen consequences.
    StoredType&& Get() && = delete;
    const StoredType&& Get() const&& = delete;

  private:
    StoredType stored_data_;
    std::unique_ptr<StopServiceDiscoveryAction> service_discovery_;
};

template <typename ProxyType>
class OptionalProxyData : public ProxyData<details::OptionalProxyFutureTypeT<ProxyType>>
{
    using ProxyFutureType = details::OptionalProxyFutureTypeT<ProxyType>;
    using Base = ProxyData<ProxyFutureType>;

  public:
    /// @brief Default constructor. Produces a hollow instance with an unsatisfied ProxyFuture and no
    /// service discovery. Intended for use in tests or as a placeholder. Callers must not wait on
    /// GetProxyFuture() of a default-constructed instance as it will never be fulfilled.
    OptionalProxyData() noexcept : Base{} {};
    explicit OptionalProxyData(ProxyFutureType proxy_future,
                               std::unique_ptr<StopServiceDiscoveryAction> service_discovery = nullptr) noexcept
        : ProxyData<ProxyFutureType>{std::move(proxy_future), std::move(service_discovery)}
    {
    }

    OptionalProxyData(const OptionalProxyData&) = delete;
    constexpr OptionalProxyData(OptionalProxyData&&) noexcept = default;
    OptionalProxyData& operator=(const OptionalProxyData&) = delete;
    OptionalProxyData& operator=(OptionalProxyData&&) & noexcept = default;
    ~OptionalProxyData() noexcept = default;

    ProxyFutureType& GetProxyFuture() & noexcept
    {
        return Base::Get();
    }

    const ProxyFutureType& GetProxyFuture() const& noexcept
    {
        return Base::Get();
    }

    // Deleted to avoid moving out the `ProxyFuture` but leaving the associated service discovery
    // untouched. Reason is that decoupling these two can have unforeseen consequences.
    ProxyFutureType&& GetProxyFuture() && = delete;
    const ProxyFutureType&& GetProxyFuture() const&& = delete;
};

template <typename ProxyType>
class MultipleProxyData : public ProxyData<MultiInstanceHolder<ProxyType>>
{
    using MultiInstanceHolder = mw::service::MultiInstanceHolder<ProxyType>;
    using Base = ProxyData<MultiInstanceHolder>;

  public:
    /// @brief Default constructor. Produces a hollow instance with an empty MultiInstanceHolder and no
    /// service discovery. Intended for use in tests or as a placeholder.
    MultipleProxyData() noexcept : Base{} {};
    explicit MultipleProxyData(MultiInstanceHolder instance_holder,
                               std::unique_ptr<StopServiceDiscoveryAction> service_discovery = nullptr) noexcept
        : ProxyData<MultiInstanceHolder>{std::move(instance_holder), std::move(service_discovery)}
    {
    }

    MultipleProxyData(const MultipleProxyData&) = delete;
    constexpr MultipleProxyData(MultipleProxyData&&) noexcept = default;
    MultipleProxyData& operator=(const MultipleProxyData&) = delete;
    MultipleProxyData& operator=(MultipleProxyData&&) & noexcept = default;
    ~MultipleProxyData() noexcept = default;

    MultiInstanceHolder& GetProxyInstances() & noexcept
    {
        return Base::Get();
    }

    const MultiInstanceHolder& GetProxyInstances() const& noexcept
    {
        return Base::Get();
    }

    // Deleted to avoid moving out the `ProxyFuture` but leaving the associated service discovery
    // untouched. Reason is that decoupling these two can have unforeseen consequences.
    MultiInstanceHolder&& GetProxyInstances() && = delete;
    const MultiInstanceHolder&& GetProxyInstances() const&& = delete;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_DATA_H
