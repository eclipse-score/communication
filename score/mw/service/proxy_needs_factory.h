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

#ifndef SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H
#define SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H

#include "score/mw/service/details/proxy_builder.h"
#include "score/mw/service/details/proxy_needs_factory_checker.h"
#include "score/mw/service/proxy_needs.h"

#include <memory>
#include <tuple>

namespace score::mw::service
{

/// @brief This factory creates `ProxyNeeds` based on provided `FindStrategies`. It shall be used only outside of
///        application core logics and rather be used to inject instances of ProxyNeeds into the applications.
/// @tparam ProxyNeeds The ProxyNeeds defined by an application
template <typename ProxyNeeds>
class ProxyNeedsFactory
{
  public:
    explicit ProxyNeedsFactory() = delete;

    /// @brief Checks for compatibility between `ProxyNeeds` and provided `Strategies` and, if compatible, constructs
    ///        the `ProxyNeeds` with the required builders.
    /// @tparam Strategies The strategies to apply for each ProxySpec in ProxyNeeds (need to be in same order)
    /// @return Constructed ProxyNeeds, compile-time error otherwise
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create()
    {
        static_assert((std::is_default_constructible_v<Strategies> && ...));
        return Create(std::make_unique<Strategies>()...);
    }

    /// @brief Checks for compatibility between `ProxyNeeds` and provided `Strategies` and, if compatible, constructs
    ///        the `ProxyNeeds` with the required builders.
    /// @tparam Strategies The Strategy types to apply for each ProxySpec in ProxyNeeds (need to be in same order)
    /// @param strategies The Strategy instances to apply for each ProxySpec in ProxyNeeds (need to be in same order)
    /// @return Constructed ProxyNeeds, compile-time error otherwise
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create(Strategies... strategies)
    {
        static_assert((std::is_move_constructible_v<Strategies> && ...));
        return Create(std::make_unique<Strategies>(std::move(strategies))...);
    }

    /// @brief Checks for compatibility between `ProxyNeeds` and provided `Strategies` and, if compatible, constructs
    ///        the `ProxyNeeds` with the required builders.
    /// @tparam Strategies The Strategy types to apply for each ProxySpec in ProxyNeeds (need to be in same order)
    /// @param strategies The Strategy instances to apply for each ProxySpec in ProxyNeeds (need to be in same order)
    /// @return Constructed ProxyNeeds, compile-time error otherwise
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create(std::unique_ptr<Strategies>... strategies)
    {
        details::ProxyNeedsFactoryChecker<typename ProxyNeeds::HolderTypes>::template AreCompatible<Strategies...>();
        return ProxyNeeds{std::make_tuple(std::unique_ptr<ProxyBuilderBase<typename Strategies::BaseProxy>>{
            std::make_unique<details::ProxyBuilder<Strategies>>(std::move(strategies))}...)};
    }
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H
