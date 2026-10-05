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
#include "score/mw/com/impl/configuration/configuration_flatbuffer_parsing_strategy.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>

#if defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)

#include "score/mw/com/impl/configuration/config_validate.h"

#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

// This is a patched copy of generate_cpp's output (:mw_com_config_fbs) -- see the
// mw_com_config_generated_h genrule in BUILD for why the patch is necessary.
#include "score/mw/com/impl/configuration/mw_com_config_generated_patched.h"

#include "score/filesystem/path.h"
#include "score/flatbuffers/load_buffer.hpp"

#include "flatbuffers/verifier.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// This file implements ConfigurationFlatbufferParsingStrategy by walking the FlatBuffer accessor tree generated
// from mw_com_config.fbs, mirroring configuration_json_parsing_strategy.cpp helper-for-helper so the two
// strategies stay easy to diff. All strings extracted from the buffer are copied via `->str()` into owning
// std::string objects before they cross a helper boundary: the underlying byte buffer (owned by the caller of
// Parse(path), a local std::vector) does not outlive a single call to Parse(), so retaining a view into it would
// dangle.
namespace score::mw::com::impl::configuration
{
namespace
{

namespace fbs = score::mw::com::impl::configuration::fbs;

using NumberOfIpcTracingSlots_t = std::uint8_t;

/// \brief Converts a flatbuffers::Optional<T> (used for scalars declared "= null" in the .fbs) to std::optional<T>.
template <typename T>
auto ToOptional(::flatbuffers::Optional<T> value) -> std::optional<T>
{
    if (value.has_value())
    {
        return value.value();
    }
    return std::nullopt;
}

/// \brief Range-checks a wider FlatBuffers scalar down to the narrower type the C++ configuration model uses,
/// aborting (like the rest of this file) if the buffer contains an out-of-range value.
template <typename Narrow, typename Wide>
auto NarrowOrFatal(Wide value, std::string_view field_name) -> Narrow
{
    static_assert(std::is_integral<Wide>::value && std::is_integral<Narrow>::value, "Narrow/Wide must be integral");
    const auto narrowed = static_cast<Narrow>(value);
    // The round trip alone misses sign flips, e.g. std::uint32_t 0x80000000 -> std::int32_t -> std::uint32_t.
    if ((static_cast<Wide>(narrowed) != value) || ((narrowed < Narrow{}) != (value < Wide{})))
    {
        score::mw::log::LogFatal("lola") << "Value of '" << field_name
                                         << "' does not fit into its expected type. "
                                            "Configuration corrupted, check with json schema. Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return narrowed;
}

template <typename Narrow, typename Wide>
auto NarrowOrFatal(std::optional<Wide> value, std::string_view field_name) -> std::optional<Narrow>
{
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return NarrowOrFatal<Narrow>(value.value(), field_name);
}

auto RequireNotNull(const void* pointer, std::string_view what) -> void
{
    if (pointer == nullptr)
    {
        score::mw::log::LogFatal("lola") << "Configuration corrupted: missing " << what << ". Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

template <typename FbsAccessorType>
auto RequireNotNull(const FbsAccessorType* pointer, std::string_view what) -> const FbsAccessorType&
{
    RequireNotNull(static_cast<const void*>(pointer), what);
    return *pointer;
}

/// \brief Unwraps a required scalar (declared "= null" in the .fbs so that an omitted key is detectable), aborting
/// if it is absent.
template <typename T>
auto RequireValue(::flatbuffers::Optional<T> value, std::string_view what) -> T
{
    if (!value.has_value())
    {
        score::mw::log::LogFatal("lola") << "Configuration corrupted: missing " << what << ". Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return value.value();
}

auto CopyString(const ::flatbuffers::String* str, std::string_view what) -> std::string
{
    RequireNotNull(static_cast<const void*>(str), what);
    return str->str();
}

auto ParseVersion(const fbs::ServiceVersion& version) -> std::pair<std::uint32_t, std::uint32_t>
{
    return {RequireValue(version.major(), "version.major"), RequireValue(version.minor(), "version.minor")};
}

auto ParseServiceTypeIdentifier(const std::string& service_type_name, const fbs::ServiceVersion& version)
    -> ServiceIdentifierType
{
    const auto& [major, minor] = ParseVersion(version);
    return make_ServiceIdentifierType(service_type_name, major, minor);
}

auto ParseAsilLevel(fbs::AsilLevel asil_level) -> QualityType
{
    switch (asil_level)
    {
        case fbs::AsilLevel::QM:
            return QualityType::kASIL_QM;
        case fbs::AsilLevel::B:
            return QualityType::kASIL_B;
        default:  // LCOV_EXCL_LINE defensive programming
            score::mw::log::LogFatal("lola") << "Invalid ASIL level. Terminating.";  // LCOV_EXCL_LINE
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);                              // LCOV_EXCL_LINE
            return QualityType::kInvalid;                                           // LCOV_EXCL_LINE
    }
}

auto ParseShmSizeCalcMode(fbs::ShmSizeCalcMode mode) -> ShmSizeCalculationMode
{
    switch (mode)
    {
        case fbs::ShmSizeCalcMode::SIMULATION:
            return ShmSizeCalculationMode::kSimulation;
        case fbs::ShmSizeCalcMode::ANALYSIS:
            return ShmSizeCalculationMode::kAnalysis;
        default:  // LCOV_EXCL_LINE defensive programming
            score::mw::log::LogFatal("lola") << "Unknown shm-size-calc-mode. Terminating.";  // LCOV_EXCL_LINE
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);                                      // LCOV_EXCL_LINE
            return ShmSizeCalculationMode::kSimulation;                                     // LCOV_EXCL_LINE
    }
}

auto ParseAllowedUser(const ::flatbuffers::Vector<std::uint32_t>* qm_ids,
                     const ::flatbuffers::Vector<std::uint32_t>* b_ids)
    -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    std::unordered_map<QualityType, std::vector<uid_t>> user_map{};
    const auto add_if_present = [&user_map](const ::flatbuffers::Vector<std::uint32_t>* ids, QualityType quality) {
        if (ids == nullptr)
        {
            return;
        }
        std::vector<uid_t> user_ids{};
        user_ids.reserve(ids->size());
        for (const auto user_id : *ids)
        {
            user_ids.push_back(static_cast<uid_t>(user_id));
        }
        user_map[quality] = std::move(user_ids);
    };
    add_if_present(qm_ids, QualityType::kASIL_QM);
    add_if_present(b_ids, QualityType::kASIL_B);
    return user_map;
}

auto ParseAllowedConsumer(const fbs::AllowedConsumer* allowed_consumer)
    -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    if (allowed_consumer == nullptr)
    {
        return {};
    }
    return ParseAllowedUser(allowed_consumer->QM(), allowed_consumer->B());
}

auto ParseAllowedProvider(const fbs::AllowedProvider* allowed_provider)
    -> std::unordered_map<QualityType, std::vector<uid_t>>
{
    if (allowed_provider == nullptr)
    {
        return {};
    }
    return ParseAllowedUser(allowed_provider->QM(), allowed_provider->B());
}

auto ParseLolaEventInstanceDeployment(const fbs::ServiceInstanceBinding& deployment, LolaServiceInstanceDeployment& service)
    -> void
{
    const auto* events = deployment.events();
    if (events == nullptr)
    {
        return;
    }
    for (const auto* event : *events)
    {
        RequireNotNull(event, "an event instance");
        auto event_name = CopyString(event->eventName(), "event name");

        const auto number_of_sample_slots = NarrowOrFatal<LolaEventInstanceDeployment::SampleSlotCountType>(
            ToOptional(event->numberOfSampleSlots()), "numberOfSampleSlots");
        const auto max_subscribers = NarrowOrFatal<LolaEventInstanceDeployment::SubscriberCountType>(
            ToOptional(event->maxSubscribers()), "maxSubscribers");
        const auto number_of_tracing_slots =
            NarrowOrFatal<NumberOfIpcTracingSlots_t>(event->numberOfIpcTracingSlots(), "numberOfIpcTracingSlots");

        auto event_deployment = LolaEventInstanceDeployment(number_of_sample_slots,
                                                            max_subscribers,
                                                            static_cast<std::uint8_t>(1U),
                                                            event->enforceMaxSamples(),
                                                            number_of_tracing_slots);

        EmplaceOrFatal(service.events_, std::move(event_name), event_deployment, "An event instance");
    }
}

auto ParseLolaFieldInstanceDeployment(const fbs::ServiceInstanceBinding& deployment, LolaServiceInstanceDeployment& service)
    -> void
{
    const auto* fields = deployment.fields();
    if (fields == nullptr)
    {
        return;
    }
    for (const auto* field : *fields)
    {
        RequireNotNull(field, "a field instance");
        auto field_name = CopyString(field->fieldName(), "field name");

        const auto number_of_sample_slots = NarrowOrFatal<LolaEventInstanceDeployment::SampleSlotCountType>(
            ToOptional(field->numberOfSampleSlots()), "numberOfSampleSlots");
        const auto max_subscribers = NarrowOrFatal<LolaEventInstanceDeployment::SubscriberCountType>(
            ToOptional(field->maxSubscribers()), "maxSubscribers");
        const auto number_of_tracing_slots =
            NarrowOrFatal<NumberOfIpcTracingSlots_t>(field->numberOfIpcTracingSlots(), "numberOfIpcTracingSlots");

        auto field_deployment =
            LolaFieldInstanceDeployment(LolaEventInstanceDeployment(number_of_sample_slots,
                                                                    max_subscribers,
                                                                    static_cast<std::uint8_t>(1U),
                                                                    field->enforceMaxSamples(),
                                                                    number_of_tracing_slots),
                                        field->useGetIfAvailable(),
                                        field->useSetIfAvailable());
        EmplaceOrFatal(service.fields_, std::move(field_name), field_deployment, "A field instance");
    }
}

auto ParseLolaMethodInstanceDeployment(const fbs::ServiceInstanceBinding& deployment, LolaServiceInstanceDeployment& service)
    -> void
{
    const auto* methods = deployment.methods();
    if (methods == nullptr)
    {
        return;
    }
    for (const auto* method : *methods)
    {
        RequireNotNull(method, "a method instance");
        auto method_name = CopyString(method->methodName(), "method name");
        const auto queue_size = ToOptional(method->queueSize());
        const LolaMethodInstanceDeployment method_deployment{queue_size, method->use()};
        EmplaceOrFatal(service.methods_, std::move(method_name), method_deployment, "A method instance");
    }
}

auto ParseServiceElementTracingEnabled(const fbs::ServiceInstanceBinding& deployment,
                                       TracingConfiguration& tracing_configuration,
                                       const std::string_view service_type_name_view,
                                       const InstanceSpecifier& instance_specifier,
                                       const ServiceElementType service_element_type) -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        (service_element_type == ServiceElementType::EVENT) || (service_element_type == ServiceElementType::FIELD),
        "Only FIELD or EVENT are allowed as ServiceElementTypes.");

    // A small local visitor covers both InstanceEvent and InstanceField without duplicating this loop, since both
    // generated types expose the same eventName()/fieldName() + numberOfIpcTracingSlots() shape under different
    // accessor names.
    const auto handle = [&](const auto* name_string, const NumberOfIpcTracingSlots_t number_of_tracing_slots) {
        if (number_of_tracing_slots > 0U)
        {
            auto service_element_name = CopyString(name_string, "service element name");
            std::string service_type_name{service_type_name_view.data(), service_type_name_view.size()};
            tracing::ServiceElementIdentifier service_element_identifier{
                std::move(service_type_name), std::move(service_element_name), service_element_type};
            tracing_configuration.SetServiceElementTracingEnabled(std::move(service_element_identifier),
                                                                  instance_specifier);
        }
    };

    if (service_element_type == ServiceElementType::EVENT)
    {
        const auto* events = deployment.events();
        if (events == nullptr)
        {
            return;
        }
        for (const auto* event : *events)
        {
            RequireNotNull(event, "an event instance");
            handle(event->eventName(), event->numberOfIpcTracingSlots());
        }
    }
    else
    {
        const auto* fields = deployment.fields();
        if (fields == nullptr)
        {
            return;
        }
        for (const auto* field : *fields)
        {
            RequireNotNull(field, "a field instance");
            handle(field->fieldName(), field->numberOfIpcTracingSlots());
        }
    }
}

auto ParseLolaServiceInstanceDeployment(const fbs::ServiceInstanceBinding& deployment) -> LolaServiceInstanceDeployment
{
    LolaServiceInstanceDeployment service{};

    service.shared_memory_size_ = ToOptional(deployment.shm_size());
    service.control_asil_b_memory_size_ = ToOptional(deployment.control_asil_b_shm_size());
    service.control_qm_memory_size_ = ToOptional(deployment.control_qm_shm_size());

    const auto instance_id = NarrowOrFatal<LolaServiceInstanceId::InstanceId>(
        ToOptional(deployment.instanceId()), "instanceId");
    if (instance_id.has_value())
    {
        service.instance_id_ = LolaServiceInstanceId{instance_id.value()};
    }

    service.inter_vm_support_ = deployment.interVmSupport();
    // No need to check that binding is SHM for now, because this method is only called if binding is SHM.
    service.inter_vm_forwarded_ = deployment.interVmForwarded();

    if (service.inter_vm_forwarded_)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            service.inter_vm_support_,
            "Configuration corrupted: Service instance is interVmForwarded but is not "
            "configured for interVmSupport");
    }

    ParseLolaEventInstanceDeployment(deployment, service);
    ParseLolaFieldInstanceDeployment(deployment, service);
    ParseLolaMethodInstanceDeployment(deployment, service);

    service.strict_permissions_ = deployment.permission_checks() == fbs::PermissionChecks::strict;

    service.allowed_consumer_ = ParseAllowedConsumer(deployment.allowedConsumer());
    service.allowed_provider_ = ParseAllowedProvider(deployment.allowedProvider());

    return service;
}

auto ParseServiceInstanceDeployments(const fbs::ServiceInstance& service_instance,
                                     TracingConfiguration& tracing_configuration,
                                     const ServiceIdentifierType& service,
                                     const InstanceSpecifier& instance_specifier)
    -> std::vector<ServiceInstanceDeployment>
{
    const auto& deployment_instances = RequireNotNull(service_instance.instances(), "serviceInstance.instances");

    std::vector<ServiceInstanceDeployment> deployments{};
    for (const auto* deployment : deployment_instances)
    {
        RequireNotNull(deployment, "a service instance deployment");
        const auto asil_level = ParseAsilLevel(RequireValue(deployment->asil_level(), "asil-level"));

        switch (RequireValue(deployment->binding(), "binding"))
        {
            case fbs::Binding::SHM:
                score::cpp::ignore = deployments.emplace_back(
                    service, ParseLolaServiceInstanceDeployment(*deployment), asil_level, instance_specifier);
                break;
            default:  // LCOV_EXCL_LINE defensive programming
                score::mw::log::LogFatal("lola") << "Unknown binding provided. Required argument.";  // LCOV_EXCL_LINE
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);                                          // LCOV_EXCL_LINE
                break;                                                                               // LCOV_EXCL_LINE
        }

        if (tracing_configuration.IsTracingEnabled())
        {
            constexpr auto EVENT = ServiceElementType::EVENT;
            constexpr auto FIELD = ServiceElementType::FIELD;
            const auto service_name = service.ToString();
            ParseServiceElementTracingEnabled(*deployment, tracing_configuration, service_name, instance_specifier, EVENT);
            ParseServiceElementTracingEnabled(*deployment, tracing_configuration, service_name, instance_specifier, FIELD);
        }
    }
    return deployments;
}

auto ParseServiceInstances(const fbs::Configuration& configuration, TracingConfiguration& tracing_configuration)
    -> Configuration::ServiceInstanceDeployments
{
    const auto& service_instances = RequireNotNull(configuration.serviceInstances(), "serviceInstances");

    Configuration::ServiceInstanceDeployments service_instance_deployments{};
    for (const auto* service_instance : service_instances)
    {
        RequireNotNull(service_instance, "a service instance");
        auto instance_specifier =
            CreateValidInstanceSpecifier(CopyString(service_instance->instanceSpecifier(), "instanceSpecifier"));
        auto service_type_name = CopyString(service_instance->serviceTypeName(), "serviceTypeName");
        const auto& version = RequireNotNull(service_instance->version(), "version");
        auto service_identifier = ParseServiceTypeIdentifier(service_type_name, version);

        auto instance_deployments =
            ParseServiceInstanceDeployments(*service_instance, tracing_configuration, service_identifier, instance_specifier);
        ValidateSingleDeployment(instance_deployments, service_identifier);

        EmplaceOrFatal(service_instance_deployments,
                       instance_specifier,
                       instance_deployments.at(0U),
                       "Service instance deployment");
    }
    return service_instance_deployments;
}

void ParseLolaEventTypeDeployments(const fbs::ServiceTypeBinding& binding, LolaServiceTypeDeployment& service)
{
    const auto* events = binding.events();
    if (events == nullptr)
    {
        return;
    }
    for (const auto* event : *events)
    {
        RequireNotNull(event, "an event");
        auto event_name = CopyString(event->eventName(), "eventName");
        const auto event_id = NarrowOrFatal<LolaEventId>(RequireValue(event->eventId(), "eventId"), "eventId");
        EmplaceOrFatal(service.events_, std::move(event_name), event_id, "An event");
    }
}

void ParseLolaFieldTypeDeployments(const fbs::ServiceTypeBinding& binding, LolaServiceTypeDeployment& service)
{
    const auto* fields = binding.fields();
    if (fields == nullptr)
    {
        return;
    }
    for (const auto* field : *fields)
    {
        RequireNotNull(field, "a field");
        auto field_name = CopyString(field->fieldName(), "fieldName");
        const auto field_id = NarrowOrFatal<LolaFieldId>(RequireValue(field->fieldId(), "fieldId"), "fieldId");
        EmplaceOrFatal(service.fields_, std::move(field_name), field_id, "A field");
    }
}

void ParseLolaMethodTypeDeployments(const fbs::ServiceTypeBinding& binding, LolaServiceTypeDeployment& service)
{
    const auto* methods = binding.methods();
    if (methods == nullptr)
    {
        return;
    }
    for (const auto* method : *methods)
    {
        RequireNotNull(method, "a method");
        auto method_name = CopyString(method->methodName(), "methodName");
        const auto method_id = NarrowOrFatal<LolaMethodId>(RequireValue(method->methodId(), "methodId"), "methodId");
        EmplaceOrFatal(service.methods_, std::move(method_name), method_id, "A method");
    }
}

auto ParseLoLaServiceTypeDeployments(const fbs::ServiceTypeBinding& binding) -> LolaServiceTypeDeployment
{
    LolaServiceTypeDeployment lola{NarrowOrFatal<LolaServiceId>(RequireValue(binding.serviceId(), "serviceId"), "serviceId")};
    ParseLolaEventTypeDeployments(binding, lola);
    ParseLolaFieldTypeDeployments(binding, lola);
    ParseLolaMethodTypeDeployments(binding, lola);
    ValidateUniqueServiceElementIds(lola);
    return lola;
}

auto ParseServiceTypeDeployment(const fbs::ServiceType& service_type) -> ServiceTypeDeployment
{
    const auto& bindings = RequireNotNull(service_type.bindings(), "bindings");
    for (const auto* binding : bindings)
    {
        RequireNotNull(binding, "a service type binding");
        switch (RequireValue(binding->binding(), "binding"))
        {
            case fbs::Binding::SHM:
                return ServiceTypeDeployment{ParseLoLaServiceTypeDeployments(*binding)};
            default:  // LCOV_EXCL_LINE defensive programming
                score::mw::log::LogFatal("lola") << "No unknown binding provided. Required argument.";  // LCOV_EXCL_LINE
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);                                             // LCOV_EXCL_LINE
                break;                                                                                  // LCOV_EXCL_LINE
        }
    }
    return ServiceTypeDeployment{score::cpp::blank{}};
}

auto ParseServiceTypes(const fbs::Configuration& configuration) -> Configuration::ServiceTypeDeployments
{
    const auto& service_types = RequireNotNull(configuration.serviceTypes(), "serviceTypes");

    Configuration::ServiceTypeDeployments service_type_deployments{};
    for (const auto* service_type : service_types)
    {
        RequireNotNull(service_type, "a service type");
        auto service_type_name = CopyString(service_type->serviceTypeName(), "serviceTypeName");
        const auto& version = RequireNotNull(service_type->version(), "version");
        const auto service_identifier = ParseServiceTypeIdentifier(service_type_name, version);

        const auto service_deployment = ParseServiceTypeDeployment(*service_type);
        EmplaceOrFatal(service_type_deployments, service_identifier, service_deployment, "Service Type");
    }
    return service_type_deployments;
}

auto ParseGlobalProperties(const fbs::Configuration& configuration) -> GlobalConfiguration
{
    GlobalConfiguration global_configuration{};
    const auto* global = configuration.global();
    if (global == nullptr)
    {
        global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);
        return global_configuration;
    }

    global_configuration.SetProcessAsilLevel(ParseAsilLevel(global->asil_level()));

    const auto* queue_size = global->queue_size();
    if (queue_size != nullptr)
    {
        global_configuration.SetReceiverMessageQueueSize(
            QualityType::kASIL_QM, NarrowOrFatal<std::int32_t>(queue_size->QM_receiver(), "queue-size.QM-receiver"));
        global_configuration.SetReceiverMessageQueueSize(
            QualityType::kASIL_B, NarrowOrFatal<std::int32_t>(queue_size->B_receiver(), "queue-size.B-receiver"));
        global_configuration.SetSenderMessageQueueSize(
            NarrowOrFatal<std::int32_t>(queue_size->B_sender(), "queue-size.B-sender"));
    }

    global_configuration.SetShmSizeCalcMode(ParseShmSizeCalcMode(global->shm_size_calc_mode()));

    const auto application_id = ToOptional(global->applicationID());
    if (application_id.has_value())
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            application_id.value() != std::numeric_limits<GlobalConfiguration::ApplicationId>::max(),
            "Configuration corrupted, check with json schema");
        global_configuration.SetApplicationId(application_id.value());
    }

    return global_configuration;
}

auto ParseTracingProperties(const fbs::Configuration& configuration) -> TracingConfiguration
{
    TracingConfiguration tracing_configuration{};
    const auto* tracing = configuration.tracing();
    if (tracing != nullptr)
    {
        tracing_configuration.SetTracingEnabled(tracing->enable());
        tracing_configuration.SetApplicationInstanceID(
            CopyString(tracing->applicationInstanceID(), "tracing.applicationInstanceID"));

        const auto* trace_filter_config_path = tracing->traceFilterConfigPath();
        if (trace_filter_config_path != nullptr)
        {
            tracing_configuration.SetTracingTraceFilterConfigPath(trace_filter_config_path->str());
        }
        else
        {
            tracing_configuration.SetTracingTraceFilterConfigPath("./etc/mw_com_trace_filter.json");
        }
    }
    return tracing_configuration;
}

}  // namespace

Configuration ConfigurationFlatbufferParsingStrategy::Parse(const std::string_view path) const
{
    const std::string owned_path{path};
    // Reason for banning is AoU of vaJson library about integrity of provided path. This AoU is forwarded as AoU of
    // Lola. See ScoreReq.AoU ConfigOnASafeFilesystem.
    // NOLINTNEXTLINE(score-banned-function): The user has to guarantee the integrity of the path
    auto buffer_result = score::flatbuffers::LoadBuffer(score::filesystem::Path{owned_path});
    if (!buffer_result.has_value())
    {
        ::score::mw::log::LogFatal("lola") << "Parsing config file" << path
                                           << "failed with error:" << buffer_result.error().ToString()
                                           << " . Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    const auto& buffer = buffer_result.value();
    return Parse(score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()});
}

Configuration ConfigurationFlatbufferParsingStrategy::Parse(score::cpp::span<const std::uint8_t> buffer) const
{
    ::flatbuffers::Verifier verifier{buffer.data(), static_cast<std::size_t>(buffer.size())};
    if (!fbs::VerifyConfigurationBuffer(verifier))
    {
        ::score::mw::log::LogFatal("lola") << "FlatBuffer structural verification failed. Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }

    const auto* config = fbs::GetConfiguration(buffer.data());
    RequireNotNull(config, "root Configuration table");

    auto tracing_configuration = ParseTracingProperties(*config);
    auto service_type_deployments = ParseServiceTypes(*config);
    auto service_instance_deployments = ParseServiceInstances(*config, tracing_configuration);
    auto global_configuration = ParseGlobalProperties(*config);

    return Configuration{std::move(service_type_deployments),
                        std::move(service_instance_deployments),
                        std::move(global_configuration),
                        std::move(tracing_configuration)};
}

}  // namespace score::mw::com::impl::configuration

#else  // !defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)

namespace score::mw::com::impl::configuration
{
namespace
{

[[noreturn]] auto FlatbuffersConfigurationDisabled() -> void
{
    ::score::mw::log::LogFatal("lola")
        << "mw::com was built without FlatBuffers configuration support "
           "(--config=flatbuffers / //score/mw/com/flags:experimental_enable_flatbuffers_configuration). "
           "Terminating.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    // LCOV_EXCL_START defensive programming: SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD above never returns.
    std::terminate();
    // LCOV_EXCL_STOP
}

}  // namespace

Configuration ConfigurationFlatbufferParsingStrategy::Parse(const std::string_view /* path */) const
{
    FlatbuffersConfigurationDisabled();
}

Configuration ConfigurationFlatbufferParsingStrategy::Parse(score::cpp::span<const std::uint8_t> /* buffer */) const
{
    FlatbuffersConfigurationDisabled();
}

}  // namespace score::mw::com::impl::configuration

#endif  // defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)
