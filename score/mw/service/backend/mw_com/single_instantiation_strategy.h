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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H

#include "score/mw/service/backend/common/instantiation_strategy_base.h"
#include "score/mw/service/backend/common/port_identifier_checking.h"
#include "score/mw/service/backend/mw_com/proxy_holder.h"
#include "score/mw/service/proxy_builder_base.h"

#include "score/mw/log/logging.h"

#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

namespace score::mw::service::backend::mw_com
{

template <typename Proxy, typename ProxyImpl, typename MwComProxy, typename PortIdentifier = void>
class SingleInstantiationStrategy final
    : public common::InstantiationStrategyBase<MwComProxy, internal::ProxyHolderCreator<MwComProxy>>
{
    static_assert(std::is_void_v<PortIdentifier> || common::IsValidPortIdentifierType<PortIdentifier>(),
                  "template parameter `PortIdentifier` lacks a static method "
                  "named `Get` which's result can be converted to std::string");

    using Base = common::InstantiationStrategyBase<MwComProxy, internal::ProxyHolderCreator<MwComProxy>>;

  public:
    using BaseProxy = Proxy;

    template <typename PortIdentifierProvider = PortIdentifier,
              std::enable_if_t<std::is_void_v<PortIdentifierProvider>, bool> = true>
    explicit SingleInstantiationStrategy(std::string port_identifier) noexcept : Base(std::move(port_identifier))
    {
    }

    template <typename PortIdentifierType = PortIdentifier,
              std::enable_if_t<not(std::is_void_v<PortIdentifierType>), bool> = true>
    SingleInstantiationStrategy() : Base(PortIdentifier::Get())
    {
    }

    /// @brief Start the service discovery for `MwComProxy`.
    /// @note It must be ensured by the caller of Find() that StopFind() gets invoked *after* Find() returned!
    void Find(std::unique_ptr<typename ProxyBuilderBase<Proxy>::BuilderCallback> on_found)
    {
        Base::StartFind(
            [on_found{std::move(on_found)}](std::vector<std::unique_ptr<MwComProxy>> mw_com_proxies,
                                            std::string_view port_identifier) -> typename Base::ShallStopFindService {
                const bool no_mw_com_proxies_found{mw_com_proxies.empty() || mw_com_proxies.front() == nullptr};
                if (not(no_mw_com_proxies_found))  // LCOV_EXCL_BR_LINE (tooling issue Ticket-188259)
                {
                    std::invoke(*on_found, std::make_unique<ProxyImpl>(std::move(mw_com_proxies.front())));
                    if (mw_com_proxies.size() == 1U)
                    {
                        return Base::ShallStopFindService::kYes;
                    }
                }

                mw::log::LogStream log_stream{no_mw_com_proxies_found ? mw::log::LogError() : mw::log::LogWarn()};
                // The logging statement is used for diagnostic purposes and does not affect the control
                // flow or logic of the program. The use of the ternary operator as a sub-expression
                // does not introduce any risk to the functionality of the program.
                // coverity[autosar_cpp14_a5_16_1_violation] see above justification
                log_stream << "Found" << (no_mw_com_proxies_found ? 0U : mw_com_proxies.size())
                           << "proxies but expected exactly 1 for instance" << port_identifier;

                return (no_mw_com_proxies_found ? Base::ShallStopFindService::kNo : Base::ShallStopFindService::kYes);
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

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_SINGLE_INSTANTIATION_STRATEGY_H
