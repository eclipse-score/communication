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

}  // namespace score::mw::com::impl::configuration
