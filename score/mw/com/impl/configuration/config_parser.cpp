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
#include "score/mw/com/impl/configuration/config_parser.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <score/string_view.hpp>

#include <cstdlib>
#include <exception>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl::configuration
{
namespace
{

using std::string_view_literals::operator""sv;

// TM: Repeated key definitions in many other files
// binding_service_type_deployment_impl.h
// config_parser.cpp
// lola_service_instance_deployment.cpp
// someip_service_instance_deployment.cpp
// tracing_filter_config_parser.cpp
constexpr auto kServiceInstancesKey = "serviceInstances"sv;
constexpr auto kInstanceSpecifierKey = "instanceSpecifier"sv;
constexpr auto kServiceTypeNameKey = "serviceTypeName"sv;
constexpr auto kVersionKey = "version"sv;
constexpr auto kMajorVersionKey = "major"sv;
constexpr auto kMinorVersionKey = "minor"sv;
constexpr auto kDeploymentInstancesKey = "instances"sv;
constexpr auto kBindingKey = "binding"sv;
constexpr auto kBindingsKey = "bindings"sv;
constexpr auto kAsilKey = "asil-level"sv;
constexpr auto kServiceIdKey = "serviceId"sv;
constexpr auto kInstanceIdKey = "instanceId"sv;
constexpr auto kServiceTypesKey = "serviceTypes"sv;
constexpr auto kEventsKey = "events"sv;
constexpr auto kEventNameKey = "eventName"sv;
constexpr auto kEventIdKey = "eventId"sv;
constexpr auto kFieldsKey = "fields"sv;
constexpr auto kFieldNameKey = "fieldName"sv;
constexpr auto kFieldIdKey = "fieldId"sv;
constexpr auto kMethodsKey = "methods"sv;
constexpr auto kMethodNameKey = "methodName"sv;
constexpr auto kMethodIdKey = "methodId"sv;
constexpr auto kMethodQueueSizeKey = "queueSize"sv;
constexpr auto kEventNumberOfSampleSlotsKey = "numberOfSampleSlots"sv;
constexpr auto kEventMaxSamplesKey = "maxSamples"sv;
constexpr auto kEventMaxSubscribersKey = "maxSubscribers"sv;
constexpr auto kEventEnforceMaxSamplesKey = "enforceMaxSamples"sv;
constexpr auto kEventMaxConcurrentAllocationsKey = "maxConcurrentAllocations"sv;
constexpr auto kMaxConcurrentAllocationsDefault = static_cast<std::uint8_t>(1U);
constexpr auto kFieldNumberOfSampleSlotsKey = "numberOfSampleSlots"sv;  //TM: duplicate of kEventNumberOfSampleSlotsKey, Line 71
constexpr auto kFieldMaxSubscribersKey = "maxSubscribers"sv;
constexpr auto kFieldEnforceMaxSamplesKey = "enforceMaxSamples"sv;
constexpr auto kFieldMaxConcurrentAllocationsKey = "maxConcurrentAllocations"sv;    //TM: duplicate of kEventMaxConcurrentAllocationsKey, Line 75
constexpr auto kLolaShmSizeKey = "shm-size"sv;
constexpr auto kLolaControlAsilBShmSizeKey = "control-asil-b-shm-size"sv;
constexpr auto kLolaControlQmShmSizeKey = "control-qm-shm-size"sv;
constexpr auto kGlobalPropertiesKey = "global"sv;
constexpr auto kAllowedConsumerKey = "allowedConsumer"sv;
constexpr auto kAllowedProviderKey = "allowedProvider"sv;
constexpr auto kQueueSizeKey = "queue-size"sv;
constexpr auto kShmSizeCalcModeKey = "shm-size-calc-mode"sv;
constexpr auto kTracingPropertiesKey = "tracing"sv;
constexpr auto kTracingEnabledKey = "enable"sv;
constexpr auto kTracingGloballyEnabledDefaultValue = false;
constexpr auto kTracingApplicationInstanceIDKey = "applicationInstanceID"sv;
constexpr auto kApplicationIdKey = "applicationID"sv;
constexpr auto kTracingTraceFilterConfigPathKey = "traceFilterConfigPath"sv;
constexpr auto kNumberOfIpcTracingSlotsKey = "numberOfIpcTracingSlots"sv;
using NumberOfIpcTracingSlots_t = std::uint8_t;
constexpr auto kNumberOfIpcTracingSlotsDefault = static_cast<NumberOfIpcTracingSlots_t>(0U);

constexpr auto kPermissionChecksKey = "permission-checks"sv;

constexpr auto kSomeIpBinding = "SOME/IP"sv;
constexpr auto kShmBinding = "SHM"sv;
constexpr auto kShmSizeCalcModeSimulation = "SIMULATION"sv;

constexpr auto kTracingTraceFilterConfigPathDefaultValue = "./etc/mw_com_trace_filter.json"sv;
constexpr auto kStrictPermission = "strict"sv;
constexpr auto kFilePermissionsOnEmpty = "file-permissions-on-empty"sv;

void AbortIfFound(const score::json::Object::const_iterator& iterator_to_element, const score::json::Object& json_obj)
{

    if (iterator_to_element != json_obj.end())
    {
        mw::log::LogFatal() << "Parsing an element " << (iterator_to_element->first.GetAsStringView())
                            << " which is not currently supported."
                            << " Remove this element from the configuration. Aborting!\n";

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

auto ParseInstanceSpecifier(const score::json::Object& json_map) -> InstanceSpecifier
{
    const auto& instanceSpecifierJson = json_map.find(kInstanceSpecifierKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(instanceSpecifierJson != json_map.cend(),
                                 "Configuration corrupted, check with json schema");

    auto string_result = instanceSpecifierJson->second.As<std::string>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(string_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& string_view = string_result.value().get();
    const auto instance_specifier_result = InstanceSpecifier::Create(string_view);
    if (!instance_specifier_result.has_value())
    {
        score::mw::log::LogFatal("lola") << "Invalid InstanceSpecifier.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return instance_specifier_result.value();
}

auto ParseServiceTypeName(const score::json::Object& json_map) -> const std::string&
{
    const auto& serviceTypeName = json_map.find(kServiceTypeNameKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(serviceTypeName != json_map.cend(), "Configuration corrupted, check with json schema");

    auto string_result = serviceTypeName->second.As<std::string>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(string_result.has_value(), "Configuration corrupted, check with json schema");
    return string_result.value().get();
}

auto ParseVersion(const score::json::Object& json_map) -> std::pair<std::uint32_t, std::uint32_t>
{
    const auto& version = json_map.find(kVersionKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(version != json_map.cend(), "Configuration corrupted, check with json schema");

    auto version_obj = version->second.As<score::json::Object>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(version_obj.has_value(), "Configuration corrupted, check with json schema");
    const auto& version_object = version_obj.value().get();
    const auto major_version_number = version_object.find(kMajorVersionKey.data());
    const auto minor_version_number = version_object.find(kMinorVersionKey.data());

    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(major_version_number != version_object.cend(),
                                 "Configuration corrupted, check with json schema");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(minor_version_number != version_object.cend(),
                                 "Configuration corrupted, check with json schema");

    const auto major_version_number_casted = major_version_number->second.As<std::uint32_t>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(major_version_number_casted.has_value(),
                                 "Configuration corrupted, check with json schema");
    const auto minor_version_number_casted = minor_version_number->second.As<std::uint32_t>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(minor_version_number_casted.has_value(),
                                 "Configuration corrupted, check with json schema");
    return std::pair<std::uint32_t, std::uint32_t>{major_version_number_casted.value(),
                                                   minor_version_number_casted.value()};
}

auto ParseServiceTypeIdentifier(const score::json::Object& json) -> ServiceIdentifierType
{
    const auto& name = ParseServiceTypeName(json);
    const auto& version = ParseVersion(json);

    return make_ServiceIdentifierType(name.data(), version.first, version.second);
}

auto ParseAsilLevel(const score::json::Object& json_map) -> score::cpp::optional<QualityType>
{
    const auto& quality = json_map.find(kAsilKey.data());
    if (quality != json_map.cend())
    {
        auto quality_result = quality->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(quality_result.has_value(), "Configuration corrupted, check with json schema");
        const auto& qualityValue = quality_result.value().get();

        if (qualityValue == "QM")
        {
            return QualityType::kASIL_QM;
        }

        if (qualityValue == "B")
        {
            return QualityType::kASIL_B;
        }

        return QualityType::kInvalid;
    }

    return score::cpp::nullopt;
}

auto ParseShmSizeCalcMode(const score::json::Object& json_map) -> score::cpp::optional<ShmSizeCalculationMode>
{
    const auto& shm_size_calc_mode = json_map.find(kShmSizeCalcModeKey.data());
    if (shm_size_calc_mode != json_map.cend())
    {
        auto mode_result = shm_size_calc_mode->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(mode_result.has_value(), "Configuration corrupted, check with json schema");
        const auto& shm_size_calc_mode_value = mode_result.value().get();

        if (shm_size_calc_mode_value == kShmSizeCalcModeSimulation)
        {
            return ShmSizeCalculationMode::kSimulation;
        }
        else
        {
            score::mw::log::LogError("lola")
                << "Unknown value " << shm_size_calc_mode_value << " in key " << kShmSizeCalcModeKey;
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }

    return score::cpp::nullopt;
}

// Note 1:
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that json.As<T>().value might throw due to std::bad_variant_access. Which will cause an implicit
// terminate.
// The .value() call will only throw if the  As<T> function didn't succeed in parsing the json. In this case termination
// is the intended behaviour.
// ToDo: implement a runtime validation check for json, after parsing when the first json object is created, s.t. we can
// be sure json.As<T> call will return a value. See Ticket-177855.
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseAllowedUser(const score::json::Object& json_map, std::string_view key)
    -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    std::unordered_map<QualityType, std::vector<uid_t>> user_map{};
    auto allowed_user = json_map.find(key.data());

    if (allowed_user != json_map.cend())
    {
        const auto user_obj_result = allowed_user->second.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(user_obj_result.has_value(), "Configuration corrupted, check with json schema");
        const auto& user_obj = user_obj_result.value().get();
        for (const auto& user : user_obj)
        {
            std::vector<uid_t> user_ids{};
            const auto user_list_result = user.second.As<score::json::List>();
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(user_list_result.has_value(),
                                         "Configuration corrupted, check with json schema");
            const auto& user_list = user_list_result.value().get();
            for (const auto& user_id : user_list)
            {
                const auto user_id_casted = user_id.As<uid_t>();
                SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(user_id_casted.has_value(),
                                             "Configuration corrupted, check with json schema");
                user_ids.push_back(user_id_casted.value());
            }

            if (user.first == "QM")
            {
                user_map[QualityType::kASIL_QM] = std::move(user_ids);
            }
            else if (user.first == "B")
            {
                user_map[QualityType::kASIL_B] = std::move(user_ids);
            }
            else
            {
                score::mw::log::LogError("lola")
                    << "Unknown quality type in " << key << " " << user.first.GetAsStringView();
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            }
        }
    }

    return user_map;
}

auto ParseAllowedConsumer(const score::json::Object& json) -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    return ParseAllowedUser(json, kAllowedConsumerKey);
}

auto ParseAllowedProvider(const score::json::Object& json) -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    return ParseAllowedUser(json, kAllowedProviderKey);
}

class ServiceElementInstanceDeploymentParser
{
  public:
    explicit ServiceElementInstanceDeploymentParser(const score::json::Object& json_object) : json_object_{json_object} {}

    // See Note 1
    // coverity[autosar_cpp14_a15_5_3_violation]
    std::string GetName(const score::json::Object::const_iterator name) const
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(name != json_object_.cend(), "Configuration corrupted, check with json schema");
        const auto name_value = name->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(name_value.has_value(), "Configuration corrupted, check with json schema");
        return name_value.value().get();
    }

    void CheckContainsEvent(const score::json::Object::const_iterator name, const LolaServiceInstanceDeployment& service)
    {
        const auto name_value = GetName(name);
        if (service.ContainsEvent(name_value))
        {
            score::mw::log::LogFatal("lola") << "Event Name Duplicated. Not allowed";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }

    void CheckContainsField(const score::json::Object::const_iterator name, const LolaServiceInstanceDeployment& service)
    {
        const auto name_value = GetName(name);
        if (service.ContainsField(name_value))
        {
            score::mw::log::LogFatal("lola") << "Field Name Duplicated. Not allowed";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }

    template <typename element_type>
    std::optional<element_type> RetrieveJsonElement(const score::json::Object::const_iterator element_iterator)
    {
        if (element_iterator != json_object_.cend())
        {
            const auto element_value = element_iterator->second.As<element_type>();
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(element_value.has_value(), "Configuration corrupted, check with json schema");
            return element_value.value();
        }
        return {};
    }

    template <typename element_type>
    std::optional<element_type> RetrieveJsonElement(const std::string_view key)
    {
        return GetOptionalValueFromJson<element_type>(json_object_, key);
    }

    std::optional<LolaEventInstanceDeployment::SampleSlotCountType> GetNumberOfSampleSlots()
    {
        const auto& number_of_sample_slots_it = json_object_.find(kEventNumberOfSampleSlotsKey.data());

        // deprecation check "for max_samples"
        const auto& max_samples_it = json_object_.find(kEventMaxSamplesKey.data());
        if (max_samples_it == json_object_.cend())
        {
            return RetrieveJsonElement<SampleSlotCountType>(number_of_sample_slots_it);
        }

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(number_of_sample_slots_it == json_object_.cend(),
                                     "Configuration corrupted, check with json schema");

        score::mw::log::LogWarn("lola")
            << "<maxSamples> property for event is DEPRECATED! use <numberOfSampleSlots> property for event ";

        return RetrieveJsonElement<SampleSlotCountType>(max_samples_it);
    }

  private:
    const score::json::Object& json_object_;
    using SampleSlotCountType = LolaEventInstanceDeployment::SampleSlotCountType;
};

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaEventInstanceDeployment(const score::json::Object& json_map, LolaServiceInstanceDeployment& service) -> void
{
    const auto& events = json_map.find(kEventsKey.data());
    if (events == json_map.cend())
    {
        return;
    }

    const auto events_list_result = events->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(events_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& events_list = events_list_result.value().get();
    for (const auto& event : events_list)
    {
        auto event_obj = event.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(event_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& event_object = event_obj.value().get();
        const auto& max_concurrent_allocations_it = event_object.find(kEventMaxConcurrentAllocationsKey);
        AbortIfFound(max_concurrent_allocations_it, event_object);

        ServiceElementInstanceDeploymentParser deployment_parser{event_object};

        const auto& event_name_it = event_object.find(kEventNameKey.data());
        deployment_parser.CheckContainsEvent(event_name_it, service);

        auto event_name_value = deployment_parser.GetName(event_name_it);

        const auto number_of_sample_slots = deployment_parser.GetNumberOfSampleSlots();

        const auto max_subscribers =
            deployment_parser.RetrieveJsonElement<LolaEventInstanceDeployment::SubscriberCountType>(
                kEventMaxSubscribersKey);
        const auto enforce_max_samples =
            deployment_parser.RetrieveJsonElement<bool>(kEventEnforceMaxSamplesKey).value_or(true);

        const auto number_of_tracing_slots =
            deployment_parser.RetrieveJsonElement<NumberOfIpcTracingSlots_t>(kNumberOfIpcTracingSlotsKey)
                .value_or(kNumberOfIpcTracingSlotsDefault);

        auto event_deployment = LolaEventInstanceDeployment(number_of_sample_slots,
                                                            max_subscribers,
                                                            kMaxConcurrentAllocationsDefault,
                                                            enforce_max_samples,
                                                            number_of_tracing_slots);

        const auto emplace_result = service.events_.emplace(std::piecewise_construct,
                                                            std::forward_as_tuple(std::move(event_name_value)),
                                                            std::forward_as_tuple(event_deployment));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second, "Could not emplace element in map");
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaFieldInstanceDeployment(const score::json::Object& json_map, LolaServiceInstanceDeployment& service) -> void
{
    const auto& fields = json_map.find(kFieldsKey.data());
    if (fields == json_map.cend())
    {
        return;
    }

    const auto fields_list_result = fields->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(fields_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& fields_list = fields_list_result.value().get();
    for (const auto& field : fields_list)
    {
        auto field_obj = field.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(field_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& field_object = field_obj.value().get();
        const auto& max_concurrent_allocations_it = field_object.find(kFieldMaxConcurrentAllocationsKey);
        AbortIfFound(max_concurrent_allocations_it, field_object);

        ServiceElementInstanceDeploymentParser deployment_parser{field_object};

        const auto& field_name_it = field_object.find(kFieldNameKey);
        deployment_parser.CheckContainsField(field_name_it, service);

        auto field_name_value = deployment_parser.GetName(field_name_it);

        const auto number_of_sample_slots =
            deployment_parser.RetrieveJsonElement<LolaFieldInstanceDeployment::SampleSlotCountType>(
                kFieldNumberOfSampleSlotsKey);  // different handling than Events which xor-ing with numberOfSampleSlots (using GetNumberOfSampleSlots()) Line 405
        const auto max_subscribers =
            deployment_parser.RetrieveJsonElement<LolaFieldInstanceDeployment::SubscriberCountType>(
                kFieldMaxSubscribersKey);
        const auto enforce_max_samples =
            deployment_parser.RetrieveJsonElement<bool>(kFieldEnforceMaxSamplesKey).value_or(true);
        const auto number_of_tracing_slots =
            deployment_parser.RetrieveJsonElement<NumberOfIpcTracingSlots_t>(kNumberOfIpcTracingSlotsKey)
                .value_or(kNumberOfIpcTracingSlotsDefault);

        auto field_deployment = LolaFieldInstanceDeployment(number_of_sample_slots,
                                                            max_subscribers,
                                                            kMaxConcurrentAllocationsDefault,
                                                            enforce_max_samples,
                                                            number_of_tracing_slots);
        const auto emplace_result = service.fields_.emplace(std::piecewise_construct,
                                                            std::forward_as_tuple(std::move(field_name_value)),
                                                            std::forward_as_tuple(field_deployment));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second, "Could not emplace element in map");
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaMethodInstanceDeployment(const score::json::Object& json_map, LolaServiceInstanceDeployment& service)
    -> void
{
    const auto& methods = json_map.find(kMethodsKey.data());
    if (methods == json_map.cend())
    {
        return;
    }
    // TM: missing methodId
    const auto methods_list_result = methods->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(methods_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& methods_list = methods_list_result.value().get();
    for (const auto& method : methods_list)
    {
        const auto method_casted = method.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(method_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto& method_object = method_casted.value().get();
        const auto& method_name = GetValueFromJson<std::string>(method_object, kMethodNameKey);
        const std::optional<LolaMethodInstanceDeployment::QueueSize> queue_size =
            GetOptionalValueFromJson<LolaMethodInstanceDeployment::QueueSize>(method_object, kMethodQueueSizeKey);
        const LolaMethodInstanceDeployment method_deployment{queue_size};

        const auto emplace_result = service.methods_.emplace(
            std::piecewise_construct, std::forward_as_tuple(method_name), std::forward_as_tuple(method_deployment));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second, "Could not emplace element in map");
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceElementTracingEnabled(const score::json::Object& json_map,
                                       TracingConfiguration& tracing_configuration,
                                       const std::string_view service_type_name_view,
                                       const InstanceSpecifier& instance_specifier,
                                       const ServiceElementType service_element_type)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        (service_element_type == ServiceElementType::EVENT) || (service_element_type == ServiceElementType::FIELD),
        "Only FIELD or EVENT are allowed as ServiceElementTypes.");

    const auto& [ElementKey, ElementNameKey] =
        (service_element_type == ServiceElementType::EVENT ? std::make_pair(kEventsKey, kEventNameKey)
                                                           : std::make_pair(kFieldsKey, kFieldNameKey));

    const auto& service_elements = json_map.find(ElementKey);
    if (service_elements == json_map.cend())
    {
        return;
    }

    const auto elements_list_result = service_elements->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(elements_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& elements_list = elements_list_result.value().get();
    for (const auto& element : elements_list)
    {
        auto element_obj = element.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(element_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& element_object = element_obj.value().get();
        const auto service_element_name = element_object.find(ElementNameKey);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(service_element_name != element_object.end());

        const auto& number_of_tracing_slots_it = element_object.find(kNumberOfIpcTracingSlotsKey);
        if (number_of_tracing_slots_it != element_object.end())
        {
            const auto number_of_tracing_slots_casted =
                number_of_tracing_slots_it->second.As<NumberOfIpcTracingSlots_t>();
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(number_of_tracing_slots_casted.has_value(),
                                         "Configuration corrupted, check with json schema");
            const auto number_of_tracing_slots = number_of_tracing_slots_casted.value();
            if (number_of_tracing_slots > 0U)
            {
                auto service_element_name_value_casted = service_element_name->second.As<std::string>();
                SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_element_name_value_casted.has_value(),
                                             "Configuration corrupted, check with json schema");
                auto service_element_name_value = std::move(service_element_name_value_casted).value().get();

                std::string service_type_name{service_type_name_view.data(), service_type_name_view.size()};
                tracing::ServiceElementIdentifier service_element_identifier{
                    std::move(service_type_name), std::move(service_element_name_value), service_element_type};
                // This enables tracing when number of tracing slots is nonzero
                tracing_configuration.SetServiceElementTracingEnabled(std::move(service_element_identifier),
                                                                      instance_specifier);
            }
        }
    }
}

auto ParsePermissionChecks(const score::json::Object& deployment_map) -> std::string_view
{
    const auto permission_checks = deployment_map.find(kPermissionChecksKey.data());
    if (permission_checks != deployment_map.cend())
    {
        auto perm_result_obj = permission_checks->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(perm_result_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& perm_result = perm_result_obj.value().get();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(perm_result == kFilePermissionsOnEmpty || perm_result == kStrictPermission,
                                     "Configuration corrupted, check with json schema");
        return perm_result;
    }

    return kFilePermissionsOnEmpty;
}

auto ParseLolaServiceInstanceDeployment(const score::json::Object& json_map) -> LolaServiceInstanceDeployment
{
    LolaServiceInstanceDeployment service{};
    const auto& found_shm_size = json_map.find(kLolaShmSizeKey.data());
    if (found_shm_size != json_map.cend())
    {
        const auto found_shm_size_casted = found_shm_size->second.As<std::uint64_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(found_shm_size_casted.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto found_shm_size_value = found_shm_size_casted.value();
        service.shared_memory_size_ = found_shm_size_value;
    }

    const auto& found_control_asil_b_shm_size = json_map.find(kLolaControlAsilBShmSizeKey.data());
    if (found_control_asil_b_shm_size != json_map.cend())
    {
        const auto found_control_asil_b_shm_size_casted = found_control_asil_b_shm_size->second.As<std::uint64_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(found_control_asil_b_shm_size_casted.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto found_control_asil_b_shm_size_value = found_control_asil_b_shm_size_casted.value();
        service.control_asil_b_memory_size_ = found_control_asil_b_shm_size_value;
    }

    const auto& found_control_qm_shm_size = json_map.find(kLolaControlQmShmSizeKey.data());
    if (found_control_qm_shm_size != json_map.cend())
    {
        const auto found_control_qm_shm_size_casted = found_control_qm_shm_size->second.As<std::uint64_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(found_control_qm_shm_size_casted.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto found_control_qm_shm_size_value = found_control_qm_shm_size_casted.value();
        service.control_qm_memory_size_ = found_control_qm_shm_size_value;
    }

    const auto& instance_id = json_map.find(kInstanceIdKey.data());
    if (instance_id != json_map.cend())
    {
        const auto instance_id_casted = instance_id->second.As<LolaServiceInstanceId::InstanceId>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(instance_id_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto instance_id_value = instance_id_casted.value();
        service.instance_id_ = LolaServiceInstanceId{instance_id_value};
    }

    ParseLolaEventInstanceDeployment(json_map, service);
    ParseLolaFieldInstanceDeployment(json_map, service);
    ParseLolaMethodInstanceDeployment(json_map, service);

    service.strict_permissions_ = ParsePermissionChecks(json_map) == kStrictPermission;

    service.allowed_consumer_ = ParseAllowedConsumer(json_map);
    service.allowed_provider_ = ParseAllowedProvider(json_map);

    return service;
}

auto ParseServiceInstanceDeployments(const score::json::Object& json_map,
                                     TracingConfiguration& tracing_configuration,
                                     const ServiceIdentifierType& service,
                                     const InstanceSpecifier& instance_specifier)
    -> std::vector<ServiceInstanceDeployment>
{
    const auto& deploymentInstances = json_map.find(kDeploymentInstancesKey);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(deploymentInstances != json_map.cend(),
                                 "Configuration corrupted, check with json schema");

    std::vector<ServiceInstanceDeployment> deployments{};

    auto deplymentObjs_result = deploymentInstances->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(deplymentObjs_result.has_value(), "Configuration corrupted, check with json schema");
    auto& deplymentObjs = deplymentObjs_result.value().get();
    for (const auto& deploymentInstance : deplymentObjs)
    {
        auto deployment_obj = deploymentInstance.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(deployment_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& deployment_map = deployment_obj.value().get();

        const auto asil_level = ParseAsilLevel(deployment_map);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(asil_level.has_value() && (asil_level.value() != QualityType::kInvalid),
                                     "Configuration corrupted, check with json schema");
        const auto binding = deployment_map.find(kBindingKey.data());
        if (binding != deployment_map.cend())
        {
            auto bindingValue_result = binding->second.As<std::string>();
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(bindingValue_result.has_value(),
                                         "Configuration corrupted, check with json schema");
            const auto& bindingValue = bindingValue_result.value().get();
            if (bindingValue == kSomeIpBinding)
            {
                score::mw::log::LogFatal("lola") << "Provided SOME/IP binding, which can not be parsed.";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            }
            else if (bindingValue == kShmBinding)
            {
                // Return Value not needed in this context
                score::cpp::ignore = deployments.emplace_back(service,
                                                       ParseLolaServiceInstanceDeployment(deployment_map),
                                                       asil_level.value(),
                                                       instance_specifier);
            }
            else
            {
                score::mw::log::LogFatal("lola") << "Unknown binding provided. Required argument.";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            }

            if (tracing_configuration.IsTracingEnabled())
            {
                constexpr auto EVENT = ServiceElementType::EVENT;
                constexpr auto FIELD = ServiceElementType::FIELD;
                const auto service_name = service.ToString();
                ParseServiceElementTracingEnabled(
                    deployment_map, tracing_configuration, service_name, instance_specifier, EVENT);
                ParseServiceElementTracingEnabled(
                    deployment_map, tracing_configuration, service_name, instance_specifier, FIELD);
            }
        }
        else
        {
            score::mw::log::LogFatal("lola") << "No binding provided. Required argument.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return deployments;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceInstances(const score::json::Object& object, TracingConfiguration& tracing_configuration)
    -> Configuration::ServiceInstanceDeployments
{
    const auto& servicesInstances = object.find(kServiceInstancesKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(servicesInstances != object.cend(), "Configuration corrupted, check with json schema");
    Configuration::ServiceInstanceDeployments service_instance_deployments{};
    auto services_list_result = servicesInstances->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(services_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& services_list = services_list_result.value().get();
    for (const auto& service_instance : services_list)
    {
        auto service_instance_obj = service_instance.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_instance_obj.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto& service_instance_map = service_instance_obj.value().get();
        auto instanceSpecifier = ParseInstanceSpecifier(service_instance_map);

        auto service_identifier = ParseServiceTypeIdentifier(service_instance_map);

        auto instance_deployments = ParseServiceInstanceDeployments(
            service_instance_map, tracing_configuration, service_identifier, instanceSpecifier);
        if (instance_deployments.size() != 1U)
        {
            score::mw::log::LogFatal("lola") << "More or less then one deployment for " << service_identifier.ToString()
                                           << ". Multi-Binding support right now not supported";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }

        auto emplaceRes = service_instance_deployments.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(instanceSpecifier),
                                                               std::forward_as_tuple(instance_deployments.at(0U)));
        if (emplaceRes.second == false)
        {
            score::mw::log::LogFatal("lola") << "Unexpected error, when inserting service instance deployments.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return service_instance_deployments;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaEventTypeDeployments(const score::json::Object& json_map, LolaServiceTypeDeployment& service) -> bool
{
    const auto& events = json_map.find(kEventsKey.data());
    if (events == json_map.cend())
    {
        return false;
    }
    auto events_list_result = events->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(events_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& events_list = events_list_result.value().get();
    for (const auto& event : events_list)
    {
        const auto event_obj = event.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(event_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& event_object = event_obj.value().get();
        const auto& event_name = event_object.find(kEventNameKey.data());
        const auto& event_id = event_object.find(kEventIdKey.data());

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE((event_name != event_object.cend()) && (event_id != event_object.cend()),
                                     "Configuration corrupted, check with json schema");

        const auto event_name_casted = event_name->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(event_name_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto event_id_casted = event_id->second.As<std::uint16_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(event_id_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto result = service.events_.emplace(std::piecewise_construct,
                                                    std::forward_as_tuple(event_name_casted.value().get()),
                                                    std::forward_as_tuple(event_id_casted.value()));

        if (result.second != true)
        {
            score::mw::log::LogFatal("lola") << "An event was configured twice.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return true;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaFieldTypeDeployments(const score::json::Object& json_map, LolaServiceTypeDeployment& service) -> bool
{
    const auto& fields = json_map.find(kFieldsKey.data());
    if (fields == json_map.cend())
    {
        return false;
    }

    auto fields_list_result = fields->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(fields_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& fields_list = fields_list_result.value().get();
    for (const auto& field : fields_list)
    {
        auto field_obj = field.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(field_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& field_object = field_obj.value().get();
        const auto& field_name = field_object.find(kFieldNameKey.data());
        const auto& field_id = field_object.find(kFieldIdKey.data());

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE((field_name != field_object.cend()) && (field_id != field_object.cend()),
                                     "Configuration corrupted, check with json schema");

        const auto field_name_casted = field_name->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(field_name_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto field_id_casted = field_id->second.As<std::uint16_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(field_id_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto result = service.fields_.emplace(std::piecewise_construct,
                                                    std::forward_as_tuple(field_name_casted.value().get()),
                                                    std::forward_as_tuple(field_id_casted.value()));

        if (result.second != true)
        {
            score::mw::log::LogFatal("lola") << "A field was configured twice.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return true;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaMethodTypeDeployments(const score::json::Object& json_map, LolaServiceTypeDeployment& service) -> bool
{
    const auto& methods = json_map.find(kMethodsKey.data());
    if (methods == json_map.cend())
    {
        return false;
    }

    auto methods_list_result = methods->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(methods_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& methods_list = methods_list_result.value().get();
    for (const auto& method : methods_list)
    {
        const auto& method_object_casted = method.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(method_object_casted.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto& method_object = method_object_casted.value().get();
        const auto& method_name = method_object.find(kMethodNameKey.data());
        const auto& method_id = method_object.find(kMethodIdKey.data());

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE((method_name != method_object.cend()) && (method_id != method_object.cend()),
                                     "Configuration corrupted, check with json schema");

        const auto method_name_casted = method_name->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(method_name_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto method_id_casted = method_id->second.As<std::uint16_t>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(method_id_casted.has_value(), "Configuration corrupted, check with json schema");
        const auto result = service.methods_.emplace(std::piecewise_construct,
                                                     std::forward_as_tuple(method_name_casted.value().get()),
                                                     std::forward_as_tuple(method_id_casted.value()));

        if (result.second != true)
        {
            score::mw::log::LogFatal("lola") << "A method was configured twice.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return true;
}

auto AreEventFieldAndMethodIdsUnique(const LolaServiceTypeDeployment& lola_service_type_deployment) -> bool
{
    const auto& events = lola_service_type_deployment.events_;
    const auto& fields = lola_service_type_deployment.fields_;
    const auto& methods = lola_service_type_deployment.methods_;

    static_assert(std::is_same<LolaEventId, LolaFieldId>::value,
                  "EventId and FieldId should have the same underlying type.");
    static_assert(std::is_same<LolaEventId, LolaMethodId>::value,
                  "EventId and MethodId should have the same underlying type.");
    std::set<LolaEventId> ids{};

    for (const auto& event : events)
    {
        const auto result = ids.insert(event.second);
        if (!result.second)
        {
            return false;
        }
    }

    for (const auto& field : fields)
    {
        const auto result = ids.insert(field.second);
        if (!result.second)
        {
            return false;
        }
    }

    for (const auto& method : methods)
    {
        const auto result = ids.insert(method.second);
        if (!result.second)
        {
            return false;
        }
    }
    return true;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLoLaServiceTypeDeployments(const score::json::Object& json_map) -> LolaServiceTypeDeployment
{
    const auto& service_id = json_map.find(kServiceIdKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_id != json_map.cend(), "Configuration corrupted, check with json schema");

    const auto service_id_casted = service_id->second.As<std::uint16_t>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_id_casted.has_value(), "Configuration corrupted, check with json schema");
    LolaServiceTypeDeployment lola{service_id_casted.value()};
    const bool events_exist = ParseLolaEventTypeDeployments(json_map, lola);
    const bool fields_exist = ParseLolaFieldTypeDeployments(json_map, lola);
    const bool methods_exist = ParseLolaMethodTypeDeployments(json_map, lola);
    if (!events_exist && !fields_exist && !methods_exist)
    {
        score::mw::log::LogFatal("lola") << "Configuration should contain at least one event, field, or method.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    if (!AreEventFieldAndMethodIdsUnique(lola)) 
    {
        score::mw::log::LogFatal("lola") << "Configuration cannot contain duplicate eventId, fieldId, or methodId.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return lola;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceTypeDeployment(const score::json::Object& json_map) -> ServiceTypeDeployment
{
    const auto& bindings = json_map.find(kBindingsKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(bindings != json_map.cend(), "Configuration corrupted, check with json schema");

    const auto bindings_list_result = bindings->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(bindings_list_result.has_value(), "Configuration corrupted, check with json schema");
    const auto& bindings_list = bindings_list_result.value().get();
    for (const auto& binding : bindings_list)
    {
        auto binding_obj = binding.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(binding_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& binding_map = binding_obj.value().get();
        auto binding_type = binding_map.find(kBindingKey.data());
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(binding_type != binding_map.cend(),
                                     "Configuration corrupted, check with json schema");

        auto value_result = binding_type->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(value_result.has_value(), "Configuration corrupted, check with json schema");
        const auto& value = value_result.value().get();
        if (value == kShmBinding)
        {
            LolaServiceTypeDeployment lola_deployment = ParseLoLaServiceTypeDeployments(binding_map);
            return ServiceTypeDeployment{lola_deployment};
        }
        else if (value == kSomeIpBinding)
        {
            // we skip this, because we don't support SOME/IP right now.
        }
        else
        {
            score::mw::log::LogFatal("lola") << "No unknown binding provided. Required argument.";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return ServiceTypeDeployment{score::cpp::blank{}};
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceTypes(const score::json::Object& json_map) -> Configuration::ServiceTypeDeployments
{
    const auto& service_types = json_map.find(kServiceTypesKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_types != json_map.cend(), "Configuration corrupted, check with json schema");

    Configuration::ServiceTypeDeployments service_type_deployments{};
    const auto service_types_list_result = service_types->second.As<score::json::List>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_types_list_result.has_value(),
                                 "Configuration corrupted, check with json schema");
    const auto& service_types_list = service_types_list_result.value().get();
    for (const auto& service_type : service_types_list)
    {
        auto service_type_obj = service_type.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(service_type_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& service_type_map = service_type_obj.value().get();
        const auto service_identifier = ParseServiceTypeIdentifier(service_type_map);

        const auto service_deployment = ParseServiceTypeDeployment(service_type_map);
        const auto inserted = service_type_deployments.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(service_identifier),
                                                               std::forward_as_tuple(service_deployment));

        if (inserted.second != true)
        {
            score::mw::log::LogFatal("lola") << "Service Type was deployed twice";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
    }
    return service_type_deployments;
}

auto ParseReceiverQueueSize(const score::json::Object& global_config_map, const QualityType quality_type)
    -> score::cpp::optional<std::int32_t>
{
    const auto& queue_size = global_config_map.find(kQueueSizeKey.data());
    if (queue_size != global_config_map.cend())
    {
        std::string_view queue_type_str{};
        switch (quality_type)
        {
            case QualityType::kASIL_QM:
                queue_type_str = "QM-receiver";
                break;
            case QualityType::kASIL_B:
                queue_type_str = "B-receiver";
                break;
            case QualityType::kInvalid:  // LCOV_EXCL_LINE defensive programming
            default:  // LCOV_EXCL_LINE defensive programming Bug: We only must hand over QM or B here.
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);  // LCOV_EXCL_LINE defensive programming
                // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch statement
                break;
        }

        auto queue_size_obj = queue_size->second.As<json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(queue_size_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& queue_size_map = queue_size_obj.value().get();
        const auto& asil_queue_size = queue_size_map.find(queue_type_str.data());
        if (asil_queue_size != queue_size_map.cend())
        {
            return score::ResultToAmpOptionalOrElse(asil_queue_size->second.As<std::int32_t>(), [](const auto&) {
                score::mw::log::LogFatal("lola") << "Invalid value for ReceiverQueueSize";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            });
        }
        else
        {
            return score::cpp::nullopt;
        }
    }
    else
    {
        return score::cpp::nullopt;
    }
}

auto ParseSenderQueueSize(const score::json::Object& global_config_map) -> score::cpp::optional<std::int32_t>
{
    const auto& queue_size = global_config_map.find(kQueueSizeKey.data());
    if (queue_size != global_config_map.cend())
    {
        auto queue_size_obj = queue_size->second.As<json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(queue_size_obj.has_value(), "Configuration corrupted, check with json schema");
        const auto& queue_size_map = queue_size_obj.value().get();
        const auto& asil_tx_queue_size = queue_size_map.find("B-sender");   //TM: No QM-sender
        if (asil_tx_queue_size != queue_size_map.cend())
        {
            return score::ResultToAmpOptionalOrElse(asil_tx_queue_size->second.As<std::int32_t>(), [](const auto&) {
                score::mw::log::LogFatal("lola") << "Invalid value for SenderQueueSize";
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            });
        }
        else
        {
            return score::cpp::nullopt;
        }
    }
    else
    {
        return score::cpp::nullopt;
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseGlobalProperties(const score::json::Object& top_level_object) -> GlobalConfiguration
{
    GlobalConfiguration global_configuration{};
    const auto& process_properties = top_level_object.find(kGlobalPropertiesKey.data());
    if (process_properties != top_level_object.cend())
    {
        const auto process_properties_obj = process_properties->second.As<score::json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(process_properties_obj.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto& process_properties_map = process_properties_obj.value().get();
        const auto asil_level = ParseAsilLevel(process_properties_map);
        if (asil_level.has_value() == false)
        {
            // set default (ASIL-QM)
            global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);
        }
        else
        {
            switch (asil_level.value())
            {
                case QualityType::kInvalid:
                    ::score::mw::log::LogFatal("lola") << "Invalid ASIL in global/asil-level, terminating.";
                    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
                    // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch
                    break;
                case QualityType::kASIL_QM:
                case QualityType::kASIL_B:
                    global_configuration.SetProcessAsilLevel(asil_level.value());
                    break;
                // LCOV_EXCL_START defensive programming
                default:
                    ::score::mw::log::LogFatal("lola") << "Unexpected QualityType, terminating";
                    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
                    // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch
                    break;
                    // LCOV_EXCL_STOP
            }
        }

        const score::cpp::optional<std::int32_t> qm_rx_message_size{
            ParseReceiverQueueSize(process_properties_map, QualityType::kASIL_QM)};
        if (qm_rx_message_size.has_value())
        {
            global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_QM, qm_rx_message_size.value());
        }

        const score::cpp::optional<std::int32_t> b_rx_message_size{
            ParseReceiverQueueSize(process_properties_map, QualityType::kASIL_B)};
        if (b_rx_message_size.has_value())
        {
            global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_B, b_rx_message_size.value());
        }

        const score::cpp::optional<std::int32_t> b_tx_message_size{ParseSenderQueueSize(process_properties_map)};
        if (b_tx_message_size.has_value())
        {
            global_configuration.SetSenderMessageQueueSize(b_tx_message_size.value());
        }

        const score::cpp::optional<ShmSizeCalculationMode> shm_size_calc_mode{ParseShmSizeCalcMode(process_properties_map)};
        if (shm_size_calc_mode.has_value())
        {
            global_configuration.SetShmSizeCalcMode(shm_size_calc_mode.value());
        }

        const auto& application_id_it = process_properties_map.find(kApplicationIdKey.data());
        if (application_id_it != process_properties_map.cend())
        {
            const auto application_id_casted = application_id_it->second.As<std::uint32_t>();
            SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(application_id_casted.has_value(),
                                         "Configuration corrupted, check with json schema");
            const auto app_id = application_id_casted.value();
            global_configuration.SetApplicationId(app_id);
        }
    }
    else
    {
        global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);
    }
    return global_configuration;
}

auto ParseTracingEnabled(const score::json::Object& tracing_config_map) -> bool
{
    const auto& tracing_enabled = tracing_config_map.find(kTracingEnabledKey);
    if (tracing_enabled != tracing_config_map.cend())
    {
        const auto tracing_enabled_bool = tracing_enabled->second.As<bool>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(tracing_enabled_bool.has_value(),
                                     "Configuration corrupted, check with json schema");
        return tracing_enabled_bool.value();
    }
    return kTracingGloballyEnabledDefaultValue;
}

auto ParseTracingApplicationInstanceId(const score::json::Object& tracing_config_map) -> const std::string&
{
    const auto& tracing_application_instance_id = tracing_config_map.find(kTracingApplicationInstanceIDKey.data());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(tracing_application_instance_id != tracing_config_map.cend(),
                                 "Configuration corrupted, check with json schema");

    const auto tracing_application_instance_id_casted = tracing_application_instance_id->second.As<std::string>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(tracing_application_instance_id_casted.has_value(),
                                 "Configuration corrupted, check with json schema");
    return tracing_application_instance_id_casted.value().get();
}

auto ParseTracingTraceFilterConfigPath(const score::json::Object& tracing_config_map) -> std::string_view
{
    const auto& tracing_filter_config_path = tracing_config_map.find(kTracingTraceFilterConfigPathKey.data());
    if (tracing_filter_config_path != tracing_config_map.cend())
    {
        const auto tracing_filter_config_path_casted = tracing_filter_config_path->second.As<std::string>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(tracing_filter_config_path_casted.has_value(),
                                     "Configuration corrupted, check with json schema");
        return tracing_filter_config_path_casted.value().get();
    }
    else
    {
        return kTracingTraceFilterConfigPathDefaultValue;
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseTracingProperties(const score::json::Object& top_level_object) -> TracingConfiguration
{
    TracingConfiguration tracing_configuration{};
    const auto& tracing_properties = top_level_object.find(kTracingPropertiesKey);
    if (tracing_properties != top_level_object.cend())
    {
        auto tracing_properties_obj = tracing_properties->second.As<json::Object>();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(tracing_properties_obj.has_value(),
                                     "Configuration corrupted, check with json schema");
        const auto& tracing_properties_map = tracing_properties_obj.value().get();
        const auto tracing_enabled = ParseTracingEnabled(tracing_properties_map);
        tracing_configuration.SetTracingEnabled(tracing_enabled);

        auto tracing_application_instance_id = ParseTracingApplicationInstanceId(tracing_properties_map);
        tracing_configuration.SetApplicationInstanceID(std::move(tracing_application_instance_id));

        auto tracing_filter_config_path = ParseTracingTraceFilterConfigPath(tracing_properties_map);
        tracing_configuration.SetTracingTraceFilterConfigPath(
            std::string{tracing_filter_config_path.data(), tracing_filter_config_path.size()});
    }
    return tracing_configuration;
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

/**
 * \brief Checks, whether for all (binding) types used in service instances, there is also a corresponding type
 *        in service types.
 * @param config configuration to be crosschecked.
 */
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
        // check, that binding in service type and service instance are equal. Since currently ServiceTypeDeployment
        // only supports LolaServiceTypeDeployment, everything else than LolaServiceInstanceDeployment is an error.
        // LCOV_EXCL_BR_START: Defensive programming: Parse() currently terminates if the ServiceInstanceDeployment
        // contains anything other than a Lola binding. Therefore, it's impossible to reach this point without
        // a LolaServiceInstanceDeployment.
        if (!std::holds_alternative<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_))
        {
            // LCOV_EXCL_BR_STOP
            // LCOV_EXCL_START defensive programming: Parse() currently terminates if the ServiceInstanceDeployment
            // contains anything other than a Lola binding. Therefore, it's impossible to reach this point without
            // a LolaServiceInstanceDeployment.
            ::score::mw::log::LogFatal("lola") << "Service instance " << service_instance.first
                                             << "refers to an not yet supported binding. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
            // LCOV_EXCL_STOP
        }
        if (!std::holds_alternative<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_))
        {
            ::score::mw::log::LogFatal("lola") << "Service type " << service_instance.second.service_.ToString()
                                             << "refers to an not yet supported binding. This is invalid, terminating";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        }
        // check, that for each service-element-name in the instance deployment, there exists a corresponding
        // service-element-name in the type deployment
        const auto& serviceInstanceDeployment =
            std::get<LolaServiceInstanceDeployment>(service_instance.second.bindingInfo_);
        for (const auto& eventInstanceElement : serviceInstanceDeployment.events_)
        {
            const auto& serviceTypeDeployment =
                std::get<LolaServiceTypeDeployment>(foundServiceType->second.binding_info_);
            const auto search = serviceTypeDeployment.events_.find(eventInstanceElement.first);
            if (search == serviceTypeDeployment.events_.cend()) // TM: there was no check for name duplication during parsing, only ids
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

}  // namespace
}  // namespace score::mw::com::impl::configuration

/**
 * \brief Parses json configuration under given path and returns a Configuration object on success.
 */
/* Function is called with different type, thus different function is called. */
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that the call to Parse might throw due to std::bad_variant_access. This is a false positive.
// see the suppression of Parse.
// This is not an
// coverity[autosar_cpp14_a15_5_3_violation]
auto score::mw::com::impl::configuration::Parse(const std::string_view path) -> Configuration
{
    const score::json::JsonParser json_parser_obj;
    // Reason for banning is AoU of vaJson library about integrity of provided path.
    // This AoU is forwarded as AoU of Lola. See broken_link_c/issue/5835192
    // NOLINTNEXTLINE(score-banned-function): The user has to guarantee the integrity of the path
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola") << "Parsing config file" << path
                                         << "failed with error:" << json_result.error().Message() << ": "
                                         << json_result.error().UserMessage() << " . Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return Parse(std::move(json_result).value());
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that CrosscheckServiceInstancesToTypes might throw due to std::bad_variant_access. This is not an
// implicit throw. Since the function checks if the variant holds the right alternative and explicitely throws if not,
// after logging an error message.
// coverity[autosar_cpp14_a15_5_3_violation]
auto score::mw::com::impl::configuration::Parse(score::json::Any json) -> Configuration
{
    const auto json_obj = json.As<score::json::Object>();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(json_obj.has_value(), "Configuration corrupted, check with json schema");
    const auto& json_map = json_obj.value().get();

    auto tracing_configuration = ParseTracingProperties(json_map);
    auto service_type_deployments = ParseServiceTypes(json_map);
    auto service_instance_deployments = ParseServiceInstances(json_map, tracing_configuration);
    auto global_configuration = ParseGlobalProperties(json_map);

    Configuration configuration{std::move(service_type_deployments),
                                std::move(service_instance_deployments),
                                std::move(global_configuration),
                                std::move(tracing_configuration)};

    CrosscheckAsilLevels(configuration);
    CrosscheckServiceInstancesToTypes(configuration);

    return configuration;
}
