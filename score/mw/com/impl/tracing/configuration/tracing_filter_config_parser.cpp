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
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_parser.h"

#include "score/mw/com/impl/tracing/trace_error.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <score/overload.hpp>

#include <exception>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl::tracing
{
namespace
{
using std::string_view_literals::operator""sv;

constexpr auto kServicesKey = "services"sv;
constexpr auto kShortnamePathKey = "shortname_path"sv;
constexpr auto kEventsKey = "events"sv;
constexpr auto kFieldsKey = "fields"sv;
constexpr auto kMethodsKey = "methods"sv;
constexpr auto kShortnameKey = "shortname"sv;
constexpr auto kNotifierKey = "notifier"sv;
constexpr auto kGetterKey = "getter"sv;
constexpr auto kSetterKey = "setter"sv;

/// \brief List of json property names from the tracing filter config json file which are not currently implemented.
constexpr std::array<const std::string_view, 2U> service_element_notifier_filter_properties_not_implemented_array{
    {"trace_subscribe_received", "trace_unsubscribe_received"}};

/// \brief mapping of json property names from the tracing filter config json file to the corresponding
/// ProxyEventTracePointType enum.
constexpr std::array<std::pair<const std::string_view, ProxyEventTracePointType>, 11U>
    filter_property_proxy_event_mappings{{
        std::make_pair("trace_subscribe_send", ProxyEventTracePointType::SUBSCRIBE),
        std::make_pair("trace_unsubscribe_send", ProxyEventTracePointType::UNSUBSCRIBE),
        std::make_pair("trace_subscription_state_changed", ProxyEventTracePointType::SUBSCRIBE_STATE_CHANGE),
        std::make_pair("trace_subscription_state_change_handler_registered",
                       ProxyEventTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER),
        std::make_pair("trace_subscription_state_change_handler_deregistered",
                       ProxyEventTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER),
        std::make_pair("trace_subscription_state_change_handler_callback",
                       ProxyEventTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK),
        std::make_pair("trace_get_new_samples", ProxyEventTracePointType::GET_NEW_SAMPLES),
        std::make_pair("trace_get_new_samples_callback", ProxyEventTracePointType::GET_NEW_SAMPLES_CALLBACK),
        std::make_pair("trace_receive_handler_registered", ProxyEventTracePointType::SET_RECEIVE_HANDLER),
        std::make_pair("trace_receive_handler_deregistered", ProxyEventTracePointType::UNSET_RECEIVE_HANDLER),
        std::make_pair("trace_receive_handler_callback", ProxyEventTracePointType::RECEIVE_HANDLER_CALLBACK),
    }};

/// \brief mapping of json property names from the tracing filter config json file to the corresponding
/// SkeletonEventTracePointType enum.
constexpr std::array<std::pair<const std::string_view, SkeletonEventTracePointType>, 2U>
    filter_property_skeleton_event_mappings{{
        std::make_pair("trace_send_allocate", SkeletonEventTracePointType::SEND_WITH_ALLOCATE),
        std::make_pair("trace_send", SkeletonEventTracePointType::SEND),
    }};

/// \brief mapping of json property names from the tracing filter config json file to the corresponding
/// ProxyFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, ProxyFieldTracePointType>, 11U>
    filter_property_proxy_field_notifier_mappings{{
        std::make_pair("trace_subscribe_send", ProxyFieldTracePointType::SUBSCRIBE),
        std::make_pair("trace_unsubscribe_send", ProxyFieldTracePointType::UNSUBSCRIBE),
        std::make_pair("trace_subscription_state_changed", ProxyFieldTracePointType::SUBSCRIBE_STATE_CHANGE),
        std::make_pair("trace_subscription_state_change_handler_registered",
                       ProxyFieldTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER),
        std::make_pair("trace_subscription_state_change_handler_deregistered",
                       ProxyFieldTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER),
        std::make_pair("trace_subscription_state_change_handler_callback",
                       ProxyFieldTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK),
        std::make_pair("trace_get_new_samples", ProxyFieldTracePointType::GET_NEW_SAMPLES),
        std::make_pair("trace_get_new_samples_callback", ProxyFieldTracePointType::GET_NEW_SAMPLES_CALLBACK),
        std::make_pair("trace_receive_handler_registered", ProxyFieldTracePointType::SET_RECEIVE_HANDLER),
        std::make_pair("trace_receive_handler_deregistered", ProxyFieldTracePointType::UNSET_RECEIVE_HANDLER),
        std::make_pair("trace_receive_handler_callback", ProxyFieldTracePointType::RECEIVE_HANDLER_CALLBACK),
    }};

/// \brief mapping of json property names of getter object from the tracing filter config json file to the corresponding
/// ProxyFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, ProxyFieldTracePointType>, 2U>
    filter_property_proxy_field_getter_mappings{{
        std::make_pair("trace_request_send", ProxyFieldTracePointType::GET),
        std::make_pair("trace_response_received", ProxyFieldTracePointType::GET_RESULT),
    }};

/// \brief mapping of json property names of setter object from the tracing filter config json file to the corresponding
/// ProxyFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, ProxyFieldTracePointType>, 2U>
    filter_property_proxy_field_setter_mappings{{
        std::make_pair("trace_request_send", ProxyFieldTracePointType::SET),
        std::make_pair("trace_response_received", ProxyFieldTracePointType::SET_RESULT),
    }};

/// \brief mapping of json property names of notifier object from the tracing filter config json file to the
/// corresponding SkeletonFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, SkeletonFieldTracePointType>, 2U>
    filter_property_skeleton_field_notifier_mappings{{
        // we duplicate "trace_update" currently to also add a trace point for lola specific UPDATE_WITH_ALLOCATE
        // @todo: define a schema extension for "update_with allocate".
        std::make_pair("trace_update", SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE),
        std::make_pair("trace_update", SkeletonFieldTracePointType::UPDATE),
    }};

/// \brief mapping of json property names of getter object from the tracing filter config json file to the corresponding
/// SkeletonFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, SkeletonFieldTracePointType>, 2U>
    filter_property_skeleton_field_getter_mappings{{
        std::make_pair("trace_request_received", SkeletonFieldTracePointType::GET_CALL),
        std::make_pair("trace_response_send", SkeletonFieldTracePointType::GET_CALL_RESULT),
    }};

/// \brief mapping of json property names of setter object from the tracing filter config json file to the corresponding
/// SkeletonFieldTracePointType enum.
constexpr std::array<std::pair<const std::string_view, SkeletonFieldTracePointType>, 2U>
    filter_property_skeleton_field_setter_mappings{{
        std::make_pair("trace_request_received", SkeletonFieldTracePointType::SET_CALL),
        std::make_pair("trace_response_send", SkeletonFieldTracePointType::SET_CALL_RESULT),
    }};

/// \brief Checks the optional bool property with the given name in the given json object. If it doesn't exist, it
///        returns false, otherwise it returns the bool value it finds.
/// \param json json object to check for bool prop.
/// \param bool_property_name name of bool prop
/// \return value of property or false if it doesn't exist.
bool IsOptionalBoolPropertyEnabled(const score::json::Object& json, const std::string_view bool_property_name)
{
    const auto& bool_prop_object = json.find(bool_property_name);
    if (bool_prop_object != json.cend())
    {
        return bool_prop_object->second.As<bool>().value();
    }
    return false;
}

/// \brief Returns the configured instances (within our mw_com_config.json) of the given service type
/// \param configuration configuration, where to do the lookup
/// \param service_type identification of the service type (which is an AUTOSAR short-name-path representation)
/// \return set of string_views reflecting an InstanceSpecifier. Those string_views reference into strings held by
///         our single/global Configuration object. Their lifetime is the same as the LoLa runtime!
std::set<std::string_view> GetInstancesOfServiceType(const Configuration& configuration, std::string_view service_type)
{
    std::set<std::string_view> result{};
    for (const auto& service_instance_element : configuration.GetServiceInstances())
    {
        if (service_instance_element.second.service_.ToString() == service_type)
        {
            const auto element_string_view = service_instance_element.first.ToString();
            score::cpp::ignore = result.insert(element_string_view);
        }
    }
    return result;
}

/// \brief Returns a set of element names, used within the given service_type. The names in the set are string_views
///        pointing to strings owned by members of Configuration. So the life-time of those string-views is bound to
///        the life-time of our single/global Configuration object held within the Runtime.
/// \param service_type service type from which to get element names
/// \param element_type element type of which to get names
/// \param configuration configuration object to get consulted for the search/lookup
/// \return a set of string_views denoting the service element names.
std::set<std::string_view> GetElementNamesOfServiceType(const std::string_view service_type,
                                                        ServiceElementType element_type,
                                                        const Configuration& configuration)
{
    std::set<std::string_view> result{};
    auto service_type_deployment_visitor = score::cpp::overload(
        [&result, element_type](const LolaServiceTypeDeployment& lola_service_deployment) {
            if (element_type == ServiceElementType::EVENT)
            {
                for (const auto& event : lola_service_deployment.events_)
                {
                    score::cpp::ignore = result.insert(event.first);
                }
            }
            // LCOV_EXCL_BR_START (Defensive programming: GetElementNamesOfServiceType is always called with either
            // ServiceElementType::EVENT or ServiceElementType::FIELD. Entering the false branch of this check is
            // therefore unreachable.
            else if (element_type == ServiceElementType::FIELD)
            // LCOV_EXCL_BR_STOP
            {

                for (const auto& field : lola_service_deployment.fields_)
                {
                    score::cpp::ignore = result.insert(field.first);
                }
            }
            // LCOV_EXCL_START (Defensive programming: See comment directly above. This branch is only included to
            // protect us from future programming mistakes)
            else
            {
                score::mw::log::LogFatal("lola")
                    << "GetElementNamesOfServiceType called with unsupported ServiceElementType: " << element_type;
                std::terminate();
            }
            // LCOV_EXCL_STOP
        },
        // LCOV_EXCL_START (Unreachable Code: GetElementNamesOfServiceType can only be called on an existing service
        // type. I.e. The ServiceTypeDeployment must be LolaServiceTypeDeployment and can never be score::cpp::blank.
        // This code is there because std::visitor must handle all std::variant types.
        [](const score::cpp::blank&) noexcept {
            return;
        }
        // LCOV_EXCL_STOP
    );

    for (const auto& service_type_deployment : configuration.GetServiceTypes())
    {
        const ServiceIdentifierTypeView current_service_type_view{service_type_deployment.first};
        if (current_service_type_view.getInternalTypeName() == service_type)
        {
            std::visit(service_type_deployment_visitor, service_type_deployment.second.binding_info_);
        }
    }
    return result;
}

///
/// \tparam TP Trace Point Type, one of ProxyEventTracePointType/ProxyFieldTracePointType or
///            SkeletonEventTracePointType/SkeletonFieldTracePointType
/// \param json json object containing the bool property
/// \param bool_prop_name name of the bool property in the json to be checked
/// \param service_type service type identified by its short-name-path for which the trace point shall be added
/// \param service_element_name name of the service within service_type, for which the trace point shall be added.
/// \param instance_id instance id of service-type for which the trace point shall be added.
/// \param trace_point_type detailed trace point type
/// \param filter_config filter_config config, where the addition shall be done.
template <typename TP>
void AddTracePoint(const score::json::Object& json,
                   const std::string_view bool_prop_name,
                   std::string_view service_type,
                   std::string_view service_element_name,
                   ITracingFilterConfig::InstanceSpecifierView instance_id,
                   TP trace_point_type,
                   TracingFilterConfig& filter_config)
{
    if (IsOptionalBoolPropertyEnabled(json, bool_prop_name))
    {
        filter_config.AddTracePoint(service_type, service_element_name, instance_id, trace_point_type);
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseEvent(const score::json::Any& json,
                std::string_view service_type,
                const std::set<std::string_view>& event_names,
                const Configuration& configuration,
                const std::set<std::string_view>& instance_specifiers,
                TracingFilterConfig& filter_config) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Event provided. Required argument.";;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& shortname = object.find(kShortnameKey);
    if (shortname == object.cend())
    {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: 'shortname' property is missing for an event in service '" << service_type
            << "'. Please ensure all events have a 'shortname' property defined. Skipping this event.";
        return;
    }

    auto event_name_result = shortname->second.As<std::string>();
    if (!event_name_result.has_value()) {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: event 'shortname' property must be a valid string. "
            << "Please check your configuration file and ensure the shortname is properly quoted.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& event_name = event_name_result.value().get();
    // check if event exists at all on our side. If not silently ignore it according to:
    // [SCR-18159328] Trace Filter Config reference to non-existing trace-point
    if (event_names.count(event_name) == 0U)
    {
        return;
    }

    const tracing::ServiceElementIdentifierView service_element_identifier{
        service_type, event_name, ServiceElementType::EVENT};

    for (const auto& instance : instance_specifiers)
    {
        if (configuration.GetTracingConfiguration().IsServiceElementTracingEnabled(service_element_identifier,
                                                                                   instance))
        {
            // trace points for the proxy side
            for (const auto& [bool_prop_name, trace_point_type] : filter_property_proxy_event_mappings)
            {
                AddTracePoint(
                    object, bool_prop_name, service_type, event_name, instance, trace_point_type, filter_config);
            }
            // trace points for the skeleton side
            for (const auto& [bool_prop_name, trace_point_type] : filter_property_skeleton_event_mappings)
            {
                AddTracePoint(
                    object, bool_prop_name, service_type, event_name, instance, trace_point_type, filter_config);
            }
            // trace points that are not currently implemented. To be removed in Ticket-126558
            for (const auto& not_implemented_property_name :
                 service_element_notifier_filter_properties_not_implemented_array)
            {
                if (IsOptionalBoolPropertyEnabled(object, not_implemented_property_name))
                {
                    ::score::mw::log::LogWarn("lola")
                        << "Event Tracing point:" << not_implemented_property_name
                        << "is currently unsupported. Will be added in Ticket-126558. Disabling trace point.";
                }
            }
        }
        else
        {
            ::score::mw::log::LogWarn("lola")
                << "Tracing for " << service_element_identifier << " with instance " << instance
                << " has been disabled in mw_com_config but is present in trace filter config file!";
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseEvents(const score::json::Any& json,
                 std::string_view service_short_name_path,
                 const Configuration& configuration,
                 const std::set<std::string_view>& instance_specifiers,
                 TracingFilterConfig& filter_config) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Events provided. Required argument.";;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& events = object.find(kEventsKey);
    if (events == object.cend())
    {
        // service with no events is fine/ok.
        return;
    }
    else
    {
        const std::set<std::string_view>& event_names =
            GetElementNamesOfServiceType(service_short_name_path, ServiceElementType::EVENT, configuration);

        auto events_list = events->second.As<score::json::List>();
        if (!events_list.has_value()) {
            score::mw::log::LogFatal("lola") << "List of Events empty.";;
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        for (const auto& event : events_list.value().get())
        {
            ParseEvent(event, service_short_name_path, event_names, configuration, instance_specifiers, filter_config);
        }
    }
}

/// \brief In case of fields, the bool props for the various trace-points aren't flat under the field-object, but spread
///        in sub-objects. This helper cares for this case to remove code duplication.
/// \tparam Mapping property-name-to-trace-point-type mapping arrays
/// \param json_object enclosing json object (currently a field-object)
/// \param sub_object_name name of sub-object to be parsed
/// \param service_type service type in which context adding takes place
/// \param service_element_name name of service element (in this case field name)
/// \param instance_id instance
/// \param property_name_trace_point_mappings json-property-name-to-trace-point-type mapping
/// \param filter_config filter config, where the trace-points shall be added.
template <typename Mapping>
void AddTracePointsFromSubObject(const score::json::Object& json_object,
                                 const std::string_view sub_object_name,
                                 std::string_view service_type,
                                 std::string_view service_element_name,
                                 ITracingFilterConfig::InstanceSpecifierView instance_id,
                                 Mapping& property_name_trace_point_mappings,
                                 TracingFilterConfig& filter_config)
{
    const auto& block = json_object.find(sub_object_name);
    if (block != json_object.cend())
    {
        auto block_result = block->second.As<score::json::Object>();
        if (!block_result.has_value()) {
            score::mw::log::LogFatal("lola") << "Attribute" << sub_object_name << "not parsable. Required argument.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        const auto& block_object = block_result.value().get();
        // trace points for the proxy side
        for (const auto& prop_mapping : property_name_trace_point_mappings)
        {
            AddTracePoint(block_object,
                          prop_mapping.first,
                          service_type,
                          service_element_name,
                          instance_id,
                          prop_mapping.second,
                          filter_config);
        }
    }
}

/// \todo Helper function that can be removed when support for these tracing points is added in Ticket-126558
void WarnNotImplementedTracePointsFromSubObject(const score::json::Object& json_object,
                                                const std::string_view sub_object_name)
{
    const auto& block = json_object.find(sub_object_name);
    if (block != json_object.cend())
    {
        auto block_result = block->second.As<score::json::Object>();
        if (!block_result.has_value()) {
            score::mw::log::LogFatal("lola") << "Attribute" << sub_object_name << "not parsable. Required argument.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        const auto& block_object = block_result.value().get();
        for (const auto& not_implemented_property_name : service_element_notifier_filter_properties_not_implemented_array)
        {
            if (IsOptionalBoolPropertyEnabled(block_object, not_implemented_property_name))
            {
                ::score::mw::log::LogWarn("lola")
                    << "Field Tracing point:" << not_implemented_property_name
                    << "is currently unsupported. Will be added in Ticket-126558. Disabling trace point.";
            }
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseField(const score::json::Any& json,
                std::string_view service_type,
                const std::set<std::string_view>& field_names,
                const Configuration& configuration,
                const std::set<std::string_view>& instance_specifiers,
                TracingFilterConfig& filter_config) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Field provided. Required argument.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& shortname = object.find(kShortnameKey);
    if (shortname == object.cend())
    {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: 'shortname' property is missing for a field in service '" << service_type
            << "'. Please ensure all fields have a 'shortname' property defined. Skipping this field.";
        return;
    }

    auto field_name_result = shortname->second.As<std::string>();
    if (!field_name_result.has_value()) {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: field 'shortname' property must be a valid string. "
            << "Please check your configuration file and ensure the shortname is properly quoted.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& field_name = field_name_result.value().get();
    // check if field exists at all on our side. If not silently ignore it according to:
    // [SCR-18159328] Trace Filter Config reference to non existing trace-point
    if (field_names.count(field_name) == 0U)
    {
        return;
    }

    const tracing::ServiceElementIdentifierView service_element_identifier{
        service_type, field_name, ServiceElementType::FIELD};

    // check, whether service element exists locally

    for (const auto& instance : instance_specifiers)
    {
        if (configuration.GetTracingConfiguration().IsServiceElementTracingEnabled(service_element_identifier,
                                                                                   instance))
        {
            AddTracePointsFromSubObject(object,
                                        kNotifierKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_proxy_field_notifier_mappings,
                                        filter_config);
            AddTracePointsFromSubObject(object,
                                        kNotifierKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_skeleton_field_notifier_mappings,
                                        filter_config);
            AddTracePointsFromSubObject(object,
                                        kGetterKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_proxy_field_getter_mappings,
                                        filter_config);
            AddTracePointsFromSubObject(object,
                                        kGetterKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_skeleton_field_getter_mappings,
                                        filter_config);
            AddTracePointsFromSubObject(object,
                                        kSetterKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_proxy_field_setter_mappings,
                                        filter_config);
            AddTracePointsFromSubObject(object,
                                        kSetterKey,
                                        service_type,
                                        field_name,
                                        instance,
                                        filter_property_skeleton_field_setter_mappings,
                                        filter_config);
            WarnNotImplementedTracePointsFromSubObject(object, kNotifierKey);
        }
        else
        {
            ::score::mw::log::LogWarn("lola")
                << "Tracing for " << service_element_identifier << " with instance " << instance
                << " has been disabled in mw_com_config but is present in trace filter config file!";
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseFields(const score::json::Any& json,
                 std::string_view service_short_name_path,
                 const Configuration& configuration,
                 const std::set<std::string_view>& instance_specifiers,
                 TracingFilterConfig& filter_config) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Fields provided. Required argument.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& fields = object.find(kFieldsKey);
    if (fields == object.cend())
    {
        // service with no fields is fine/ok.
        return;
    }
    else
    {
        const std::set<std::string_view>& field_names =
            GetElementNamesOfServiceType(service_short_name_path, ServiceElementType::FIELD, configuration);

        auto fields_list = fields->second.As<score::json::List>();
        if (!fields_list.has_value()) {
            score::mw::log::LogFatal("lola") << "Attribute" << kFieldsKey << "not parsable. Required argument.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        for (const auto& field : fields_list.value().get())
        {
            ParseField(field, service_short_name_path, field_names, configuration, instance_specifiers, filter_config);
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseMethods(const score::json::Any& json,
                  std::string_view /*service_short_name_path*/,
                  const Configuration& /*configuration*/,
                  const std::set<std::string_view>& /*instance_specifiers*/,
                  TracingFilterConfig& /*filter_config*/) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Methods provided. Required argument.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& methods = object.find(kMethodsKey);
    if (methods == object.cend())
    {
        // service with no methods is fine/ok.
        return;
    }
    else
    {
        // @todo: Implement ParseMethod, when we support methods in LoLa.
        /* for (const auto& method : methods->second.As<score::json::List>().value().get())
        {

            break;
        }*/
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void ParseService(const score::json::Any& json,
                  const std::set<std::string_view>& configured_service_types,
                  const Configuration& configuration,
                  TracingFilterConfig& filter_config) noexcept
{
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        score::mw::log::LogFatal("lola") << "No Service provided. Required argument.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& shortname_path = object.find(kShortnamePathKey);
    if (shortname_path == object.cend())
    {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: 'shortname_path' property is missing for a service. "
            << "Please ensure all services have a 'shortname_path' property defined.";
        return;
    }

    auto shortname_path_result = shortname_path->second.As<std::string>();
    if (!shortname_path_result.has_value()) {
        score::mw::log::LogError("lola")
            << "Trace Filter Configuration error: service 'shortname_path' property must be a valid string. "
            << "Please check your configuration file and ensure the shortname_path is properly quoted.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& shortname_path_string = shortname_path_result.value().get();
    if (configured_service_types.count(shortname_path_string) > 0U)
    {
        // determine the configured service-instances of the given service-type
        const auto instance_specifiers = GetInstancesOfServiceType(configuration, shortname_path_string);

        ParseEvents(json, shortname_path_string, configuration, instance_specifiers, filter_config);
        ParseFields(json, shortname_path_string, configuration, instance_specifiers, filter_config);
        ParseMethods(json, shortname_path_string, configuration, instance_specifiers, filter_config);
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from
// 'json.As<score::json::Object>().value()' in case the returned result from 'json.As()' doesn't have value but as we do
// check inside 'json.As' using 'std::get_if' and null pointer check before accessing the value, so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::Result<TracingFilterConfig> ParseServices(const score::json::Any& json, const Configuration& configuration) noexcept
{
    TracingFilterConfig tracing_filter_config{};
    auto object_result = json.As<score::json::Object>();
    if (!object_result.has_value()) {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& object = object_result.value().get();
    const auto& services = object.find(kServicesKey);
    if (services == object.cend())
    {
        // even if it is "weird" having a filter-config without any service in it ... it is valid/ok
        return tracing_filter_config;
    }

    // which service types are configured locally in mw::com/LoLa?
    std::set<std::string_view> configured_service_types{};
    for (const auto& map_entry : configuration.GetServiceTypes())
    {
        const auto service_type_string_view = map_entry.first.ToString();
        score::cpp::ignore = configured_service_types.insert(service_type_string_view);
    }

    auto services_list = services->second.As<score::json::List>();
    if (!services_list.has_value()) {
        score::mw::log::LogFatal("lola") << "Attribute" << kServicesKey << "not parsable. Required argument.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    for (const auto& service : services_list.value().get())
    {

        ParseService(service, configured_service_types, configuration, tracing_filter_config);
    }
    return tracing_filter_config;
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'json_result.value()' in case the
// status doesn't have value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access
// which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::Result<TracingFilterConfig> Parse(const std::string_view path, const Configuration& configuration) noexcept
{
    const score::json::JsonParser json_parser_obj{};
    // Reason for banning is AoU of vaJson library about integrity of provided path.
    // This AoU is forwarded as AoU of Lola. See broken_link_c/issue/5835192
    // NOLINTNEXTLINE(score-banned-function): The user has to guarantee the integrity of the path
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola") << "Parsing trace filter config file" << path
                                         << "failed with error:" << json_result.error().Message() << ": "
                                         << json_result.error().UserMessage() << " .";
        return MakeUnexpected(TraceErrorCode::JsonConfigParseError, json_result.error().UserMessage());
    }
    return Parse(std::move(json_result).value(), configuration);
}

score::Result<TracingFilterConfig> Parse(score::json::Any json, const Configuration& configuration) noexcept
{
    return ParseServices(json, configuration);
}

}  // namespace score::mw::com::impl::tracing
