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
#include "score/mw/com/impl/configuration/tracing_configuration.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <iostream>
#include <unordered_set>
#include <utility>

namespace score::mw::com::impl
{

namespace detail_tracing_configuration
{

bool CompareServiceElementIdentifierWithView::operator()(const tracing::ServiceElementIdentifier& lhs,
                                                         const tracing::ServiceElementIdentifier& rhs) const noexcept
{
    return lhs < rhs;
}

bool CompareServiceElementIdentifierWithView::operator()(const tracing::ServiceElementIdentifierView lhs_view,
                                                         const tracing::ServiceElementIdentifier& rhs) const noexcept
{
    const tracing::ServiceElementIdentifierView rhs_view{
        score::cpp::string_view{rhs.service_type_name}, score::cpp::string_view{rhs.service_element_name}, rhs.service_element_type};
    return lhs_view < rhs_view;
}

bool CompareServiceElementIdentifierWithView::operator()(
    const tracing::ServiceElementIdentifier& lhs,
    const tracing::ServiceElementIdentifierView rhs_view) const noexcept
{
    const tracing::ServiceElementIdentifierView lhs_view{
        score::cpp::string_view{lhs.service_type_name}, score::cpp::string_view{lhs.service_element_name}, lhs.service_element_type};
    return lhs_view < rhs_view;
}

}  // namespace detail_tracing_configuration

void TracingConfiguration::SetTracingEnabled(const bool tracing_enabled) noexcept
{
    tracing_config_.enabled = tracing_enabled;
}

void TracingConfiguration::SetApplicationInstanceID(std::string application_instance_id) noexcept
{
    tracing_config_.application_instance_id = application_instance_id;
}

void TracingConfiguration::SetTracingTraceFilterConfigPath(std::string trace_filter_config_path) noexcept
{
    tracing_config_.trace_filter_config_path = trace_filter_config_path;
}

void TracingConfiguration::SetServiceElementTracingEnabled(tracing::ServiceElementIdentifier service_element_identifier,
                                                           InstanceSpecifier instance_specifier) noexcept
{
    auto find_result = service_element_tracing_enabled_map_.find(service_element_identifier);
    if (find_result == service_element_tracing_enabled_map_.end())
    {
        const auto insert_result = service_element_tracing_enabled_map_.emplace(
            std::move(service_element_identifier),
            std::unordered_set<InstanceSpecifier>{std::move(instance_specifier)});
        // Defensive programming: Since we checked about that service_element_identifier is already in the map, there's
        // no reason why the emplace should fail.
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(insert_result.second);
    }
    else
    {
        const auto insert_result = find_result->second.insert(std::move(instance_specifier));
        if (!insert_result.second)
        {
            ::score::mw::log::LogFatal("lola")
                << "Could not insert instance specifier into service element tracing enabled map. Terminating";
            std::terminate();
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports InstanceSpecifier::Create().value might throw due to std::bad_variant_access. Which will cause an
// implicit terminate.
// The .value() call will only throw if the the instance specifier can not be created and thus Create returns an error.
// In this case termination is the intended behaviour.
// coverity[autosar_cpp14_a15_5_3_violation]
bool TracingConfiguration::IsServiceElementTracingEnabled(
    tracing::ServiceElementIdentifierView service_element_identifier_view,
    score::cpp::string_view instance_specifier_view) const noexcept
{
    const auto find_result = service_element_tracing_enabled_map_.find(service_element_identifier_view);
    if (find_result == service_element_tracing_enabled_map_.end())
    {
        return false;
    }

    const auto instance_specifier_result = InstanceSpecifier::Create(instance_specifier_view);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier_result.has_value(),
                           "an isntance specifier could not be created from the given instance_specifier_view.");

    const auto instance_specifier = instance_specifier_result.value();
    const auto& instance_specifier_set = find_result->second;
    return instance_specifier_set.count(instance_specifier) != 0U;
}

}  // namespace score::mw::com::impl
