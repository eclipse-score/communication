/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///
#include "score/mw/com/impl/generic_proxy.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

namespace
{

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::vector<std::string_view> GetEventNameList(const InstanceIdentifier& identifier) noexcept
{
    using ReturnType = std::vector<std::string_view>;

    const auto& service_type_deployment = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    auto visitor = score::cpp::overload(
        [](const LolaServiceTypeDeployment& deployment) -> ReturnType {
            ReturnType event_names;
            for (const auto& event : deployment.events_)
            {
                event_names.push_back(std::string_view{event.first});
            }
            return event_names;
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return {};
        });
    return std::visit(visitor, service_type_deployment.binding_info_);
}

}  // namespace

Result<GenericProxy> GenericProxy::Create(HandleType instance_handle) noexcept
{
    auto proxy_binding = ProxyBindingFactory::Create(instance_handle);
    if (proxy_binding == nullptr)
    {
        ::score::mw::log::LogError("lola") << "Could not create GenericProxy as binding could not be created.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    GenericProxy generic_proxy{std::move(proxy_binding), std::move(instance_handle)};

    const auto& instance_identifier = generic_proxy.handle_.GetInstanceIdentifier();
    const auto event_names = GetEventNameList(instance_identifier);
    generic_proxy.FillEventMap(event_names);
    if (!generic_proxy.AreBindingsValid())
    {
        ::score::mw::log::LogError("lola") << "Could not create GenericProxy as binding is invalid.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return generic_proxy;
}

GenericProxy::GenericProxy(std::unique_ptr<ProxyBinding> proxy_binding, HandleType instance_handle)
    : ProxyBase{std::move(proxy_binding), std::move(instance_handle)}
{
}

void GenericProxy::FillEventMap(const std::vector<std::string_view>& event_names) noexcept
{
    for (const auto event_name : event_names)
    {
        if (proxy_binding_->IsEventProvided(event_name))
        {
            const auto emplace_result = events_.emplace(
                std::piecewise_construct, std::forward_as_tuple(event_name), std::forward_as_tuple(*this, event_name));
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second,
                                                        "Could not emplace GenericProxyEvent in map.");
        }
        else
        {
            ::score::mw::log::LogError("lola")
                << "GenericProxy: Event provided in the ServiceTypeDeployment could not be "
                   "found in shared memory. This is likely a configuration error.";
        }
    }
}

GenericProxy::EventMap& GenericProxy::GetEvents() noexcept
{
    // The signature of this function is part of the public API of the GenericProxy, specified in this requirement:
    // broken_link_c/issue/14006006
    // coverity[autosar_cpp14_a9_3_1_violation]
    return events_;
}

}  // namespace score::mw::com::impl
