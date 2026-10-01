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

#ifndef SCORE_MW_SERVICE_BACKEND_COMMON_TEST_MULTIPLE_IMMEDIATE_INSTANTIATION_STRATEGY_H
#define SCORE_MW_SERVICE_BACKEND_COMMON_TEST_MULTIPLE_IMMEDIATE_INSTANTIATION_STRATEGY_H

#include "score/mw/service/find_service_strategy.h"
#include "score/mw/service/proxy_builder_base.h"

#include "score/mw/service/details/proxy_spec_traits.h"

#include <score/assert.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

namespace score::mw::service::test
{

/// @brief  Find Strategy for testing purposes that immediately calls the BuilderCallback `number_of_found_proxies`
/// times.
///         This is useful for unit testing classes using ProxyNeeds where a strategy needs to be utilized that will
///         return Mock objects.
/// @tparam Proxy Interface that will be provided by the strategy.
/// @tparam ProxyImplFactory A callable that will return an implementation of the interface wrapped in a unique_ptr.
/// @tparam number_of_found_proxies The number of times the BuilderCallback callback will be immediately called with a
///         proxy created by ProxyImplFactory.
template <typename Proxy, typename ProxyImplFactory, std::size_t number_of_found_proxies>
class MultipleImmediateInstantiationStrategy final : public FindServiceStrategy
{
    static_assert(std::is_invocable_v<ProxyImplFactory>, "ProxyImplFactory must be invocable!");
    static_assert(
        std::is_same_v<std::invoke_result_t<ProxyImplFactory>, typename details::ProxySpecTraits<Proxy>::HolderType>,
        "When invoked, ProxyImplFactory must return a ProxySpecTraits::HolderType!");

  public:
    using BaseProxy = Multiple<Proxy>;

    constexpr MultipleImmediateInstantiationStrategy() noexcept = default;

    /// @brief Construct the strategy with a ProxyImplFactory instance, allowing it to carry state (e.g. mocks).
    explicit MultipleImmediateInstantiationStrategy(ProxyImplFactory factory) : factory_{std::move(factory)} {}

    /// @brief Create proxy object via ProxyImplFactory
    void Find(std::unique_ptr<typename ProxyBuilderBase<Multiple<Proxy>>::BuilderCallback> on_found)
    {
        for (std::size_t i = 0U; i < number_of_found_proxies; ++i)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(on_found != nullptr);
            auto impl = std::invoke(factory_);
            std::invoke(*on_found, std::move(impl));
        }
    }

    void StopFind() noexcept override {}

  private:
    ProxyImplFactory factory_;
};

}  // namespace score::mw::service::test

#endif  // SCORE_MW_SERVICE_BACKEND_COMMON_TEST_MULTIPLE_IMMEDIATE_INSTANTIATION_STRATEGY_H
