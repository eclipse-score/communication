/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_DEPLOYMENT_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_DEPLOYMENT_RESOURCES_H

#include "score/mw/com/impl/bindings/someip/service_instance_endpoint.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <set>
#include <string>
#include <string_view>

namespace score::mw::com::impl::someip
{

/// \brief Combines the type and the instance deployment of a SOME/IP service instance into the endpoint description
/// which the binding works with.
///
/// An instance deployment without an instance id describes a "find any instance" deployment. Since the binding needs a
/// concrete instance id to address the endpoint, such a deployment is mapped to instance id 0, which SOME/IP reserves
/// for exactly that purpose.
inline ServiceInstanceEndpoint MakeServiceInstanceEndpoint(
    const SomeIpServiceInstanceDeployment& instance_deployment,
    const SomeIpServiceTypeDeployment& type_deployment) noexcept
{
    constexpr SomeIpServiceInstanceId::InstanceId kAnyInstanceId{0U};
    const auto instance_id =
        instance_deployment.instance_id_.has_value() ? instance_deployment.instance_id_->GetId() : kAnyInstanceId;
    return ServiceInstanceEndpoint{type_deployment.service_id_, instance_id};
}

/// \brief Collects the names of all events that the service interface provides according to its type deployment.
inline std::set<std::string> GetProvidedEventNames(const SomeIpServiceTypeDeployment& type_deployment) noexcept
{   
    //TODO: change to std::set<std::string, std::less<>>
    std::set<std::string> event_names{};
    for (const auto& event : type_deployment.events_)
    {
        score::cpp::ignore = event_names.insert(event.first);
    }
    return event_names;
}

/// \brief Resolves the SOME/IP wire id of a service element from its binding independent name.
///
/// In contrast to GetServiceElementId(), the element type is a runtime value here, since the binding independent event
/// binding factories only know at runtime whether they create an event or a field.
inline SomeIpServiceElementId GetSomeIpServiceElementId(const SomeIpServiceTypeDeployment& type_deployment,
                                                        const std::string& service_element_name,
                                                        const ServiceElementType element_type)
{
    switch (element_type)
    {
        case ServiceElementType::EVENT:
            return GetServiceElementId<ServiceElementType::EVENT>(type_deployment, service_element_name);
        case ServiceElementType::FIELD:
            return GetServiceElementId<ServiceElementType::FIELD>(type_deployment, service_element_name);
        case ServiceElementType::METHOD:
            return GetServiceElementId<ServiceElementType::METHOD>(type_deployment, service_element_name);
        case ServiceElementType::INVALID:
        default:
            score::mw::log::LogFatal("someip")
                << "GetSomeIpServiceElementId called with unsupported ServiceElementType. Terminating.";
            std::terminate();
    }
}

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_DEPLOYMENT_RESOURCES_H
