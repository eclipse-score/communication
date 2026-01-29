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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H

#include <memory>
#include <string>
#include <utility>

namespace score
{
namespace mw
{
namespace service
{
namespace backend
{
namespace mw_com
{

/// @brief Stub for SingleInstantiationStrategy
/// @tparam Proxy The proxy interface type
/// @tparam ProxyImpl The proxy implementation type
/// @tparam MwComProxy The MW_COM-specific proxy type
/// @tparam PortIdentifier Optional port identifier type (defaults to void)
template <typename Proxy, typename ProxyImpl, typename MwComProxy, typename PortIdentifier = void>
class SingleInstantiationStrategy
{
  public:
    using BaseProxy = Proxy;

    // Constructor when PortIdentifier is void (manual port identifier)
    template <typename PortIdentifierProvider = PortIdentifier,
              std::enable_if_t<std::is_void_v<PortIdentifierProvider>, bool> = true>
    explicit SingleInstantiationStrategy(std::string /*port_identifier*/) noexcept
    {
        // Stub implementation
    }

    // Constructor when PortIdentifier is a type with Get() method
    template <typename PortIdentifierType = PortIdentifier,
              std::enable_if_t<!std::is_void_v<PortIdentifierType>, bool> = true>
    SingleInstantiationStrategy()
    {
        // Stub implementation - would call PortIdentifier::Get()
    }

    // Default destructor
    ~SingleInstantiationStrategy() = default;

    // Deleted copy operations
    SingleInstantiationStrategy(const SingleInstantiationStrategy&) = delete;
    SingleInstantiationStrategy& operator=(const SingleInstantiationStrategy&) = delete;

    // Default move operations
    SingleInstantiationStrategy(SingleInstantiationStrategy&&) noexcept = default;
    SingleInstantiationStrategy& operator=(SingleInstantiationStrategy&&) noexcept = default;
};

}  // namespace mw_com
}  // namespace backend
}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H
