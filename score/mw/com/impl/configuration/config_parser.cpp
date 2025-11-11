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
constexpr auto kEventNumberOfSampleSlotsKey = "numberOfSampleSlots"sv;
constexpr auto kEventMaxSamplesKey = "maxSamples"sv;
constexpr auto kEventMaxSubscribersKey = "maxSubscribers"sv;
constexpr auto kEventEnforceMaxSamplesKey = "enforceMaxSamples"sv;
constexpr auto kEventMaxConcurrentAllocationsKey = "maxConcurrentAllocations"sv;
constexpr auto kMaxConcurrentAllocationsDefault = static_cast<std::uint8_t>(1U);
constexpr auto kFieldNumberOfSampleSlotsKey = "numberOfSampleSlots"sv;
constexpr auto kFieldMaxSubscribersKey = "maxSubscribers"sv;
constexpr auto kFieldEnforceMaxSamplesKey = "enforceMaxSamples"sv;
constexpr auto kFieldMaxConcurrentAllocationsKey = "maxConcurrentAllocations"sv;
constexpr auto kLolaShmSizeKey = "shm-size"sv;
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

void ErrorIfFound(const score::json::Object::const_iterator& iterator_to_element, const score::json::Object& json_obj)
{

    if (iterator_to_element != json_obj.end())
    {
        mw::log::LogFatal() << "Parsing an element " << (iterator_to_element->first.GetAsStringView())
                            << " which is not currently supported."
                            << " Remove this element from the configuration. Aborting!\n";

        // Abortion call tolerated. See Assumptions of Use in mw/com/design/README.md
        std::terminate();
    }
}

auto ParseInstanceSpecifier(const score::json::Any& json) -> InstanceSpecifier
{
    const auto& instanceSpecifierJson = json.As<score::json::Object>().value().get().find(kInstanceSpecifierKey.data());
    if (instanceSpecifierJson != json.As<score::json::Object>().value().get().cend())
    {
        const auto& string_view = instanceSpecifierJson->second.As<std::string>()->get();
        const auto instance_specifier_result =
            InstanceSpecifier::Create(std::string{string_view.data(), string_view.size()});
        if (!instance_specifier_result.has_value())
        {
            score::mw::log::LogFatal("lola") << "Invalid InstanceSpecifier.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        return instance_specifier_result.value();
    }

    score::mw::log::LogFatal("lola") << "No instance specifier provided. Required argument.";
    std::terminate(); /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
}

auto ParseServiceTypeName(const score::json::Any& json) -> const std::string&
{
    const auto& serviceTypeName = json.As<score::json::Object>().value().get().find(kServiceTypeNameKey.data());
    if (serviceTypeName != json.As<score::json::Object>().value().get().cend())
    {
        return serviceTypeName->second.As<std::string>().value().get();
    }

    score::mw::log::LogFatal("lola") << "No service type name provided. Required argument.";
    /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
    std::terminate();
}

auto ParseVersion(const score::json::Any& json) -> std::pair<std::uint32_t, std::uint32_t>
{
    const auto& version = json.As<score::json::Object>().value().get().find(kVersionKey.data());
    if (version != json.As<score::json::Object>().value().get().cend())
    {
        const auto& version_object = version->second.As<score::json::Object>().value().get();
        const auto major_version_number = version_object.find(kMajorVersionKey.data());
        const auto minor_version_number = version_object.find(kMinorVersionKey.data());

        const auto major_version_number_exists = major_version_number != version_object.cend();
        const auto minor_version_number_exists = minor_version_number != version_object.cend();

        // LCOV_EXCL_BR_START (Tool incorrectly marks the decision as "Decision couldn't be analyzed" despite all lines
        // in both branches (true / false) being covered. Suppression can be removed when bug is fixed in
        // Ticket-188259).
        if (major_version_number_exists && minor_version_number_exists)
        {
            // LCOV_EXCL_BR_STOP
            return std::pair<std::uint32_t, std::uint32_t>{major_version_number->second.As<std::uint32_t>().value(),
                                                           minor_version_number->second.As<std::uint32_t>().value()};
        }
    }

    score::mw::log::LogFatal("lola") << "No Version provided. Required argument.";
    /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
    std::terminate();
}

auto ParseServiceTypeIdentifier(const score::json::Any& json) -> ServiceIdentifierType
{
    const auto& name = ParseServiceTypeName(json);
    const auto& version = ParseVersion(json);

    return make_ServiceIdentifierType(name.data(), version.first, version.second);
}

auto ParseAsilLevel(const score::json::Any& json) -> score::cpp::optional<QualityType>
{
    const auto& quality = json.As<score::json::Object>().value().get().find(kAsilKey.data());
    if (quality != json.As<score::json::Object>().value().get().cend())
    {
        const auto& qualityValue = quality->second.As<std::string>().value().get();

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

auto ParseShmSizeCalcMode(const score::json::Any& json) -> score::cpp::optional<ShmSizeCalculationMode>
{
    const auto& shm_size_calc_mode = json.As<score::json::Object>().value().get().find(kShmSizeCalcModeKey.data());
    if (shm_size_calc_mode != json.As<score::json::Object>().value().get().cend())
    {
        const auto& shm_size_calc_mode_value = shm_size_calc_mode->second.As<std::string>().value().get();

        if (shm_size_calc_mode_value == kShmSizeCalcModeSimulation)
        {
            return ShmSizeCalculationMode::kSimulation;
        }
        else
        {
            score::mw::log::LogError("lola")
                << "Unknown value " << shm_size_calc_mode_value << " in key " << kShmSizeCalcModeKey;
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
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
auto ParseAllowedUser(const score::json::Any& json, std::string_view key) noexcept
    -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    std::unordered_map<QualityType, std::vector<uid_t>> user_map{};
    auto allowed_user = json.As<score::json::Object>().value().get().find(key.data());

    if (allowed_user != json.As<score::json::Object>().value().get().cend())
    {
        for (const auto& user : allowed_user->second.As<score::json::Object>().value().get())
        {
            std::vector<uid_t> user_ids{};
            for (const auto& user_id : user.second.As<score::json::List>().value().get())
            {
                user_ids.push_back(user_id.As<uid_t>().value());
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
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }
        }
    }

    return user_map;
}

auto ParseAllowedConsumer(const score::json::Any& json) noexcept -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    return ParseAllowedUser(json, kAllowedConsumerKey);
}

auto ParseAllowedProvider(const score::json::Any& json) noexcept -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    return ParseAllowedUser(json, kAllowedProviderKey);
}

class ServiceElementInstanceDeploymentParser
{
  public:
    explicit ServiceElementInstanceDeploymentParser(const score::json::Object& json_object) : json_object_{json_object}
    {
    }

    // See Note 1
    // coverity[autosar_cpp14_a15_5_3_violation]
    std::string GetName(const score::json::Object::const_iterator name) const noexcept
    {
        if (name == json_object_.cend())
        {
            score::mw::log::LogFatal("lola") << "No Event/Field-Name provided. Required attribute";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        return name->second.As<std::string>().value().get();
    }

    void CheckContainsEvent(const score::json::Object::const_iterator name,
                            const LolaServiceInstanceDeployment& service)
    {
        if (name != json_object_.cend() && service.ContainsEvent(name->second.As<std::string>().value().get()))
        {
            score::mw::log::LogFatal("lola") << "Event Name Duplicated. Not allowed";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }

    void CheckContainsField(const score::json::Object::const_iterator name,
                            const LolaServiceInstanceDeployment& service)
    {
        if (name != json_object_.cend() && service.ContainsField(name->second.As<std::string>().value().get()))
        {
            score::mw::log::LogFatal("lola") << "Field Name Duplicated. Not allowed";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }

    template <typename element_type>
    std::optional<element_type> RetrieveJsonElement(const score::json::Object::const_iterator element_iterator)
    {
        if (element_iterator != json_object_.cend())
        {
            return element_iterator->second.As<element_type>().value();
        }
        return {};
    }

    template <typename element_type>
    std::optional<element_type> RetrieveJsonElement(const std::string_view key)
    {
        return GetOptionalValueFromJson<element_type>(json_object_, key);
    }

    std::optional<LolaEventInstanceDeployment::SampleSlotCountType> GetNumberOfSampleSlots(
        const std::string& event_name)
    {
        const auto& number_of_sample_slots_it = json_object_.find(kEventNumberOfSampleSlotsKey.data());

        // deprecation check "for max_samples"
        const auto& max_samples_it = json_object_.find(kEventMaxSamplesKey.data());
        if (max_samples_it == json_object_.cend())
        {
            return RetrieveJsonElement<SampleSlotCountType>(number_of_sample_slots_it);
        }

        if (number_of_sample_slots_it != json_object_.cend())
        {
            score::mw::log::LogFatal("lola")
                << "<maxSamples> and <numberOfSampleSlots> provided for event " << event_name << ". This is invalid!";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }

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
auto ParseLolaEventInstanceDeployment(const score::json::Any& json, LolaServiceInstanceDeployment& service) noexcept
    -> void
{
    const auto& events = json.As<score::json::Object>().value().get().find(kEventsKey.data());
    if (events == json.As<score::json::Object>().value().get().cend())
    {
        return;
    }

    for (const auto& event : events->second.As<score::json::List>().value().get())
    {
        const auto& event_object = event.As<score::json::Object>().value().get();
        const auto& max_concurrent_allocations_it = event_object.find(kEventMaxConcurrentAllocationsKey);
        ErrorIfFound(max_concurrent_allocations_it, event_object);

        ServiceElementInstanceDeploymentParser deployment_parser{event_object};

        const auto& event_name_it = event_object.find(kEventNameKey.data());
        deployment_parser.CheckContainsEvent(event_name_it, service);

        auto event_name_value = deployment_parser.GetName(event_name_it);

        const auto number_of_sample_slots = deployment_parser.GetNumberOfSampleSlots(event_name_value);

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
auto ParseLolaFieldInstanceDeployment(const score::json::Any& json, LolaServiceInstanceDeployment& service) noexcept
    -> void
{
    const auto& fields = json.As<score::json::Object>().value().get().find(kFieldsKey.data());
    if (fields == json.As<score::json::Object>().value().get().cend())
    {
        return;
    }

    for (const auto& field : fields->second.As<score::json::List>().value().get())
    {
        const auto& field_object = field.As<score::json::Object>().value().get();
        const auto& max_concurrent_allocations_it = field_object.find(kFieldMaxConcurrentAllocationsKey);
        ErrorIfFound(max_concurrent_allocations_it, field_object);

        ServiceElementInstanceDeploymentParser deployment_parser{field_object};

        const auto& field_name_it = field_object.find(kFieldNameKey);
        deployment_parser.CheckContainsField(field_name_it, service);

        auto field_name_value = deployment_parser.GetName(field_name_it);

        const auto number_of_sample_slots =
            deployment_parser.RetrieveJsonElement<LolaFieldInstanceDeployment::SampleSlotCountType>(
                kFieldNumberOfSampleSlotsKey);
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
auto ParseServiceElementTracingEnabled(const score::json::Any& json,
                                       TracingConfiguration& tracing_configuration,
                                       const std::string_view service_type_name_view,
                                       const InstanceSpecifier& instance_specifier,
                                       const ServiceElementType service_element_type) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        (service_element_type == ServiceElementType::EVENT) || (service_element_type == ServiceElementType::FIELD),
        "Only FIELD or EVENT are allowed as ServiceElementTypes.");

    const auto& [ElementKey, ElementNameKey] =
        (service_element_type == ServiceElementType::EVENT ? std::make_pair(kEventsKey, kEventNameKey)
                                                           : std::make_pair(kFieldsKey, kFieldNameKey));

    const auto& service_elements = json.As<score::json::Object>().value().get().find(ElementKey);
    if (service_elements == json.As<score::json::Object>().value().get().cend())
    {
        return;
    }

    for (const auto& element : service_elements->second.As<score::json::List>().value().get())
    {
        const auto& element_object = element.As<score::json::Object>().value().get();
        const auto service_element_name = element_object.find(ElementNameKey);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(service_element_name != element_object.end());

        const auto& number_of_tracing_slots_it = element_object.find(kNumberOfIpcTracingSlotsKey);
        if (number_of_tracing_slots_it != element_object.end())
        {
            const auto number_of_tracing_slots =
                number_of_tracing_slots_it->second.As<NumberOfIpcTracingSlots_t>().value();
            if (number_of_tracing_slots > 0U)
            {
                auto service_element_name_value = service_element_name->second.As<std::string>().value().get();

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

auto ParsePermissionChecks(const score::json::Any& deployment_instance) -> std::string_view
{
    const auto permission_checks =
        deployment_instance.As<score::json::Object>().value().get().find(kPermissionChecksKey.data());
    if (permission_checks != deployment_instance.As<score::json::Object>().value().get().cend())
    {
        const auto& perm_result = permission_checks->second.As<std::string>().value().get();
        if (perm_result != kFilePermissionsOnEmpty && perm_result != kStrictPermission)
        {
            score::mw::log::LogFatal("lola") << "Unknown permission" << perm_result << "in permission-checks attribute";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        return perm_result;
    }

    return kFilePermissionsOnEmpty;
}

auto ParseLolaServiceInstanceDeployment(const score::json::Any& json) -> LolaServiceInstanceDeployment
{
    LolaServiceInstanceDeployment service{};
    const auto& found_shm_size = json.As<score::json::Object>().value().get().find(kLolaShmSizeKey.data());
    if (found_shm_size != json.As<score::json::Object>().value().get().cend())
    {
        service.shared_memory_size_ = found_shm_size->second.As<std::uint64_t>().value();
    }

    const auto& instance_id = json.As<score::json::Object>().value().get().find(kInstanceIdKey.data());
    if (instance_id != json.As<score::json::Object>().value().get().cend())
    {
        service.instance_id_ =
            LolaServiceInstanceId{instance_id->second.As<LolaServiceInstanceId::InstanceId>().value()};
    }

    ParseLolaEventInstanceDeployment(json, service);
    ParseLolaFieldInstanceDeployment(json, service);

    service.strict_permissions_ = ParsePermissionChecks(json) == kStrictPermission;

    service.allowed_consumer_ = ParseAllowedConsumer(json);
    service.allowed_provider_ = ParseAllowedProvider(json);

    return service;
}

auto ParseServiceInstanceDeployments(const score::json::Any& json,
                                     TracingConfiguration& tracing_configuration,
                                     const ServiceIdentifierType& service,
                                     const InstanceSpecifier& instance_specifier)
    -> std::vector<ServiceInstanceDeployment>
{
    const auto& deploymentInstances = json.As<score::json::Object>().value().get().find(kDeploymentInstancesKey);
    if (deploymentInstances == json.As<score::json::Object>().value().get().cend())
    {
        score::mw::log::LogFatal("lola") << "No deployment instances provided. Required argument.";
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }

    std::vector<ServiceInstanceDeployment> deployments{};

    auto& deplymentObjs = deploymentInstances->second.As<score::json::List>().value().get();
    for (const auto& deploymentInstance : deplymentObjs)
    {
        const auto asil_level = ParseAsilLevel(deploymentInstance);
        if ((asil_level.has_value() == false) || (asil_level.value() == QualityType::kInvalid))
        {
            score::mw::log::LogFatal("lola") << "Invalid or no ASIL-Level provided. Required argument.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        const auto binding = deploymentInstance.As<score::json::Object>().value().get().find(kBindingKey.data());
        if (binding != deploymentInstance.As<score::json::Object>().value().get().cend())
        {
            const auto& bindingValue = binding->second.As<std::string>().value().get();
            if (bindingValue == kSomeIpBinding)
            {
                score::mw::log::LogFatal("lola") << "Provided SOME/IP binding, which can not be parsed.";
                std::terminate();
            }
            else if (bindingValue == kShmBinding)
            {
                // Return Value not needed in this context
                score::cpp::ignore = deployments.emplace_back(service,
                                                              ParseLolaServiceInstanceDeployment(deploymentInstance),
                                                              asil_level.value(),
                                                              instance_specifier);
            }
            else
            {
                score::mw::log::LogFatal("lola") << "No unknown binding provided. Required argument.";
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }

            if (tracing_configuration.IsTracingEnabled())
            {
                constexpr auto EVENT = ServiceElementType::EVENT;
                constexpr auto FIELD = ServiceElementType::FIELD;
                const auto service_name = service.ToString();
                ParseServiceElementTracingEnabled(
                    deploymentInstance, tracing_configuration, service_name, instance_specifier, EVENT);
                ParseServiceElementTracingEnabled(
                    deploymentInstance, tracing_configuration, service_name, instance_specifier, FIELD);
            }
        }
        else
        {
            score::mw::log::LogFatal("lola") << "No binding provided. Required argument.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return deployments;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceInstances(const score::json::Any& json, TracingConfiguration& tracing_configuration) noexcept
    -> Configuration::ServiceInstanceDeployments
{
    const auto& object = json.As<score::json::Object>().value().get();
    const auto& servicesInstances = object.find(kServiceInstancesKey.data());
    if (servicesInstances == object.cend())
    {
        score::mw::log::LogFatal("lola") << "No service instances provided. Required argument.";
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }
    Configuration::ServiceInstanceDeployments service_instance_deployments{};
    for (const auto& serviceInstance : servicesInstances->second.As<score::json::List>().value().get())
    {
        auto instanceSpecifier = ParseInstanceSpecifier(serviceInstance);

        auto service_identifier = ParseServiceTypeIdentifier(serviceInstance);

        auto instance_deployments = ParseServiceInstanceDeployments(
            serviceInstance, tracing_configuration, service_identifier, instanceSpecifier);
        if (instance_deployments.size() != 1U)
        {
            score::mw::log::LogFatal("lola") << "More or less then one deployment for " << service_identifier.ToString()
                                             << ". Multi-Binding support right now not supported";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }

        auto emplaceRes = service_instance_deployments.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(instanceSpecifier),
                                                               std::forward_as_tuple(instance_deployments.at(0U)));
        if (emplaceRes.second == false)
        {
            score::mw::log::LogFatal("lola") << "Unexpected error, when inserting service instance deployments.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return service_instance_deployments;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaEventTypeDeployments(const score::json::Any& json, LolaServiceTypeDeployment& service) noexcept -> bool
{
    const auto& events = json.As<score::json::Object>().value().get().find(kEventsKey.data());
    if (events == json.As<score::json::Object>().value().get().cend())
    {
        return false;
    }
    for (const auto& event : events->second.As<score::json::List>().value().get())
    {
        const auto& event_object = event.As<score::json::Object>().value().get();
        const auto& event_name = event_object.find(kEventNameKey.data());
        const auto& event_id = event_object.find(kEventIdKey.data());

        if ((event_name != event_object.cend()) && (event_id != event_object.cend()))
        {
            const auto result =
                service.events_.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(event_name->second.As<std::string>().value().get()),
                                        std::forward_as_tuple(event_id->second.As<std::uint16_t>().value()));

            if (result.second != true)
            {
                score::mw::log::LogFatal("lola") << "An event was configured twice.";
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }
        }
        else
        {
            score::mw::log::LogFatal("lola") << "Either no Event-Name or no Event-Id provided";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return true;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLolaFieldTypeDeployments(const score::json::Any& json, LolaServiceTypeDeployment& service) noexcept -> bool
{
    const auto& fields = json.As<score::json::Object>().value().get().find(kFieldsKey.data());
    if (fields == json.As<score::json::Object>().value().get().cend())
    {
        return false;
    }

    for (const auto& field : fields->second.As<score::json::List>().value().get())
    {
        const auto& field_object = field.As<score::json::Object>().value().get();
        const auto& field_name = field_object.find(kFieldNameKey.data());
        const auto& field_id = field_object.find(kFieldIdKey.data());

        if ((field_name != field_object.cend()) && (field_id != field_object.cend()))
        {
            const auto result =
                service.fields_.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(field_name->second.As<std::string>().value().get()),
                                        std::forward_as_tuple(field_id->second.As<std::uint16_t>().value()));

            if (result.second != true)
            {
                score::mw::log::LogFatal("lola") << "A field was configured twice.";
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }
        }
        else
        {
            score::mw::log::LogFatal("lola") << "Either no Field-Name or no Field-Id provided";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return true;
}

auto AreEventAndFieldIdsUnique(const LolaServiceTypeDeployment& lola_service_type_deployment) noexcept -> bool
{
    const auto& events = lola_service_type_deployment.events_;
    const auto& fields = lola_service_type_deployment.fields_;

    static_assert(std::is_same<LolaEventId, LolaFieldId>::value,
                  "EventId and FieldId should have the same underlying type.");
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
    return true;
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseLoLaServiceTypeDeployments(const score::json::Any& json) noexcept -> LolaServiceTypeDeployment
{
    const auto& service_id = json.As<score::json::Object>().value().get().find(kServiceIdKey.data());
    if (service_id != json.As<score::json::Object>().value().get().cend())
    {
        LolaServiceTypeDeployment lola{service_id->second.As<std::uint16_t>().value()};
        const bool events_exist = ParseLolaEventTypeDeployments(json, lola);
        const bool fields_exist = ParseLolaFieldTypeDeployments(json, lola);
        if (!events_exist && !fields_exist)
        {
            score::mw::log::LogFatal("lola") << "Configuration should contain at least one event or field.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        if (!AreEventAndFieldIdsUnique(lola))
        {
            score::mw::log::LogFatal("lola") << "Configuration cannot contain duplicate eventId or fieldIds.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
        return lola;
    }
    else
    {
        score::mw::log::LogFatal("lola") << "No Service Id Provided. Required argument.";
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceTypeDeployment(const score::json::Any& json) noexcept -> ServiceTypeDeployment
{
    const auto& bindings = json.As<score::json::Object>().value().get().find(kBindingsKey.data());
    for (const auto& binding : bindings->second.As<score::json::List>().value().get())
    {
        auto binding_type = binding.As<score::json::Object>().value().get().find(kBindingKey.data());
        if (binding_type != binding.As<score::json::Object>().value().get().cend())
        {
            const auto& value = binding_type->second.As<std::string>().value().get();
            if (value == kShmBinding)
            {
                LolaServiceTypeDeployment lola_deployment = ParseLoLaServiceTypeDeployments(binding);
                return ServiceTypeDeployment{lola_deployment};
            }
            else if (value == kSomeIpBinding)
            {
                // we skip this, because we don't support SOME/IP right now.
            }
            else
            {
                score::mw::log::LogFatal("lola") << "No unknown binding provided. Required argument.";
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }
        }
        else
        {
            score::mw::log::LogFatal("lola") << "No binding provided. Required argument.";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return ServiceTypeDeployment{score::cpp::blank{}};
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseServiceTypes(const score::json::Any& json) noexcept -> Configuration::ServiceTypeDeployments
{
    const auto& service_types = json.As<score::json::Object>().value().get().find(kServiceTypesKey.data());
    if (service_types == json.As<score::json::Object>().value().get().cend())
    {
        score::mw::log::LogFatal("lola") << "No service type deployments provided. Terminating";
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }

    Configuration::ServiceTypeDeployments service_type_deployments{};
    for (const auto& service_type : service_types->second.As<score::json::List>().value().get())
    {
        const auto service_identifier = ParseServiceTypeIdentifier(service_type);

        const auto service_deployment = ParseServiceTypeDeployment(service_type);
        const auto inserted = service_type_deployments.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(service_identifier),
                                                               std::forward_as_tuple(service_deployment));

        if (inserted.second != true)
        {
            score::mw::log::LogFatal("lola") << "Service Type was deployed twice";
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
        }
    }
    return service_type_deployments;
}

auto ParseReceiverQueueSize(const score::json::Any& global_config, const QualityType quality_type)
    -> score::cpp::optional<std::int32_t>
{
    const auto& global_config_map = global_config.As<json::Object>().value().get();
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
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();  // LCOV_EXCL_LINE defensive programming
                // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch statement
                break;
        }

        const auto& queue_size_map = queue_size->second.As<json::Object>().value().get();
        const auto& asil_queue_size = queue_size_map.find(queue_type_str.data());
        if (asil_queue_size != queue_size_map.cend())
        {
            return asil_queue_size->second.As<std::int32_t>().value();
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

auto ParseSenderQueueSize(const score::json::Any& global_config) -> score::cpp::optional<std::int32_t>
{
    const auto& global_config_map = global_config.As<json::Object>().value().get();
    const auto& queue_size = global_config_map.find(kQueueSizeKey.data());
    if (queue_size != global_config_map.cend())
    {
        const auto& queue_size_map = queue_size->second.As<json::Object>().value().get();
        const auto& asil_tx_queue_size = queue_size_map.find("B-sender");
        if (asil_tx_queue_size != queue_size_map.cend())
        {
            return asil_tx_queue_size->second.As<std::int32_t>().value();
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
auto ParseGlobalProperties(const score::json::Any& json) noexcept -> GlobalConfiguration
{
    GlobalConfiguration global_configuration{};

    const auto& top_level_object = json.As<score::json::Object>().value().get();
    const auto& process_properties = top_level_object.find(kGlobalPropertiesKey.data());
    if (process_properties != top_level_object.cend())
    {
        const auto asil_level = ParseAsilLevel(process_properties->second);
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
                    /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                    std::terminate();
                    // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch
                    break;
                case QualityType::kASIL_QM:
                case QualityType::kASIL_B:
                    global_configuration.SetProcessAsilLevel(asil_level.value());
                    break;
                // LCOV_EXCL_START defensive programming
                default:
                    ::score::mw::log::LogFatal("lola") << "Unexpected QualityType, terminating";
                    /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                    std::terminate();
                    // coverity[autosar_cpp14_m0_1_1_violation]: Break necessary to have well-formed switch
                    break;
                    // LCOV_EXCL_STOP
            }
        }

        const score::cpp::optional<std::int32_t> qm_rx_message_size{
            ParseReceiverQueueSize(process_properties->second, QualityType::kASIL_QM)};
        if (qm_rx_message_size.has_value())
        {
            global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_QM, qm_rx_message_size.value());
        }

        const score::cpp::optional<std::int32_t> b_rx_message_size{
            ParseReceiverQueueSize(process_properties->second, QualityType::kASIL_B)};
        if (b_rx_message_size.has_value())
        {
            global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_B, b_rx_message_size.value());
        }

        const score::cpp::optional<std::int32_t> b_tx_message_size{ParseSenderQueueSize(process_properties->second)};
        if (b_tx_message_size.has_value())
        {
            global_configuration.SetSenderMessageQueueSize(b_tx_message_size.value());
        }

        const score::cpp::optional<ShmSizeCalculationMode> shm_size_calc_mode{
            ParseShmSizeCalcMode(process_properties->second)};
        if (shm_size_calc_mode.has_value())
        {
            global_configuration.SetShmSizeCalcMode(shm_size_calc_mode.value());
        }

        const auto& process_properties_map = process_properties->second.As<json::Object>().value().get();
        const auto& application_id_it = process_properties_map.find(kApplicationIdKey.data());
        if (application_id_it != process_properties_map.cend())
        {
            const auto app_id = application_id_it->second.As<std::uint32_t>().value();
            global_configuration.SetApplicationId(app_id);
        }
    }
    else
    {
        global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);
    }
    return global_configuration;
}

auto ParseTracingEnabled(const score::json::Any& tracing_config) -> bool
{
    const auto& tracing_config_map = tracing_config.As<json::Object>().value().get();
    const auto& tracing_enabled = tracing_config_map.find(kTracingEnabledKey);
    if (tracing_enabled != tracing_config_map.cend())
    {
        return tracing_enabled->second.As<bool>().value();
    }
    return kTracingGloballyEnabledDefaultValue;
}

auto ParseTracingApplicationInstanceId(const score::json::Any& tracing_config) -> const std::string&
{
    const auto& tracing_config_map = tracing_config.As<json::Object>().value().get();
    const auto& tracing_application_instance_id = tracing_config_map.find(kTracingApplicationInstanceIDKey.data());
    if (tracing_application_instance_id != tracing_config_map.cend())
    {
        return tracing_application_instance_id->second.As<std::string>().value().get();
    }
    score::mw::log::LogFatal("lola") << "Could not find" << kTracingApplicationInstanceIDKey
                                     << "in json file which is a required attribute.";
    std::terminate();
}

auto ParseTracingTraceFilterConfigPath(const score::json::Any& tracing_config) -> std::string_view
{
    const auto& tracing_config_map = tracing_config.As<json::Object>().value().get();
    const auto& tracing_filter_config_path = tracing_config_map.find(kTracingTraceFilterConfigPathKey.data());
    if (tracing_filter_config_path != tracing_config_map.cend())
    {
        return tracing_filter_config_path->second.As<std::string>().value().get();
    }
    else
    {
        return kTracingTraceFilterConfigPathDefaultValue;
    }
}

// See Note 1
// coverity[autosar_cpp14_a15_5_3_violation]
auto ParseTracingProperties(const score::json::Any& json) noexcept -> TracingConfiguration
{
    TracingConfiguration tracing_configuration{};
    const auto& top_level_object = json.As<score::json::Object>().value().get();
    const auto& tracing_properties = top_level_object.find(kTracingPropertiesKey);
    if (tracing_properties != top_level_object.cend())
    {
        const auto tracing_enabled = ParseTracingEnabled(tracing_properties->second);
        tracing_configuration.SetTracingEnabled(tracing_enabled);

        auto tracing_application_instance_id = ParseTracingApplicationInstanceId(tracing_properties->second);
        tracing_configuration.SetApplicationInstanceID(std::move(tracing_application_instance_id));

        auto tracing_filter_config_path = ParseTracingTraceFilterConfigPath(tracing_properties->second);
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
            /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            std::terminate();
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
            std::terminate(); /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
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
            ::score::mw::log::LogFatal("lola")
                << "Service instance " << service_instance.first
                << "refers to an not yet supported binding. This is invalid, terminating";
            std::terminate(); /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
            // LCOV_EXCL_STOP
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
            if (search == serviceTypeDeployment.events_.cend())
            {
                ::score::mw::log::LogFatal("lola")
                    << "Service instance " << service_instance.first << "event" << eventInstanceElement.first
                    << "refers to an event, which doesn't exist in the referenced service type ("
                    << service_instance.second.service_.ToString() << "). This is invalid, terminating";
                std::terminate(); /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
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
                std::terminate(); /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
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
auto score::mw::com::impl::configuration::Parse(const std::string_view path) noexcept -> Configuration
{
    const score::json::JsonParser json_parser_obj;
    // Reason for banning is AoU of vaJson library about integrity of provided path.
    // This AoU is forwarded as AoU of Lola. See broken_link_c/issue/5835192
    // NOLINTNEXTLINE(score-banned-function): The user has to guarantee the integrity of the path
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing config file" << path << "failed with error:" << json_result.error().Message() << ": "
            << json_result.error().UserMessage() << " . Terminating.";
        std::terminate();
    }
    return Parse(std::move(json_result).value());
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that CrosscheckServiceInstancesToTypes might throw due to std::bad_variant_access. This is not an
// implicit throw. Since the function checks if the variant holds the right alternative and explicitely throws if not,
// after logging an error message.
// coverity[autosar_cpp14_a15_5_3_violation]
auto score::mw::com::impl::configuration::Parse(score::json::Any json) noexcept -> Configuration
{
    auto tracing_configuration = ParseTracingProperties(json);
    auto service_type_deployments = ParseServiceTypes(json);
    auto service_instance_deployments = ParseServiceInstances(json, tracing_configuration);
    auto global_configuration = ParseGlobalProperties(json);

    Configuration configuration{std::move(service_type_deployments),
                                std::move(service_instance_deployments),
                                std::move(global_configuration),
                                std::move(tracing_configuration)};

    CrosscheckAsilLevels(configuration);
    CrosscheckServiceInstancesToTypes(configuration);

    return configuration;
}
