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
#include "score/mw/com/impl/configuration/config_validate.h"

#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_field_id.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"

#include <set>
#include <type_traits>
#include <variant>

namespace score::mw::com::impl::configuration
{

void ValidateUniqueServiceElementIds(const LolaServiceTypeDeployment& deployment)
{
    static_assert(std::is_same<LolaEventId, LolaFieldId>::value,
                  "EventId and FieldId should have the same underlying type.");
    static_assert(std::is_same<LolaEventId, LolaMethodId>::value,
                  "EventId and MethodId should have the same underlying type.");
    std::set<LolaEventId> ids{};

    for (const auto& event : deployment.events_)
    {
        if (!ids.insert(event.second).second)
        {
            score::mw::log::LogFatal("lola") << "Configuration cannot contain duplicate eventId, fieldId, or methodId.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }

    for (const auto& field : deployment.fields_)
    {
        if (!ids.insert(field.second).second)
        {
            score::mw::log::LogFatal("lola") << "Configuration cannot contain duplicate eventId, fieldId, or methodId.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }

    for (const auto& method : deployment.methods_)
    {
        if (!ids.insert(method.second).second)
        {
            score::mw::log::LogFatal("lola") << "Configuration cannot contain duplicate eventId, fieldId, or methodId.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
}

InstanceSpecifier CreateValidInstanceSpecifier(std::string instance_specifier_name)
{
    auto result = InstanceSpecifier::Create(std::move(instance_specifier_name));
    if (!result.has_value())
    {
        score::mw::log::LogFatal("lola") << "Invalid InstanceSpecifier.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return result.value();
}

void CrosscheckAsilLevels(const Configuration& config)
{
    for (const auto& service_instance : config.GetServiceInstances())
    {
        if ((service_instance.second.asilLevel_ == QualityType::kASIL_B) &&
            (config.GetGlobalConfiguration().GetProcessAsilLevel() != QualityType::kASIL_B))
        {
            ::score::mw::log::LogFatal("lola")
                << "Service instance has a higher ASIL than the process. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
}

// coverity[autosar_cpp14_a15_5_3_violation]
void CrosscheckServiceInstancesToTypes(const Configuration& config)
{
    for (const auto& service_instance : config.GetServiceInstances())
    {
        const auto foundServiceType = config.GetServiceTypes().find(service_instance.second.service_);
        if (foundServiceType == config.GetServiceTypes().cend())
        {
            ::score::mw::log::LogFatal("lola")
                << "Service instance " << service_instance.first << "refers to a service type ("
                << service_instance.second.service_.ToString()
                << "), which is not configured. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        // LCOV_EXCL_BR_START: Defensive programming: Parse() currently terminates if the ServiceInstanceDeployment
        // contains anything other than a Lola binding. Therefore, it's impossible to reach this point without
        // a LolaServiceInstanceDeployment.
        if (!std::holds_alternative<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_))
        {
            // LCOV_EXCL_BR_STOP
            // LCOV_EXCL_START defensive programming
            ::score::mw::log::LogFatal("lola")
                << "Service instance " << service_instance.first
                << "refers to an not yet supported binding. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            // LCOV_EXCL_STOP
        }
        if (!std::holds_alternative<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_))
        {
            ::score::mw::log::LogFatal("lola")
                << "Service type " << service_instance.second.service_.ToString()
                << "refers to an not yet supported binding. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        const auto& serviceInstanceDeployment =
            std::get<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_);
        for (const auto& eventInstanceElement : serviceInstanceDeployment.events_)
        {
            const auto& serviceTypeDeployment =
                std::get<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_);
            const auto search = serviceTypeDeployment.events_.find(eventInstanceElement.first);
            if (search == serviceTypeDeployment.events_.cend())
            {
                ::score::mw::log::LogFatal("lola")
                    << "Service instance " << service_instance.first << "event" << eventInstanceElement.first
                    << "refers to an event, which doesn't exist in the referenced service type ("
                    << service_instance.second.service_.ToString() << "). This is invalid, terminating";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            }
        }
        for (const auto& fieldInstanceElement : serviceInstanceDeployment.fields_)
        {
            const auto& serviceTypeDeployment =
                std::get<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_);
            const auto search = serviceTypeDeployment.fields_.find(fieldInstanceElement.first);
            if (search == serviceTypeDeployment.fields_.cend())
            {
                ::score::mw::log::LogFatal("lola")
                    << "Service instance " << service_instance.first << "field" << fieldInstanceElement.first
                    << "refers to a field, which doesn't exist in the referenced service type ("
                    << service_instance.second.service_.ToString() << "). This is invalid, terminating";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            }
        }
    }
}

}  // namespace score::mw::com::impl::configuration
