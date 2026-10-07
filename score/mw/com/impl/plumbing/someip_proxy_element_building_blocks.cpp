/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/impl/plumbing/someip_proxy_element_building_blocks.h"

#include <score/assert.hpp>

#include <string>
#include <variant>

namespace score::mw::com::impl
{

someip::ElementFqId GetSomeIpElementFqId(const HandleType& handle,
                                        const SomeIpServiceTypeDeployment& someip_type_deployment,
                                        const std::string_view service_element_name,
                                        const ServiceElementType element_type)
{
    const auto instance_id = handle.GetInstanceId();
    const auto* const someip_instance_id = std::get_if<SomeIpServiceInstanceId>(&(instance_id.binding_info_));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(someip_instance_id != nullptr,
                                                "ServiceInstanceId does not contain SOME/IP binding.");

    const std::string service_element_name_str{service_element_name};
    const SomeIpServiceElementId element_id{[element_type, &someip_type_deployment, &service_element_name_str]() {
        switch (element_type)
        {
            case ServiceElementType::EVENT:
                return GetServiceElementId<ServiceElementType::EVENT>(someip_type_deployment, service_element_name_str);
            case ServiceElementType::FIELD:
                return GetServiceElementId<ServiceElementType::FIELD>(someip_type_deployment, service_element_name_str);
            case ServiceElementType::METHOD:
                return GetServiceElementId<ServiceElementType::METHOD>(someip_type_deployment, service_element_name_str);
            case ServiceElementType::INVALID:
            default:
                SCORE_LANGUAGE_FUTURECPP_UNREACHABLE_MESSAGE("Invalid SOME/IP service element type.");
        }
    }()};

    return {someip_type_deployment.service_id_, element_id, someip_instance_id->GetId(), element_type};
}

}  // namespace score::mw::com::impl