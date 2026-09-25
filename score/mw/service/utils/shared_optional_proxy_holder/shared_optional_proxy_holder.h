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

#ifndef SCORE_MW_SERVICE_UTILS_SHARED_OPTIONAL_PROXY_HOLDER_SHARED_OPTIONAL_PROXY_HOLDER_H
#define SCORE_MW_SERVICE_UTILS_SHARED_OPTIONAL_PROXY_HOLDER_SHARED_OPTIONAL_PROXY_HOLDER_H

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/mw/service/proxy_data.h"
#include "score/mw/service/single_instance_holder.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <memory>
#include <utility>

namespace score::mw::service::utils
{

/// @brief Thread-safe, shareable holder for optional proxies extracted from OptionalProxyData
/// @details This class manages the lifecycle of an optional proxy by:
///          - Accepting OptionalProxyData and optional callback in constructor
///          - Managing its own internal Scope for callback lifetime
///          - Eagerly registering extraction callback via .Then() on proxy_data.GetProxyFuture()
///          - Automatically extracting the proxy when the proxy future resolves
///          - Invoking optional callback with Result<ProxyType>& parameter
///          - Providing safe access via IsAvailable() and Get()
///          - Enabling sharing across components via copy semantics
///
/// @note Thread-safe for concurrent access to IsAvailable() and Get()
/// @note Callback receives score::Result<ProxyType>& parameter (same signature as ProxyFuture::Then() expects)
/// @note If callback provided: invoked BEFORE proxy storage to allow user access to result before move
/// @note IsAvailable() returns true only AFTER callback completes (if result had value)
/// @note Get() requires IsAvailable() to return true
///
/// @tparam ProxyType the type of the proxy (e.g., unique_ptr<VinProxy>)
template <typename ProxyType>
class SharedOptionalProxyHolder final
{
    using ProxyHolder = mw::service::SingleInstanceHolder<ProxyType>;
    using OptionalProxyData = mw::service::OptionalProxyData<ProxyType>;
    using ProxyFutureExtrationCallback = score::safecpp::MoveOnlyScopedFunction<void(score::Result<ProxyHolder>&)>;

  public:
    explicit SharedOptionalProxyHolder(OptionalProxyData proxy_data, ProxyFutureExtrationCallback on_found = {})
        : data_{std::make_shared<Data>(std::move(proxy_data))}
    {

        ProxyFutureExtrationCallback proxy_extraction_callback{
            data_->Scope(),
            [data_weak_ptr{std::weak_ptr<Data>{data_}},
             on_found_callback = std::move(on_found)](score::Result<ProxyHolder>& result) mutable {
                // Invoke user callback first with result (whether success or error)
                if (on_found_callback)
                {
                    std::invoke(on_found_callback, result);
                }

                // Then store proxy if result contains value (moving out the value)
                if (auto data_shared_ptr = data_weak_ptr.lock(); (data_shared_ptr != nullptr) && result.has_value())
                {
                    data_shared_ptr->SetProxyHolder(std::move(result).value());
                }
            }};

        data_->RegisterExtractionCallback(std::move(proxy_extraction_callback));
    }

    /// @brief Convenience constructor accepting a raw ProxyFuture. Intended for use in tests only.
    /// @note Without an associated service discovery action the future will never resolve in production.
    ///       Prefer using the constructor that accepts OptionalProxyData in production code.
    explicit SharedOptionalProxyHolder(mw::service::ProxyFuture<ProxyHolder> future,
                                       ProxyFutureExtrationCallback on_found = {})
        : SharedOptionalProxyHolder(OptionalProxyData{std::move(future)}, std::move(on_found))
    {
    }

    constexpr SharedOptionalProxyHolder(const SharedOptionalProxyHolder& source) noexcept = default;
    constexpr SharedOptionalProxyHolder& operator=(const SharedOptionalProxyHolder& source) & noexcept = default;

    constexpr SharedOptionalProxyHolder(SharedOptionalProxyHolder&&) noexcept = default;
    constexpr SharedOptionalProxyHolder& operator=(SharedOptionalProxyHolder&&) & noexcept = default;

    ~SharedOptionalProxyHolder() noexcept = default;

    /// @brief Check if proxy is available
    /// @return true if proxy has been extracted and is ready for use, false otherwise
    [[nodiscard]] constexpr bool IsAvailable() const noexcept
    {
        return (data_ != nullptr) && data_->HasProxy();
    }

    /// @brief Get reference to the extracted proxy
    /// @return Reference to the proxy
    /// @pre IsAvailable() must return true
    // Proxytype&
    [[nodiscard]] ProxyType& Get() const
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(data_ != nullptr);
        return data_->Get();
    }

    /// @brief Get reference to the extracted proxy
    /// @return Reference to the proxy
    [[nodiscard]] ProxyType& operator*() const
    {
        return Get();
    }

    /// @brief Get pointer to the extracted proxy
    /// @return pointer to the proxy
    [[nodiscard]] ProxyType* operator->() const
    {
        return &operator*();
    }

  private:
    class Data
    {
      public:
        constexpr explicit Data(OptionalProxyData optional_proxy_data) noexcept
            : proxy_data{std::move(optional_proxy_data)}
        {
        }

        bool HasProxy() const noexcept
        {
            return proxy_instance.load(std::memory_order_acquire) != nullptr;
        }

        void SetProxyHolder(ProxyHolder proxy_instance_holder)
        {
            // The `Data` class is only visible within `SharedOptionalProxyHolder` and `SetProxyHolder` is called once
            // by the continuation. It is thus guaranteed that exactly one writer exists and we  hence do not need
            // to handle the case of concurrent calls to `SetProxyHolder`
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(proxy_instance == nullptr, "proxy_instance_ already set");
            proxy_holder = std::move(proxy_instance_holder);
            proxy_instance.store(proxy_holder.get(), std::memory_order_release);
        }

        ProxyType& Get()
        {
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(HasProxy(), "Proxy not available");
            return *proxy_holder;
        }

        [[nodiscard]] constexpr auto& Scope() noexcept
        {
            return scope;
        }

        auto RegisterExtractionCallback(ProxyFutureExtrationCallback proxy_extraction_callback)
        {
            auto then_result = proxy_data.GetProxyFuture().Then(std::move(proxy_extraction_callback));
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                then_result.has_value(),
                "Failed to register proxy extraction callback - proxy will never become available");
        }

        // As unique_ptr is not thread safe we will use the ProxyType* for atomic loads and stores before accessing the
        // proxy_holder
        std::atomic<ProxyType*> proxy_instance{nullptr};
        OptionalProxyData proxy_data{};
        ProxyHolder proxy_holder{};
        score::safecpp::Scope<> scope{};
    };

    std::shared_ptr<Data> data_;
};

}  // namespace score::mw::service::utils

#endif  // SCORE_MW_SERVICE_UTILS_SHARED_OPTIONAL_PROXY_HOLDER_SHARED_OPTIONAL_PROXY_HOLDER_H
