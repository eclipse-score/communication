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
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <string>
#include <variant>

namespace score::mw::com::impl
{

lola::ElementFqId GetElementFqId(const HandleType& handle,
                                 const LolaServiceTypeDeployment& lola_type_deployment,
                                 const std::string_view service_element_name,
                                 const ServiceElementType element_type)
{
    const auto instance_id = handle.GetInstanceId();
    const auto* const lola_instance_id = std::get_if<LolaServiceInstanceId>(&(instance_id.binding_info_));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_instance_id != nullptr,
                                                "ServiceInstanceId does not contain lola binding.");

    const std::string service_element_name_str{service_element_name};
    const LolaServiceElementId element_id{[element_type, &lola_type_deployment, &service_element_name_str]() {
        switch (element_type)
        {
            case ServiceElementType::EVENT:
                return GetServiceElementId<ServiceElementType::EVENT>(lola_type_deployment, service_element_name_str);
                break;
            case ServiceElementType::FIELD:
                return GetServiceElementId<ServiceElementType::FIELD>(lola_type_deployment, service_element_name_str);
                break;
            case ServiceElementType::METHOD:
                return GetServiceElementId<ServiceElementType::METHOD>(lola_type_deployment, service_element_name_str);
                break;
            case ServiceElementType::INVALID:
            default:
                score::mw::log::LogFatal("lola") << "LookupLolaProxyElement called with invalid ServiceElementType.";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                    false, "LookupLolaProxyElement called with invalid ServiceElementType.");
        }
    }()};

    return {lola_type_deployment.service_id_, element_id, lola_instance_id->GetId(), element_type};
}

}  // namespace score::mw::com::impl
