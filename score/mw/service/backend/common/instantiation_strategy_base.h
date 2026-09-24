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

#ifndef SCORE_MW_SERVICE_BACKEND_COMMON_INSTANTIATION_STRATEGY_BASE_H
#define SCORE_MW_SERVICE_BACKEND_COMMON_INSTANTIATION_STRATEGY_BASE_H

#include "score/mw/service/find_service_strategy.h"

#include <score/assert.hpp>
#include <score/callback.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::service::backend::common
{
namespace detail
{

template <typename T>
struct IsSharedPtr : std::false_type
{
};

template <typename T>
struct IsSharedPtr<std::shared_ptr<T>> : std::true_type
{
};

}  // namespace detail

template <typename BackendProxy, typename ProxyCreator>
class InstantiationStrategyBase : public FindServiceStrategy
{
    static_assert(std::is_invocable_v<ProxyCreator, std::string>, "ProxyCreator must be invocable with a std::string!");

    // The ProxyHolder type must be a shared_ptr so that its ownership is shared between the InstantiationStrategyBase
    // and the find service handler provided to StartFindService. See the docstring in
    // mw/service/backend/mw_com/proxy_holder.h for details.
    using ProxyHolder = std::invoke_result_t<ProxyCreator, std::string>;
    static_assert(detail::IsSharedPtr<ProxyHolder>::value, "ProxyCreator must obtain a shared_ptr");

  protected:
    enum class ShallStopFindService : bool
    {
        kYes = true,
        kNo = false,
    };

    explicit InstantiationStrategyBase(std::string port_identifier) noexcept
        : proxy_holder_{nullptr}, port_identifier_{std::move(port_identifier)}
    {
    }

    /// @brief Start the service discovery for `BackendProxy`.
    /// @note It must be ensured by the caller of Find() that StopFind() will also get invoked!
    void StartFind(score::cpp::callback<ShallStopFindService(std::vector<std::unique_ptr<BackendProxy>>,
                                                             std::string_view)> on_found)
    {
        auto new_proxy_holder = std::invoke(ProxyCreator{}, port_identifier_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(new_proxy_holder != nullptr);

        {
            ProxyHolder expected{nullptr};
            const bool exchanged = std::atomic_compare_exchange_strong(&proxy_holder_, &expected, new_proxy_holder);
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(exchanged);
        }

        new_proxy_holder->StartFindService(
            [on_found{std::move(on_found)}, port_identifier{port_identifier_}](auto& proxy_holder) noexcept {
                const ShallStopFindService shall_stop_find_service =
                    on_found(proxy_holder.ExtractProxies(), port_identifier);
                if (shall_stop_find_service == ShallStopFindService::kYes)
                {
                    proxy_holder.StopFindService();
                }
            });

        bool stop_requested_during_start{false};
        {
            stop_requested_during_start = (std::atomic_load(&proxy_holder_) != new_proxy_holder);
        }
        if (stop_requested_during_start)
        {
            new_proxy_holder->StopFindService();
        }
    }

  public:
    /// @brief Obtain the port identifier which got provided upon construction of this Strategy.
    const auto& GetPortIdentifier() const noexcept
    {
        return port_identifier_;
    }

    /// @brief Stop the service discovery for `BackendProxy`.
    void StopFind() noexcept override
    {
        const ProxyHolder holder_to_stop = std::atomic_exchange(&proxy_holder_, ProxyHolder{nullptr});
        if (holder_to_stop != nullptr)
        {
            holder_to_stop->StopFindService();
        }
    }

    /// @brief Check whether service discovery for `BackendProxy` got stopped.
    bool IsStopped() const noexcept
    {
        return std::atomic_load(&proxy_holder_) == nullptr;
    }

  private:
    ProxyHolder proxy_holder_;
    const std::string port_identifier_;
};

}  // namespace score::mw::service::backend::common

#endif  // SCORE_MW_SERVICE_BACKEND_COMMON_INSTANTIATION_STRATEGY_BASE_H
