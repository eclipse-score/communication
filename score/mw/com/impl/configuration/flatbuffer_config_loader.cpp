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

#include "score/mw/com/impl/configuration/flatbuffer_config_loader.h"

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_version_type.h"
#include "score/mw/com/impl/configuration/shm_size_calc_mode.h"

#include "score/mw/com/impl/instance_specifier.h"

#include "score/mw/log/logging.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace score::mw::com::impl::configuration
{

void FlatBufferConfigLoader::MmapDeleter::operator()(void* ptr) const noexcept
{
    if (ptr && size > 0)
    {
        munmap(ptr, size);
    }
}

namespace
{

// Helper to convert FlatBuffer AsilLevel to QualityType
QualityType ConvertAsilLevel(AsilLevel asil_level) noexcept
{
    return (asil_level == AsilLevel::B) ? QualityType::kASIL_B : QualityType::kASIL_QM;
}

// Helper to parse event deployments from FlatBuffer Instance
LolaServiceInstanceDeployment::EventInstanceMapping ParseEventDeployments(const Instance* instance)
{
    LolaServiceInstanceDeployment::EventInstanceMapping events;

    if (instance->events() != nullptr)
    {
        for (const auto* event : *instance->events())
        {
            if (event != nullptr)
            {
                std::optional<uint16_t> slots = event->number_of_sample_slots() > 0
                                                    ? std::optional<uint16_t>(event->number_of_sample_slots())
                                                    : std::nullopt;
                std::optional<uint8_t> max_subs =
                    event->max_subscribers() > 0
                        ? std::optional<uint8_t>(static_cast<uint8_t>(event->max_subscribers()))
                        : std::nullopt;

                LolaEventInstanceDeployment event_deploy(slots,
                                                         max_subs,
                                                         std::nullopt,
                                                         event->enforce_max_samples(),
                                                         static_cast<uint8_t>(event->number_of_ipc_tracing_slots()));
                // event_name is obliged to contain a value (marked as required)
                events.emplace(event->event_name()->str(), std::move(event_deploy));
            }
        }
    }

    return events;
}

// Helper to parse field deployments from FlatBuffer Instance
LolaServiceInstanceDeployment::FieldInstanceMapping ParseFieldDeployments(const Instance* instance)
{
    LolaServiceInstanceDeployment::FieldInstanceMapping fields;

    if (instance->fields() != nullptr)
    {
        for (const auto* field : *instance->fields())
        {
            if (field != nullptr)
            {
                std::optional<uint16_t> slots = field->number_of_sample_slots() > 0
                                                    ? std::optional<uint16_t>(field->number_of_sample_slots())
                                                    : std::nullopt;
                std::optional<uint8_t> max_subs =
                    field->max_subscribers() > 0
                        ? std::optional<uint8_t>(static_cast<uint8_t>(field->max_subscribers()))
                        : std::nullopt;

                LolaFieldInstanceDeployment field_deploy(slots,
                                                         max_subs,
                                                         std::nullopt,
                                                         field->enforce_max_samples(),
                                                         static_cast<uint8_t>(field->number_of_ipc_tracing_slots()));
                // field_name is obliged to contain a value (marked as required)
                fields.emplace(field->field_name()->str(), std::move(field_deploy));
            }
        }
    }

    return fields;
}

// Helper to parse method deployments from FlatBuffer Instance
LolaServiceInstanceDeployment::MethodInstanceMapping ParseMethodDeployments(const Instance* instance)
{
    LolaServiceInstanceDeployment::MethodInstanceMapping methods;

    if (instance->methods() != nullptr)
    {
        for (const auto* method : *instance->methods())
        {
            if (method != nullptr)
            {
                std::optional<LolaMethodInstanceDeployment::QueueSize> queue_size = std::nullopt;
                if (method->queue_size() > 0)
                {
                    queue_size = static_cast<uint8_t>(method->queue_size());
                }

                LolaMethodInstanceDeployment method_deploy(queue_size);
                // method_name is obliged to contain a value (marked as required)
                methods.emplace(method->method_name()->str(), std::move(method_deploy));
            }
        }
    }

    return methods;
}

// Helper to parse permission mappings from FlatBuffer Permissions (AllowedConsumer or AllowedProvider)
template <typename PermissionsType>
std::unordered_map<QualityType, std::vector<uid_t>> ParsePermissions(const PermissionsType* permissions)
{
    std::unordered_map<QualityType, std::vector<uid_t>> permission_map;

    if (permissions != nullptr)
    {
        if (permissions->qm() != nullptr)
        {
            std::vector<uid_t> qm_users(permissions->qm()->begin(), permissions->qm()->end());
            permission_map.emplace(QualityType::kASIL_QM, std::move(qm_users));
        }
        if (permissions->b() != nullptr)
        {
            std::vector<uid_t> b_users(permissions->b()->begin(), permissions->b()->end());
            permission_map.emplace(QualityType::kASIL_B, std::move(b_users));
        }
    }

    return permission_map;
}

// Helper to set optional memory sizes on deployment
void SetMemorySizes(LolaServiceInstanceDeployment& deployment, const Instance* instance)
{
    if (instance->shm_size() > 0)
    {
        deployment.shared_memory_size_ = static_cast<std::size_t>(instance->shm_size());
    }
    if (instance->control_asil_b_shm_size() > 0)
    {
        deployment.control_asil_b_memory_size_ = static_cast<std::size_t>(instance->control_asil_b_shm_size());
    }
    if (instance->control_qm_shm_size() > 0)
    {
        deployment.control_qm_memory_size_ = static_cast<std::size_t>(instance->control_qm_shm_size());
    }
}

// Helper to create LolaServiceInstanceDeployment from FlatBuffer Instance
score::mw::com::impl::LolaServiceInstanceDeployment CreateLolaServiceInstanceDeployment(const Instance* instance)
{
    score::cpp::optional<LolaServiceInstanceId> instance_id;
    if (instance->instance_id() != 0)
    {
        instance_id = LolaServiceInstanceId{instance->instance_id()};
    }

    auto events = ParseEventDeployments(instance);
    auto fields = ParseFieldDeployments(instance);
    auto methods = ParseMethodDeployments(instance);
    auto allowed_consumer = ParsePermissions(instance->allowed_consumer());
    auto allowed_provider = ParsePermissions(instance->allowed_provider());

    bool strict_permission = (instance->permission_checks() == PermissionCheckStrategy::STRICT);

    LolaServiceInstanceDeployment deployment(instance_id,
                                             std::move(events),
                                             std::move(fields),
                                             std::move(methods),
                                             strict_permission,
                                             std::move(allowed_consumer),
                                             std::move(allowed_provider));

    SetMemorySizes(deployment, instance);

    return deployment;
}

}  // namespace

Configuration FlatBufferConfigLoader::CreateConfiguration(std::string_view path) noexcept
{
    FlatBufferConfigLoader loader(path);

    return score::mw::com::impl::Configuration(loader.CreateServiceTypes(),
                                               loader.CreateServiceInstances(),
                                               loader.CreateGlobalConfiguration(),
                                               loader.CreateTracingConfiguration());
}

FlatBufferConfigLoader::FlatBufferConfigLoader(std::string_view path) : fd_(-1), com_config_(nullptr)
{
    LoadBuffer(path);
}

FlatBufferConfigLoader::~FlatBufferConfigLoader()
{
    if (fd_ != -1)
    {
        close(fd_);
    }
}

void FlatBufferConfigLoader::LoadBuffer(std::string_view path)
{
    fd_ = open(std::string(path).c_str(), O_RDONLY);
    if (fd_ == -1)
    {
        ::score::mw::log::LogFatal("lola")
            << "Failed to open FlatBuffer file: " << path << " (" << std::string_view(std::strerror(errno)) << ")";
        std::terminate();
    }

    // Get file size
    struct stat st;
    if (fstat(fd_, &st) == -1)
    {
        ::score::mw::log::LogFatal("lola")
            << "Failed to stat FlatBuffer file: " << path << " (" << std::string_view(std::strerror(errno)) << ")";
        std::terminate();
    }

    if (st.st_size == 0)
    {
        ::score::mw::log::LogFatal("lola") << "FlatBuffer file is empty: " << path;
        std::terminate();
    }

    std::size_t mapped_size = static_cast<std::size_t>(st.st_size);

    // Map file into memory
    void* ptr = mmap(nullptr, mapped_size, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (ptr == MAP_FAILED)
    {
        ::score::mw::log::LogFatal("lola")
            << "Failed to mmap FlatBuffer file: " << path << " (" << std::string_view(std::strerror(errno)) << ")";
        std::terminate();
    }
    mapped_ptr_ = std::unique_ptr<void, MmapDeleter>(ptr, MmapDeleter{mapped_size});

    // Verify the FlatBuffer using the mapped region
    const uint8_t* data = reinterpret_cast<const uint8_t*>(mapped_ptr_.get());
    flatbuffers::Verifier verifier(data, mapped_size);
    if (!VerifyComConfigurationBuffer(verifier))
    {
        ::score::mw::log::LogFatal("lola") << "FlatBuffer verification failed for: " << path;
        std::terminate();
    }

    // Get the root ComConfiguration (points into the mmap'd region)
    com_config_ = GetComConfiguration(data);
    if (com_config_ == nullptr)
    {
        ::score::mw::log::LogFatal("lola") << "Failed to get ComConfiguration from buffer: " << path;
        std::terminate();
    }
}

Configuration::ServiceTypeDeployments FlatBufferConfigLoader::CreateServiceTypes() const noexcept
{
    Configuration::ServiceTypeDeployments service_type_deployments;

    // service_types is obliged to contain a value (marked as required)
    for (const auto* service_type : *com_config_->service_types())
    {
        const auto* version = service_type->version();
        if (version == nullptr)
        {
            ::score::mw::log::LogFatal("lola") << "Service type missing version. Terminating";
            std::terminate();
        }

        // service_type_name is obliged to contain a value (marked as required)
        auto service_identifier =
            make_ServiceIdentifierType(service_type->service_type_name()->str(), version->major(), version->minor());

        ServiceTypeDeployment::BindingInformation binding_info = score::cpp::blank{};

        // bindings is obliged to contain a value (marked as required)
        // Process all bindings - currently only SHM is supported, use the first SHM binding found
        for (const auto* binding : *service_type->bindings())
        {
            if (binding->binding() == BindingType::SHM)
            {
                // Create LolaServiceTypeDeployment (SHM binding)
                LolaServiceId service_id{binding->service_id()};
                LolaServiceTypeDeployment::EventIdMapping events;
                LolaServiceTypeDeployment::FieldIdMapping fields;
                LolaServiceTypeDeployment::MethodIdMapping methods;

                if (binding->events() != nullptr)
                {
                    for (const auto* event : *binding->events())
                    {
                        // event_name is obliged to contain a value (marked as required)
                        events.emplace(event->event_name()->str(),
                                       LolaEventId{static_cast<uint8_t>(event->event_id())});
                    }
                }

                if (binding->fields() != nullptr)
                {
                    for (const auto* field : *binding->fields())
                    {
                        // field_name is obliged to contain a value (marked as required)
                        fields.emplace(field->field_name()->str(),
                                       LolaFieldId{static_cast<uint8_t>(field->field_id())});
                    }
                }

                if (binding->methods() != nullptr)
                {
                    for (const auto* method : *binding->methods())
                    {
                        // method_name is obliged to contain a value (marked as required)
                        methods.emplace(method->method_name()->str(),
                                        LolaMethodId{static_cast<uint8_t>(method->method_id())});
                    }
                }

                binding_info =
                    LolaServiceTypeDeployment{service_id, std::move(events), std::move(fields), std::move(methods)};
                break;  // Use first SHM binding
            }
            else if (binding->binding() == BindingType::SOME_IP)
            {
                // Skip SOME/IP - not supported yet
                ::score::mw::log::LogFatal("lola") << "Provided SOME/IP binding, which is not supported yet.";
                std::terminate();
            }
            else
            {
                ::score::mw::log::LogFatal("lola") << "Unknown binding type provided. Required argument.";
                std::terminate();
            }
        }

        if (binding_info == score::cpp::blank{})
        {
            ::score::mw::log::LogFatal("lola")
                << "No SHM binding found for Service Type: " << service_identifier.ToString();
            std::terminate();
        }

        ServiceTypeDeployment service_deployment{binding_info};

        const auto inserted = service_type_deployments.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(service_identifier),
                                                               std::forward_as_tuple(std::move(service_deployment)));

        if (!inserted.second)
        {
            ::score::mw::log::LogFatal("lola") << "Service Type was deployed twice in FlatBuffer";
            std::terminate();
        }
    }

    return service_type_deployments;
}

Configuration::ServiceInstanceDeployments FlatBufferConfigLoader::CreateServiceInstances() const noexcept
{
    Configuration::ServiceInstanceDeployments service_instances;

    // service_instances is obliged to contain a value (marked as required)
    for (const auto* service_instance : *com_config_->service_instances())
    {
        // service_instance and instance_specifier are obliged to contain a value (marked as required)
        auto instance_spec_result = InstanceSpecifier::Create(service_instance->instance_specifier()->str());
        if (!instance_spec_result)
        {
            ::score::mw::log::LogFatal("lola") << "Invalid instance specifier in FlatBuffer. Terminating";
            std::terminate();
        }
        InstanceSpecifier instance_spec = std::move(instance_spec_result.value());

        // version and service_type_name are obliged to contain a value (marked as required)
        const auto* version = service_instance->version();
        auto service_identifier = make_ServiceIdentifierType(
            service_instance->service_type_name()->str(), version->major(), version->minor());

        if (service_instance->instances() == nullptr || service_instance->instances()->size() == 0)
        {
            ::score::mw::log::LogFatal("lola") << "Service instance missing deployment instances. Terminating";
            std::terminate();
        }

        // Find the single SHM instance - multi-binding not supported
        const Instance* shm_instance = nullptr;
        for (const auto* instance : *service_instance->instances())
        {
            if (instance->binding() == BindingType::SHM)
            {
                if (shm_instance != nullptr)
                {
                    ::score::mw::log::LogFatal("lola") << "Multiple SHM bindings for " << service_identifier.ToString()
                                                       << ". Multi-Binding not supported";
                    std::terminate();
                }
                shm_instance = instance;
            }
            else if (instance->binding() == BindingType::SOME_IP)
            {
                ::score::mw::log::LogFatal("lola") << "Provided SOME/IP binding, which cannot be parsed.";
                std::terminate();
            }
            else
            {
                ::score::mw::log::LogFatal("lola") << "Unknown binding type provided. Required argument.";
                std::terminate();
            }
        }

        if (shm_instance == nullptr)
        {
            ::score::mw::log::LogFatal("lola") << "No SHM binding found for " << service_identifier.ToString();
            std::terminate();
        }
        QualityType asil_level = ConvertAsilLevel(shm_instance->asil_level());
        ServiceInstanceDeployment::BindingInformation binding_info = CreateLolaServiceInstanceDeployment(shm_instance);
        ServiceInstanceDeployment deployment(service_identifier, std::move(binding_info), asil_level, instance_spec);
        service_instances.emplace(std::move(instance_spec), std::move(deployment));
    }

    return service_instances;
}

GlobalConfiguration FlatBufferConfigLoader::CreateGlobalConfiguration() const noexcept
{
    GlobalConfiguration global_config;

    if (com_config_->global() != nullptr)
    {
        const auto* global = com_config_->global();

        // Set ASIL level
        QualityType asil_level = ConvertAsilLevel(global->asil_level());
        global_config.SetProcessAsilLevel(asil_level);

        // Set application ID if present
        if (global->application_id() != 0)
        {
            global_config.SetApplicationId(global->application_id());
        }

        // Set queue sizes
        if (global->queue_size() != nullptr)
        {
            const auto* queue_size = global->queue_size();
            global_config.SetReceiverMessageQueueSize(QualityType::kASIL_QM,
                                                      static_cast<int32_t>(queue_size->qm_receiver()));
            global_config.SetReceiverMessageQueueSize(QualityType::kASIL_B,
                                                      static_cast<int32_t>(queue_size->b_receiver()));
            global_config.SetSenderMessageQueueSize(static_cast<int32_t>(queue_size->b_sender()));
        }

        // Set SHM size calculation mode.
        // NOTE: SHM size calculation currently only supports the simulation mode.
        //       Therefore, we always use ShmSizeCalculationMode::kSimulation here,
        //       regardless of any potential configuration in the FlatBuffer `global`
        //       object. If additional modes are supported in the future, this code
        //       should be extended to read the mode from the FlatBuffer.
        ShmSizeCalculationMode shm_mode = ShmSizeCalculationMode::kSimulation;
        global_config.SetShmSizeCalcMode(shm_mode);
    }

    return global_config;
}

TracingConfiguration FlatBufferConfigLoader::CreateTracingConfiguration() const noexcept
{
    TracingConfiguration tracing_config;

    if (com_config_->tracing() != nullptr)
    {
        const auto* tracing = com_config_->tracing();

        tracing_config.SetTracingEnabled(tracing->enable());

        // application_instance_id is obliged to contain a value (marked as required)
        tracing_config.SetApplicationInstanceID(tracing->application_instance_id()->str());

        if (tracing->trace_filter_config_path() != nullptr)
        {
            tracing_config.SetTracingTraceFilterConfigPath(tracing->trace_filter_config_path()->str());
        }
        else
        {
            // Default path if not provided
            tracing_config.SetTracingTraceFilterConfigPath("./etc/mw_com_trace_filter.json");
        }
    }

    return tracing_config;
}

}  // namespace score::mw::com::impl::configuration
