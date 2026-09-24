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

#ifndef SCORE_MW_SERVICE_PROXY_NEEDS_H
#define SCORE_MW_SERVICE_PROXY_NEEDS_H

#include "score/mw/service/proxy_container.h"

#include "score/mw/service/details/create_proxies.h"
#include "score/mw/service/details/proxy_spec_traits.h"

#include <score/assert.hpp>
#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::service
{

// Require ProxyNeedsFactory to create these ProxyNeeds based on provided Strategies.
// coverity[autosar_cpp14_m3_2_3_violation] see above justification
template <typename ProxyNeeds>
class ProxyNeedsFactory;

template <typename... ProxySpec>
class ProxyNeeds;

/// @brief Class managing requested proxies once service discovery got initiated for a particular `ProxyNeeds` instance.
/// @tparam ProxySpec The specification which proxies got requested
template <typename... ProxySpec>
class RequestedProxies
{
  public:
    using Container = ProxyContainer<ProxySpec...>;

    constexpr RequestedProxies() noexcept = default;
    ~RequestedProxies() noexcept = default;

    constexpr RequestedProxies(RequestedProxies&&) noexcept = default;
    constexpr RequestedProxies(const RequestedProxies&) noexcept = delete;
    constexpr RequestedProxies& operator=(RequestedProxies&&) & noexcept = default;
    constexpr RequestedProxies& operator=(const RequestedProxies&) & noexcept = delete;

    /// @brief Waits until the mandatory proxies found their corresponding service(s).
    /// @details This method can only be called once since our internal state gets reset while doing so.
    /// @param stop_token in case stop got requested, the waiting will end resulting in an unpopulated ProxyContainer.
    /// @return A ProxyContainer with the created proxies according to their build specification; nullptr for mandatory
    ///         proxies if the waiting got aborted via the provided stop_token.
    [[nodiscard]] Container WaitForMandatoryProxies(score::cpp::stop_token stop_token)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            builder_results_.has_value(),
            "Current internal state of `RequestedProxies` instance does not or "
            "no longer allow method `WaitForMandatoryProxies()` to be called!");

        Container container{details::ProxyCreator<ProxySpec...>::GetProxies(std::move(stop_token),
                                                                            std::move(builder_results_).value())};
        builder_results_ = {};
        return container;
    }

    /// @brief Returns a populated ProxyContainer after service discovery got initiated already beforehand.
    /// @note This method is only available in case no mandatory proxies got requested!
    /// @details This method can only be called once since our internal state gets reset while doing so.
    /// @return A ProxyContainer with the created proxies according to their build specification.
    [[nodiscard]] Container GetProxyContainer()
    {
        // This a false-positive, fold over operator&& using the pack
        // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
        // coverity[autosar_cpp14_a5_2_6_violation : FALSE] see above justification
        // coverity[autosar_cpp14_a7_1_8_violation] False positive, constexpr is not part of a decl here but C++17 "if"
        // coverity[autosar_cpp14_m6_4_1_violation] False positive, statement is a compound statement
        if constexpr (((details::ProxySpecTraits<ProxySpec>::IsMandatory::value) || ...))
        {
            static_assert(not(details::ProxySpecTraits<ProxySpec>::IsMandatory::value || ...),
                          "`RequestedProxies`' method `GetProxyContainer()` is not supported to be used once mandatory "
                          "proxies got requested. In such case, `WaitForMandatoryProxies()` must be used instead!");
        }
        else
        {
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
                builder_results_.has_value(),
                "Current internal state of `RequestedProxies` instance does not or "
                "no longer allow method `GetProxyContainer()` to be called!");

            Container container{details::ProxyCreator<ProxySpec...>::GetProxies(std::move(builder_results_).value())};
            builder_results_ = {};
            return container;
        }
    }

    /// @brief Returns the number of contained requested proxies, if any
    /// @return Number of contained requested proxies
    [[nodiscard]] std::size_t Count() const noexcept
    {
        return (builder_results_.has_value() ? std::tuple_size_v<typename decltype(builder_results_)::value_type> : 0U);
    }

  private:
    using BuilderResults = std::tuple<typename details::ProxySpecTraits<ProxySpec>::BuilderReturn...>;

    // In order that users do not depend on implementation details, we only expose on user facing classes the bare
    // necessary. Thus we have friend classes that expose internals for our implementation. Design decision for better
    // encapsulation.
    // coverity[autosar_cpp14_a11_3_1_violation] see above justification
    friend class ProxyNeeds<ProxySpec...>;
    /// @note This class must only get constructed by `ProxyNeeds`.
    explicit RequestedProxies(BuilderResults builder_results) noexcept : builder_results_{std::move(builder_results)} {}

    std::optional<BuilderResults> builder_results_{};
};

/// @brief ProxyNeeds shall be used to declare for an application what kind of proxy interfaces are needed.
/// @tparam ProxySpec The specification which proxies are needed
template <typename... ProxySpec>
class ProxyNeeds
{
  public:
    constexpr ProxyNeeds() noexcept = default;
    ~ProxyNeeds() noexcept = default;

    constexpr ProxyNeeds(ProxyNeeds&&) noexcept = default;
    constexpr ProxyNeeds(const ProxyNeeds&) noexcept = delete;
    constexpr ProxyNeeds& operator=(ProxyNeeds&&) & noexcept = default;
    constexpr ProxyNeeds& operator=(const ProxyNeeds&) & noexcept = delete;

    using Container = typename RequestedProxies<ProxySpec...>::Container;
    using HolderTypes = std::tuple<typename details::ProxySpecTraits<ProxySpec>::HolderType...>;
    using UserCallbacks = std::tuple<typename details::ProxySpecTraits<ProxySpec>::UserCallback...>;
    using InitCallbacks = std::tuple<typename details::ProxySpecTraits<ProxySpec>::InitCallback...>;

    /// @brief Registers callbacks for each proxy which shall get invoked once a proxy's corresp. service got found.
    ///
    /// WithOnServiceFound should only be called once (either with UserCallbacks or InitCallbacks). Subsequent calls
    /// will not register the callback.
    ///
    /// @param callbacks user's callbacks for each proxy which shall get invoked once the corresp. service got found.
    ProxyNeeds& WithOnServiceFound(UserCallbacks callbacks) &
    {
        static_assert(std::tuple_size_v<decltype(callbacks)> == sizeof...(ProxySpec),
                      "number of provided callbacks does not match number of specified proxies");

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            builder_instances_.has_value(),
            "Current internal state of `ProxyNeeds` instance does not or "
            "no longer allow method `WithOnServiceFound()` to be called!");

        RegisterOnServiceFoundCallbacks(builder_instances_.value(),
                                        std::move(callbacks),
                                        std::make_index_sequence<std::tuple_size_v<decltype(callbacks)>>{});

        return *this;
    }

    /// @brief Registers initialization callbacks for each proxy which shall get invoked once a proxy's corresp. service
    /// got found.
    ///
    /// WithOnServiceFound should only be called once (either with UserCallbacks or InitCallbacks). Subsequent calls
    /// will not register the callback.
    ///
    /// @param callbacks user's initialization callbacks for each proxy which shall get invoked once the corresp.
    /// service got found.
    ProxyNeeds& WithOnServiceFound(InitCallbacks callbacks) &
    {
        static_assert(std::tuple_size_v<decltype(callbacks)> == sizeof...(ProxySpec),
                      "number of provided callbacks does not match number of specified proxies");

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            builder_instances_.has_value(),
            "Current internal state of `ProxyNeeds` instance does not or "
            "no longer allow method `WithOnServiceFound()` to be called!");

        RegisterOnServiceFoundCallbacks(builder_instances_.value(),
                                        std::move(callbacks),
                                        std::make_index_sequence<std::tuple_size_v<decltype(callbacks)>>{});

        return *this;
    }

    /// @brief Registers callbacks for each proxy which shall get invoked once a proxy's corresp. service got found
    /// @param callbacks user's callbacks for each proxy which shall get invoked once the corresp. service got found
    ProxyNeeds&& WithOnServiceFound(UserCallbacks callbacks) &&
    {
        return std::move(this->WithOnServiceFound(std::move(callbacks)));
    }

    /// @brief Initiates the service discovery for all proxies.
    /// @note Service discovery remains active only as long as the returned `RequestedProxies` or
    ///       their corresponding `ProxyContainer` respectively its `ProxyFuture` elements are alive.
    /// @details This method can only be called once since our internal state gets reset while doing so.
    [[nodiscard]] RequestedProxies<ProxySpec...> InitiateServiceDiscovery()
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            builder_instances_.has_value(),
            "Current internal state of `ProxyNeeds` instance does not or "
            "no longer allow method `InitiateServiceDiscovery()` to be called!");

        auto builder_results =
            details::ProxyCreator<ProxySpec...>::RequestProxies(std::nullopt, std::move(builder_instances_).value());
        builder_instances_ = {};

        RequestedProxies<ProxySpec...> requested_proxies{std::move(builder_results)};
        return requested_proxies;
    }

    /// @brief Initiates the service discovery for all proxies.
    /// @details This method can only be called once since our internal state gets reset while doing so.
    /// @param stop_token to be used by the service discovery functionality to check whether stop got requested
    [[nodiscard]] RequestedProxies<ProxySpec...> InitiateServiceDiscovery(score::cpp::stop_token stop_token)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            builder_instances_.has_value(),
            "Current internal state of `ProxyNeeds` instance does not or "
            "no longer allow method `InitiateServiceDiscovery()` to be called!");

        auto builder_results = details::ProxyCreator<ProxySpec...>::RequestProxies(
            std::move(stop_token), std::move(builder_instances_).value());
        builder_instances_ = {};

        RequestedProxies<ProxySpec...> requested_proxies{std::move(builder_results)};
        return requested_proxies;
    }

    /// @brief Initiates service discovery for all proxies and waits until mandatory ones found their corresp. service.
    /// @param stop_token in case stop got requested, the waiting will end resulting in an unpopulated ProxyContainer
    /// @return A ProxyContainer with the created proxies according to their build specification; nullptr for
    ///         mandatory proxies if the waiting got aborted via the provided stop_token.
    [[nodiscard]] Container WaitForMandatoryProxies(score::cpp::stop_token stop_token)
    {
        return InitiateServiceDiscovery(stop_token).WaitForMandatoryProxies(stop_token);
    }

  private:
    using Builders = std::tuple<std::unique_ptr<typename details::ProxySpecTraits<ProxySpec>::BuilderType>...>;

    // Helper function to iterate through tuples via fold expression
    template <typename... Builders, typename... Callbacks, std::size_t... index>
    static void RegisterOnServiceFoundCallbacks(std::tuple<Builders...>& proxy_builders,
                                                std::tuple<Callbacks...>&& on_found_callbacks,
                                                const std::index_sequence<index...> index_sequence)
    {
        static_assert(sizeof...(Builders) == sizeof...(Callbacks),
                      "number of provided callbacks does not match number of specified builders");

        score::cpp::ignore = index_sequence;
        (std::get<index>(proxy_builders)
             ->WithOnServiceFound(std::forward<std::tuple_element_t<index, std::tuple<Callbacks...>>>(
                 std::get<index>(on_found_callbacks))),
         ...);
    }

    template <typename>
    // In order that users do not depend on implementation details, we only expose on user facing classes the bare
    // necessary. Thus we have friend classes that expose internals for our implementation. Design decision for better
    // encapsulation.
    // coverity[autosar_cpp14_a11_3_1_violation] see above justification
    friend class ProxyNeedsFactory;

    /// @note This class should not be directly constructed by the user, `ProxyNeedsFactory` shall be used for that.
    explicit ProxyNeeds(Builders builders) noexcept : builder_instances_{std::move(builders)} {}

    std::optional<Builders> builder_instances_{};
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_NEEDS_H
