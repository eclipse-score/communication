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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_TPP
#define SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_TPP

#include "score/json/json_parser.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

/// \brief Contains the definitions for all template functions / methods of the templated BindingServiceTypeDeployment
/// to improve readability in binding_service_type_deployment.h
namespace score::mw::com::impl
{

namespace detail
{

constexpr auto kSerializationVersionKey = "serializationVersion";
constexpr auto kServiceIdKey = "serviceId";
constexpr auto kEventsKey = "events";
constexpr auto kFieldsKey = "fields";

template <typename ServiceElementMappingType>
json::Object ConvertServiceElementIdMapToJson(const ServiceElementMappingType& input_map) noexcept
{
    json::Object service_element_instance_mapping_object{};
    for (auto it : input_map)
    {
        const auto insert_result = service_element_instance_mapping_object.insert(std::make_pair(it.first, it.second));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Could not insert element in map");
    }
    return service_element_instance_mapping_object;
}

template <typename ServiceElementIdType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that json.As<T>().value might throw due to std::bad_variant_access. Which will cause an implicit
// terminate.
// The .value() call will only throw if the  As<T> function didn't succeed in parsing the json. In this case termination
// is the intended behaviour.
// ToDo: implement a runtime validation check for json, after parsing when the first json object is created, s.t. we can
// be sure json.As<T> call will return a value. See Ticket-177855.
// coverity[autosar_cpp14_a15_5_3_violation]
auto ConvertJsonToServiceElementIdMap(const json::Object& json_object, const score::cpp::string_view key) noexcept
    -> std::unordered_map<std::string, ServiceElementIdType>
{
    const auto& service_element_json = GetValueFromJson<json::Object>(json_object, key);

    std::unordered_map<std::string, ServiceElementIdType> service_element_map{};
    for (auto& it : service_element_json)
    {
        const score::cpp::string_view event_name_view{it.first.GetAsStringView()};
        const std::string event_name{event_name_view.data(), event_name_view.size()};
        auto event_instance_deployment_json = it.second.As<ServiceElementIdType>().value();
        const auto insert_result =
            service_element_map.insert(std::make_pair(event_name, std::move(event_instance_deployment_json)));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Could not insert element in map");
    }
    return service_element_map;
}

template <typename ServiceIdType>
std::string ToHashStringImpl(const ServiceIdType service_id, const std::size_t hash_string_size) noexcept
{
    /// \todo When we can use C++17 features, use std::to_chars so that we can convert from an int to a hex string
    /// with no dynamic allocations.
    std::stringstream stream;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(hash_string_size <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    // Passing std::hex to std::stringstream object with the stream operator follows the idiomatic way that both
    // features in conjunction were designed in the C++ standard.
    // coverity[autosar_cpp14_m8_4_4_violation] See above
    stream << std::setfill('0') << std::setw(static_cast<int>(hash_string_size)) << std::hex << service_id;
    return stream.str();
}

}  // namespace detail

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>::BindingServiceTypeDeployment(
    const ServiceIdType service_id,
    EventIdMapping events,
    FieldIdMapping fields) noexcept
    : service_id_{service_id},
      events_{std::move(events)},
      fields_{std::move(fields)},
      hash_string_{detail::ToHashStringImpl(service_id_, hashStringSize)}
{
}

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>::BindingServiceTypeDeployment(
    const score::json::Object& json_object) noexcept
    : BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>::BindingServiceTypeDeployment(
          GetValueFromJson<ServiceIdType>(json_object, detail::kServiceIdKey),
          detail::ConvertJsonToServiceElementIdMap<EventIdType>(json_object, detail::kEventsKey),
          detail::ConvertJsonToServiceElementIdMap<FieldIdType>(json_object, detail::kFieldsKey))
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, detail::kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }
}

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
json::Object BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>::Serialize() const noexcept
{
    score::json::Object json_object{};
    json_object[detail::kSerializationVersionKey] = json::Any{serializationVersion};
    json_object[detail::kServiceIdKey] = score::json::Any{service_id_};

    json_object[detail::kEventsKey] = detail::ConvertServiceElementIdMapToJson(events_);
    json_object[detail::kFieldsKey] = detail::ConvertServiceElementIdMapToJson(fields_);

    return json_object;
}

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
std::string_view BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>::ToHashString() const noexcept
{
    return hash_string_;
}

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
bool operator==(const BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>& lhs,
                const BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>& rhs) noexcept
{
    return ((lhs.service_id_ == rhs.service_id_) && (lhs.events_ == rhs.events_) && (lhs.fields_ == rhs.fields_));
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_TPP
