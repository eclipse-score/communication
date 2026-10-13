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

#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl::configuration
{

// A common function for emplacing an element into a map and terminating the program if the element already exists in
// the map. shall be used by both strategies (JSON & Flatbuffers) parsers.
template <typename Map, typename Key, typename Value>
void EmplaceOrFatal(Map& map, Key&& key, Value&& value, std::string_view element_description)
{
    const auto result = map.emplace(std::forward<Key>(key), std::forward<Value>(value));
    if (!result.second)
    {
        score::mw::log::LogFatal("lola") << element_description << " was configured twice.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

/// \brief Validates that the event, field and method ids of a service type deployment are mutually unique.
/// \details Templated on the concrete binding deployment, since LolaServiceTypeDeployment and
///          SomeIpServiceTypeDeployment are distinct instantiations of the same BindingServiceTypeDeployment template.
template <typename BindingServiceTypeDeploymentType>
void ValidateUniqueServiceElementIds(const BindingServiceTypeDeploymentType& deployment)
{
    using EventIdType = typename BindingServiceTypeDeploymentType::EventIdMapping::mapped_type;
    using FieldIdType = typename BindingServiceTypeDeploymentType::FieldIdMapping::mapped_type;
    using MethodIdType = typename BindingServiceTypeDeploymentType::MethodIdMapping::mapped_type;
    static_assert(std::is_same<EventIdType, FieldIdType>::value,
                  "EventId and FieldId should have the same underlying type.");
    static_assert(std::is_same<EventIdType, MethodIdType>::value,
                  "EventId and MethodId should have the same underlying type.");
    std::set<EventIdType> ids{};

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

InstanceSpecifier CreateValidInstanceSpecifier(std::string instance_specifier_name);

template <typename Container>
void ValidateSingleDeployment(const Container& deployments, const ServiceIdentifierType& service_identifier)
{
    if (deployments.size() != 1U)
    {
        score::mw::log::LogFatal("lola") << "More or less then one deployment for " << service_identifier.ToString()
                                         << ". Multi-Binding support right now not supported";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H
