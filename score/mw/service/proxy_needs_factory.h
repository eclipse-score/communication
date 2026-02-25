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

#ifndef SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H
#define SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H

#include "score/mw/service/proxy_needs.h"

#include <memory>
#include <utility>

namespace score
{
namespace mw
{
namespace service
{

/// @brief Stub factory for creating ProxyNeeds instances
/// @tparam ProxyNeeds The ProxyNeeds type to be created
template <typename ProxyNeeds>
class ProxyNeedsFactory
{
  public:
    ProxyNeedsFactory() = delete;

    /// @brief Creates a ProxyNeeds instance with the provided strategies (stub implementation)
    /// @tparam Strategies The strategy types for each ProxySpec
    /// @return Constructed ProxyNeeds instance
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create()
    {
        // Stub implementation - return default-constructed ProxyNeeds
        return ProxyNeeds{};
    }

    /// @brief Creates a ProxyNeeds instance with the provided strategy instances (stub implementation)
    /// @tparam Strategies The strategy types for each ProxySpec
    /// @param strategies The strategy instances (moved)
    /// @return Constructed ProxyNeeds instance
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create(Strategies... /*strategies*/)
    {
        // Stub implementation - return default-constructed ProxyNeeds
        return ProxyNeeds{};
    }

    /// @brief Creates a ProxyNeeds instance with unique_ptr strategy instances (stub implementation)
    /// @tparam Strategies The strategy types for each ProxySpec
    /// @param strategies The strategy instances as unique_ptrs
    /// @return Constructed ProxyNeeds instance
    template <typename... Strategies>
    [[nodiscard]] static ProxyNeeds Create(std::unique_ptr<Strategies>... /*strategies*/)
    {
        // Stub implementation - return default-constructed ProxyNeeds
        return ProxyNeeds{};
    }
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROXY_NEEDS_FACTORY_H
