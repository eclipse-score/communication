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

#ifndef SCORE_MW_SERVICE_PROXY_CONTAINER_H
#define SCORE_MW_SERVICE_PROXY_CONTAINER_H

#include "score/mw/service/details/proxy_spec_traits.h"
#include "score/mw/service/details/tuple_index_for_type.h"

#include "score/mw/service/multi_instance_holder.h"
#include "score/mw/service/proxy_future.h"

#include <score/assert.hpp>

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::service
{

namespace details
{
template <typename T>
struct IsOptionalProxySpec : std::false_type
{
};
template <typename T>
struct IsOptionalProxySpec<Optional<T>> : std::true_type
{
};

template <typename T>
struct IsMultipleProxySpec : std::false_type
{
};
template <typename T>
struct IsMultipleProxySpec<Multiple<T>> : std::true_type
{
};
}  // namespace details

/// @brief Holds multiple proxy instances specified by ProxySpecs. The general idea is that a user no longer needs to
/// hold each single proxy in his application logic. This container provides an easy and natural access to reduce code
/// bloat, but still access the different proxy instances.
///
/// @note As explained later, depending on the ProxySpec, the stored type will be different. e.g. Mandatory proxies will
/// be stored as unique_ptr, while optional proxies might be stored as futures (which are already fulfilled or might be
/// fulfilled in the future).
///
/// @tparam ProxySpec The specification of what kind of proxies interfaces are needed (and e.g. if they are optional)
template <typename... ProxySpec>
class ProxyContainer
{
    /// @brief Defines the container which will be used to retrieve all necessary types.
    using ProxyDataTuple = std::tuple<typename details::ProxySpecTraits<ProxySpec>::ResolvedType...>;

    /// @brief Utility for calculating the relative index of a proxy type contained in `ProxyDataTuple`.
    template <typename T, std::size_t type_index>
    // coverity[autosar_cpp14_a0_1_3_violation : FALSE] Method is used by other class functions
    constexpr static auto RelativeIndex() noexcept
    {
        using IndexCalculator =
            details::TupleIndexForType<typename details::ProxySpecTraits<T>::ResolvedType, type_index, ProxyDataTuple>;
        constexpr auto kRelativeIndex = IndexCalculator::Index();
        static_assert(kRelativeIndex != IndexCalculator::kInvalidIndex, "No proxy could be found for provided index!");
        return kRelativeIndex;
    }

  public:
    constexpr ProxyContainer() = default;
    ~ProxyContainer() noexcept = default;

    /// @note This should not be called by users. Instead, the respective methods of class `ProxyNeeds` shall be used.
    constexpr explicit ProxyContainer(ProxyDataTuple proxy_data_tuple) noexcept
        : proxies_with_service_discovery_{std::apply(
              [](auto&&... proxy_data) {
                  return std::make_tuple(std::make_optional(std::move(proxy_data))...);
              },
              std::move(proxy_data_tuple))}
    {
    }

    constexpr ProxyContainer& operator=(const ProxyContainer& other) & noexcept = delete;
    constexpr ProxyContainer& operator=(ProxyContainer&& other) & noexcept = default;
    constexpr ProxyContainer(const ProxyContainer& other) noexcept = delete;
    constexpr ProxyContainer(ProxyContainer&& other) noexcept = default;

    /// @brief Access a proxy instance that was previously built.
    ///
    /// @tparam T The type of the proxy that you want to access.
    /// @tparam type_index If a type appears multiple times, the index can specify which one of them you want to
    /// access (starting at 0, default is 0).
    ///
    /// @return Depending on ProxySpec:
    /// if ProxySpec == Mandatory, then mw::service::SingleInstanceHolder<T>&
    /// if ProxySpec == Multiple,  then mw::service::MultiInstanceHolder<T>&
    /// if ProxySpec == Optional,  then mw::service::ProxyFuture<mw::service::SingleInstanceHolder<T>>&
    /// if ProxySpec == Variant,   then std::variant<mw::service::SingleInstanceHolder<T1>,
    ///                                            mw::service::SingleInstanceHolder<T2>, (etc.)>&
    ///
    /// @note Service discovery remains active for ALL proxies (mandatory, optional, and multiple) until the
    ///       `ProxyContainer` is destroyed or, in the mandatory and optional case, once the proxy was found
    ///       successfully. The `ProxyContainer` manages that via the internal `StopServiceDiscoveryAction` held
    ///       in conjunction with each proxy element.
    template <typename T, std::size_t type_index = 0>
    // coverity[autosar_cpp14_a15_5_3_violation] implicit termination is intended here to indicate contract violation
    [[nodiscard]] auto& Get() noexcept
    {
        auto& container_element = std::get<RelativeIndex<T, type_index>()>(proxies_with_service_discovery_);
        // coverity[autosar_cpp14_a15_4_2_violation] implicit termination is intended here to indicate violation
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(container_element.has_value());

        if constexpr (details::ProxySpecTraits<T>::IsMandatory::value)
        {
            return *container_element;  // SingleInstanceHolder<T>& (or variant)
        }
        else if constexpr (details::IsOptionalProxySpec<T>::value)
        {
            return container_element->GetProxyFuture();  // ProxyFuture<...>&
        }
        else if constexpr (details::IsMultipleProxySpec<T>::value)
        {
            return container_element->GetProxyInstances();  // MultiInstanceHolder<...>&
        }
        else
        {
            static_assert(sizeof(T) == 0, "Unsupported ProxySpec type!");
        }
    }
    /// @brief Moves out a proxy instance from the container, transferring ownership to the caller.
    ///
    /// @tparam T The type of the proxy that you want to extract.
    /// @tparam type_index If a type appears multiple times, the index can specify which one of them you want to
    /// extract (starting at 0, default is 0).
    ///
    /// @return Depending on ProxySpec:
    /// if ProxySpec == Mandatory, then mw::service::SingleInstanceHolder<T>
    /// if ProxySpec == Multiple,  then mw::service::MultipleProxyData<T>
    /// if ProxySpec == Optional,  then mw::service::OptionalProxyData<T>
    /// if ProxySpec == Variant,   then std::variant<mw::service::SingleInstanceHolder<T1>,
    ///                                              mw::service::SingleInstanceHolder<T2>, (etc.)>
    ///
    /// @note Extracting a proxy via `Extract()` transfers ownership of both the proxy and its service discovery to the
    ///       caller. The caller may stop service discovery explicitly at any time by calling `StopServiceDiscovery()`
    ///       on the extracted `ProxyData`, or it will be stopped automatically when the extracted object is destroyed.
    ///       This method must not be called more than once for the same element. After extraction, `Has<T>()` will
    ///       return false and subsequent calls to `Get<T>()` or `Extract<T>()` will terminate. The caller owns the
    ///       proxy instance and its associated service discovery upon return (cf. `Get()`).
    template <typename T, std::size_t type_index = 0>
    // coverity[autosar_cpp14_a15_5_3_violation] implicit termination is intended here to indicate contract violation
    [[nodiscard]] auto Extract() noexcept
    {
        auto& container_element = std::get<RelativeIndex<T, type_index>()>(proxies_with_service_discovery_);
        // coverity[autosar_cpp14_a15_4_2_violation] implicit termination is intended here to indicate violation
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(container_element.has_value());

        auto extracted = std::move(*container_element);
        container_element = std::nullopt;
        return extracted;
    }

    /// @brief Check whether a valid instance of a particular proxy type is contained.
    template <typename T, std::size_t type_index = 0>
    constexpr bool Has() const noexcept
    {
        const auto& container_element = std::get<RelativeIndex<T, type_index>()>(proxies_with_service_discovery_);
        return container_element.has_value();
    }

  private:
    /// @brief Defines the container which will store all necessary types.
    using Container = std::tuple<std::optional<typename details::ProxySpecTraits<ProxySpec>::ResolvedType>...>;
    Container proxies_with_service_discovery_{};
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_CONTAINER_H
