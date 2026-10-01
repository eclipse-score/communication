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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_MULTIPLE_INSTANTIATION_STRATEGY_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_MULTIPLE_INSTANTIATION_STRATEGY_H

#include "score/mw/service/backend/common/instantiation_strategy_base.h"

#include "score/mw/service/backend/common/port_identifier_checking.h"
#include "score/mw/service/backend/mw_com/proxy_holder.h"
#include "score/mw/service/proxy_builder_base.h"

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::service::backend::mw_com
{

/// @brief FindService strategy which will call StartFindService and only stop the search once StopFind() is called.
/// @note When using this instantiation strategy, be aware that the corresponding `MultiInstanceHolder` object will
///       always get created immediately even though the multiple proxies get requested as optional ones via
///       `ProxyNeeds`! At the same time, in case they get requested as mandatory ones via `ProxyNeeds`, an
///       empty `MultiInstanceHolder` will be the result of `WaitForMandatoryProxies()` and users in both
///       cases have to observe the returned `MultiInstanceHolder` object to check whether all expected
///       proxies got already found (cf. `MultiInstanceHolder`'s method `WaitUntilInstancesGotFound()`).
template <typename Proxy, typename ProxyImpl, typename MwComProxy, typename PortIdentifier = void>
class MultipleInstantiationStrategy final
    : public common::InstantiationStrategyBase<MwComProxy, internal::ProxyHolderCreator<MwComProxy>>
{
    static_assert(std::is_void_v<PortIdentifier> || common::IsValidPortIdentifierType<PortIdentifier>(),
                  "template parameter `PortIdentifier` lacks a static method "
                  "named `Get` which's result can be converted to std::string!");

    using Base = common::InstantiationStrategyBase<MwComProxy, internal::ProxyHolderCreator<MwComProxy>>;

  public:
    using BaseProxy = Multiple<Proxy>;

    template <typename PortIdentifierType = PortIdentifier,
              std::enable_if_t<std::is_void_v<PortIdentifierType>, bool> = true>
    explicit MultipleInstantiationStrategy(std::string port_identifier) noexcept : Base(std::move(port_identifier))
    {
    }

    template <typename PortIdentifierType = PortIdentifier,
              std::enable_if_t<not(std::is_void_v<PortIdentifierType>), bool> = true>
    MultipleInstantiationStrategy() : Base(PortIdentifier::Get())
    {
    }

    /// @brief Start the service discovery for `MwComProxy`.
    /// @note It must be ensured by the caller of Find() that StopFind() gets invoked *after* Find() returned!
    void Find(std::unique_ptr<typename ProxyBuilderBase<Multiple<Proxy>>::BuilderCallback> on_found)
    {
        Base::StartFind([on_found{std::move(on_found)}](std::vector<std::unique_ptr<MwComProxy>> proxies,
                                                        std::string_view /*port_identifier*/) ->
                        typename Base::ShallStopFindService {
                            for (auto& proxy : proxies)  // LCOV_EXCL_BR_LINE (tooling issue Ticket-188259)
                            {
                                std::invoke(*on_found, std::make_unique<ProxyImpl>(std::move(proxy)));
                            }

                            // We must continue looking for further proxies to get found
                            // as long as StopFind() did not get invoked from outside.
                            return Base::ShallStopFindService::kNo;
                        });
    }

    /// @brief Stop the service discovery for `MwComProxy`.
    // Suppressed coverity finding "A10-2-1", because base class method should not be virtual and this implementation is
    // calling the parent class method
    // coverity[autosar_cpp14_a10_2_1_violation] see above justification
    void StopFind() noexcept override
    {
        Base::StopFind();
    }
};

}  // namespace score::mw::service::backend::mw_com

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_MULTIPLE_INSTANTIATION_STRATEGY_H
