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
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"

#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/configuration/hash_helper_utility.h"
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include "score/mw/log/logging.h"

#include <exception>
#include <limits>
#include <unordered_set>

namespace score::mw::com::impl::tracing
{

namespace detail_tracing_filter_config
{

bool CompareStringWithStringView::operator()(const score::cpp::string_view lhs_view,
                                             const std::string& rhs_string) const noexcept
{
    return lhs_view < score::cpp::string_view{rhs_string.data(), rhs_string.size()};
}

bool CompareStringWithStringView::operator()(const std::string& lhs_string,
                                             const score::cpp::string_view rhs_view) const noexcept
{
    return score::cpp::string_view{lhs_string.data(), lhs_string.size()} < rhs_view;
}

}  // namespace detail_tracing_filter_config

namespace
{

using CompareStringWithStringView = detail_tracing_filter_config::CompareStringWithStringView;
using InstanceSpecifierView = ITracingFilterConfig::InstanceSpecifierView;

const score::cpp::string_view GetOrInsertStringInSet(const score::cpp::string_view key,
                                              std::set<std::string, CompareStringWithStringView>& search_set) noexcept
{
    const auto find_result = search_set.find(key);
    const bool does_string_already_exist{find_result != search_set.end()};
    if (does_string_already_exist)
    {
        return find_result->data();
    }

    auto insert_result = search_set.emplace(key.data(), key.size());
    // emplace in a std::unordered_set can only return false if the key already exists. We check this condition above
    // with find() so this check can never fail.
    // LCOV_EXCL_LINE (defensive programming. In current implementation, the assertion can never become wrong.)
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(insert_result.second);
    return insert_result.first->data();
}

void InsertTracePointIntoMap(
    const TracePointKey& trace_point_key,
    InstanceSpecifierView instance_specifier,
    std::unordered_map<TracePointKey, std::set<InstanceSpecifierView>>& trace_point_map) noexcept
{
    auto map_it = trace_point_map.find(trace_point_key);
    if (map_it == trace_point_map.end())
    {
        std::set<InstanceSpecifierView> instance_specifiers{instance_specifier};
        const auto map_insert_result = trace_point_map.insert({trace_point_key, std::move(instance_specifiers)});
        // insert in a std::unordered_map can only return false if the key already exists. We check this condition
        // above with find() so this check can never fail.
        // LCOV_EXCL_LINE (defensive programming. In current implementation, the assertion can never become wrong.)
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(map_insert_result.second);
    }
    else
    {
        score::cpp::ignore = map_it->second.insert(instance_specifier);
    }
}

template <typename TracePointType>
void AddTracePointToMap(score::cpp::string_view service_type,
                        score::cpp::string_view service_element_name,
                        ServiceElementType service_element_type,
                        InstanceSpecifierView instance_specifier,
                        TracePointType trace_point_type,
                        std::unordered_map<TracePointKey, std::set<InstanceSpecifierView>>& trace_point_map,
                        std::set<std::string, CompareStringWithStringView>& config_names) noexcept
{
    if (trace_point_type == TracePointType::INVALID)
    {
        score::mw::log::LogFatal("lola") << "Invalid TracePointType: " << static_cast<int>(trace_point_type);
        std::terminate();
    }
    auto service_type_stored = GetOrInsertStringInSet(service_type, config_names);
    auto service_element_name_stored = GetOrInsertStringInSet(service_element_name, config_names);
    const ServiceElementIdentifierView service_element_identifer{
        service_type_stored, service_element_name_stored, service_element_type};

    auto trace_point_type_int = static_cast<std::uint8_t>(trace_point_type);
    const TracePointKey trace_point_key{service_element_identifer, trace_point_type_int};

    InsertTracePointIntoMap(trace_point_key, instance_specifier, trace_point_map);
}

template <typename TracePointType>
bool IsTracePointEnabledInMap(
    score::cpp::string_view service_type,
    score::cpp::string_view service_element_name,
    ServiceElementType service_element_type,
    InstanceSpecifierView instance_specifier,
    TracePointType trace_point_type,
    const std::unordered_map<TracePointKey, std::set<InstanceSpecifierView>>& trace_point_map) noexcept
{
    const ServiceElementIdentifierView service_element_identifer{
        service_type, service_element_name, service_element_type};
    auto trace_point_type_int = static_cast<std::uint8_t>(trace_point_type);
    const TracePointKey trace_point_key{service_element_identifer, trace_point_type_int};

    auto map_it = trace_point_map.find(trace_point_key);
    if (map_it == trace_point_map.end())
    {
        return false;
    }
    const auto instance_specifiers = map_it->second;
    const bool instance_specifier_in_vector{instance_specifiers.count(instance_specifier) != 0U};
    return instance_specifier_in_vector;
}

template <typename T>
constexpr bool DoesTracePointNeedTraceDoneCB([[maybe_unused]] const T& trace_point_type) noexcept
{
    // Suppress AUTOSAR C++14 M6-4-1 rule finding. This rule states: "An if ( condition ) construct shall be followed
    // by a compound statement. The else keyword shall be followed by either a compound statement, or another if
    // statement.".
    // Suppress "AUTOSAR C++14 A7-1-8" rule finding. This rule states: "A class, structure, or enumeration shall not
    // be declared in the definition of its type.".
    // Rationale: This is a false positive because "if constexpr" is a valid statement since C++17.
    // coverity[autosar_cpp14_m6_4_1_violation]
    // coverity[autosar_cpp14_a7_1_8_violation]
    if constexpr (std::is_same_v<T, SkeletonEventTracePointType>)
    {
        return ((trace_point_type == SkeletonEventTracePointType::SEND) ||
                (trace_point_type == SkeletonEventTracePointType::SEND_WITH_ALLOCATE));
    }
    // coverity[autosar_cpp14_m6_4_1_violation]
    // coverity[autosar_cpp14_a7_1_8_violation]
    else if constexpr (std::is_same_v<T, SkeletonFieldTracePointType>)
    {
        return ((trace_point_type == SkeletonFieldTracePointType::UPDATE) ||
                (trace_point_type == SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE));
    }
    // coverity[autosar_cpp14_m6_4_1_violation]
    // coverity[autosar_cpp14_a7_1_8_violation]
    else if constexpr (std::is_same_v<T, ProxyEventTracePointType> || std::is_same_v<T, ProxyFieldTracePointType>)
    {
        score::cpp::ignore = trace_point_type;
        return false;
    }
    else
    {
        static_assert(std::is_same_v<T, void>, "Unsupported trace point type");
        return false;  // This line will never be reached
    }
}

}  // namespace

namespace
{
/// @brief: Determine for all trace points, if they can be traced. Accumulate the number of configured tracing slots,
/// for all traceable trace points.
template <typename TracePointType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". This is a false positive, std::terminate() is implicitly called from '.value()' in case the
// json_result doesn't have a value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::size_t FindNumberOfTracingSlots(
    const std::unordered_map<TracePointKey, std::set<InstanceSpecifierView>>& trace_point_map,
    std::unordered_set<ServiceElementIdentifierView>& service_element_identifier_view_set,
    const score::mw::com::impl::Configuration& configuration,
    ServiceElementType service_element_type) noexcept
{
    std::size_t number_of_needed_traceing_slots{0U};
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Both variables are used in the same function.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    for (const auto& [trace_point_key, instance_specifier_view_set] : trace_point_map)
    {
        // Suppress "AUTOSAR C++14 A7-2-1" rule finding: "An expression with enum underlying type shall only have values
        // corresponding to the enumerators of the enumeration".
        // Here we cast to std::variant of different enums, So we can't set the underlying type to a specific enum,
        // also inside DoesTracePointNeedTraceDoneCB a comparison against the enumerators takes place.
        // coverity[autosar_cpp14_a7_2_1_violation]
        const auto trace_point_type = static_cast<TracePointType>(trace_point_key.trace_point_type);
        if (!DoesTracePointNeedTraceDoneCB(trace_point_type))
        {
            continue;
        }
        const auto service_element = trace_point_key.service_element;
        if (service_element_identifier_view_set.count(service_element) != 0U)
        {
            continue;
        }
        score::cpp::ignore = service_element_identifier_view_set.insert(service_element);

        for (const auto& instance_specifier : instance_specifier_view_set)
        {
            const auto instance_specifier_result = InstanceSpecifier::Create(instance_specifier);
            if (!instance_specifier_result.has_value())
            {
                score::mw::log::LogFatal() << "Lola: Could not create instance specifier. Terminating.";
                std::terminate();
            }

            const auto& service = configuration.GetServiceInstances();

            const auto service_instance_it = service.find(instance_specifier_result.value());
            if (service_instance_it == service.end())
            {
                score::mw::log::LogFatal() << "Lola: provided service instance with name:"
                                         << instance_specifier_result.value() << " could not be found.";
                std::terminate();
            }

            const auto* lola_service_instance_deployment =
                std::get_if<LolaServiceInstanceDeployment>(&service_instance_it->second.bindingInfo_);
            if (lola_service_instance_deployment == nullptr)
            {
                score::mw::log::LogFatal("lola") << "FindNumberOfTracingSlots: Wrong Binding! ServiceInstanceDeployment "
                                                  "doesn't contain a LoLa deployment!";
                std::terminate();
            }

            const auto& service_instance_map = [service_element_type, &lola_service_instance_deployment]() {
                if (service_element_type == ServiceElementType::EVENT)
                {
                    return lola_service_instance_deployment->events_;
                }
                if (service_element_type == ServiceElementType::FIELD)
                {
                    return lola_service_instance_deployment->fields_;
                }
                // LCOV_EXCL_START Defensive programming.
                // This function is only used internally and only ever called with EVENT or FIELD ServiceElementType,
                // thus the following lines can never be reached.
                score::mw::log::LogFatal() << "Lola: invalid service element (" << service_element_type << ") provided.";
                std::terminate();
                // LCOV_EXCL_STOP
            }();

            const auto service_element_name = service_element.service_element_name.data();
            const auto service_element_name_it = service_instance_map.find(service_element_name);

            if (service_element_name_it == service_instance_map.end())
            {
                score::mw::log::LogFatal("lola")
                    << "Requested service element (" << service_element << ") does not exist.";
                std::terminate();
            }
            const auto slots_per_tracing_point = service_element_name_it->second.GetNumberOfTracingSlots();

            number_of_needed_traceing_slots += slots_per_tracing_point;
        }
    }
    return number_of_needed_traceing_slots;
}
}  // namespace

bool TracingFilterConfig::IsTracePointEnabled(
    score::cpp::string_view service_type,
    score::cpp::string_view event_name,
    InstanceSpecifierView instance_specifier,
    SkeletonEventTracePointType skeleton_event_trace_point_type) const noexcept
{
    return IsTracePointEnabledInMap(service_type,
                                    event_name,
                                    ServiceElementType::EVENT,
                                    instance_specifier,
                                    skeleton_event_trace_point_type,
                                    skeleton_event_trace_points_);
}

bool TracingFilterConfig::IsTracePointEnabled(
    score::cpp::string_view service_type,
    score::cpp::string_view field_name,
    InstanceSpecifierView instance_specifier,
    SkeletonFieldTracePointType skeleton_field_trace_point_type) const noexcept
{
    return IsTracePointEnabledInMap(service_type,
                                    field_name,
                                    ServiceElementType::FIELD,
                                    instance_specifier,
                                    skeleton_field_trace_point_type,
                                    skeleton_field_trace_points_);
}

bool TracingFilterConfig::IsTracePointEnabled(score::cpp::string_view service_type,
                                              score::cpp::string_view event_name,
                                              InstanceSpecifierView instance_specifier,
                                              ProxyEventTracePointType proxy_event_trace_point_type) const noexcept
{
    return IsTracePointEnabledInMap(service_type,
                                    event_name,
                                    ServiceElementType::EVENT,
                                    instance_specifier,
                                    proxy_event_trace_point_type,
                                    proxy_event_trace_points_);
}

bool TracingFilterConfig::IsTracePointEnabled(score::cpp::string_view service_type,
                                              score::cpp::string_view field_name,
                                              InstanceSpecifierView instance_specifier,
                                              ProxyFieldTracePointType proxy_field_trace_point_type) const noexcept
{
    return IsTracePointEnabledInMap(service_type,
                                    field_name,
                                    ServiceElementType::FIELD,
                                    instance_specifier,
                                    proxy_field_trace_point_type,
                                    proxy_field_trace_points_);
}

void TracingFilterConfig::AddTracePoint(score::cpp::string_view service_type,
                                        score::cpp::string_view event_name,
                                        InstanceSpecifierView instance_specifier,
                                        SkeletonEventTracePointType skeleton_event_trace_point_type) noexcept
{
    AddTracePointToMap(service_type,
                       event_name,
                       ServiceElementType::EVENT,
                       instance_specifier,
                       skeleton_event_trace_point_type,
                       skeleton_event_trace_points_,
                       config_names_);
}

void TracingFilterConfig::AddTracePoint(score::cpp::string_view service_type,
                                        score::cpp::string_view field_name,
                                        InstanceSpecifierView instance_specifier,
                                        SkeletonFieldTracePointType skeleton_field_trace_point_type) noexcept
{
    AddTracePointToMap(service_type,
                       field_name,
                       ServiceElementType::FIELD,
                       instance_specifier,
                       skeleton_field_trace_point_type,
                       skeleton_field_trace_points_,
                       config_names_);
}

void TracingFilterConfig::AddTracePoint(score::cpp::string_view service_type,
                                        score::cpp::string_view event_name,
                                        InstanceSpecifierView instance_specifier,
                                        ProxyEventTracePointType proxy_event_trace_point_type) noexcept
{
    AddTracePointToMap(service_type,
                       event_name,
                       ServiceElementType::EVENT,
                       instance_specifier,
                       proxy_event_trace_point_type,
                       proxy_event_trace_points_,
                       config_names_);
}

void TracingFilterConfig::AddTracePoint(score::cpp::string_view service_type,
                                        score::cpp::string_view field_name,
                                        InstanceSpecifierView instance_specifier,
                                        ProxyFieldTracePointType proxy_field_trace_point_type) noexcept
{
    AddTracePointToMap(service_type,
                       field_name,
                       ServiceElementType::FIELD,
                       instance_specifier,
                       proxy_field_trace_point_type,
                       proxy_field_trace_points_,
                       config_names_);
}

/// @brief: Find the number of configured tracing slots for all trace points.
std::uint16_t TracingFilterConfig::GetNumberOfTracingSlots(score::mw::com::impl::Configuration& config) const noexcept
{
    std::unordered_set<ServiceElementIdentifierView> service_element_identifier_view_set{};
    const std::array<std::size_t, 4U> number_trace_points_list{
        {FindNumberOfTracingSlots<SkeletonEventTracePointType>(
             skeleton_event_trace_points_, service_element_identifier_view_set, config, ServiceElementType::EVENT),
         FindNumberOfTracingSlots<SkeletonFieldTracePointType>(
             skeleton_field_trace_points_, service_element_identifier_view_set, config, ServiceElementType::FIELD),
         FindNumberOfTracingSlots<ProxyEventTracePointType>(
             proxy_event_trace_points_, service_element_identifier_view_set, config, ServiceElementType::EVENT),
         FindNumberOfTracingSlots<ProxyFieldTracePointType>(
             proxy_field_trace_points_, service_element_identifier_view_set, config, ServiceElementType::FIELD)}};

    const auto [number_trace_points, data_overflow_error] = score::mw::com::impl::tracing::configuration::Accumulate(
        number_trace_points_list.begin(),
        number_trace_points_list.end(),
        // std::size_t{0U} triggers false positive A5-2-2 violation, see Ticket-161711
        std::size_t{});
    if (data_overflow_error || (number_trace_points > std::numeric_limits<std::uint16_t>::max()))
    {
        score::mw::log::LogFatal("lola") << "Invalid Trace points: the sum exceeds std::uint16_t max";
        std::terminate();
    }
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding.
    // This rule states: "An integer expression shall not lead to loss.".
    // The check above ensures that number_trace_points does not exceed its maximum value for uint16_t type.
    // coverity[autosar_cpp14_a4_7_1_violation]
    return static_cast<std::uint16_t>(number_trace_points);
}

}  // namespace score::mw::com::impl::tracing
