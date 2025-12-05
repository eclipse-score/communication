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
#include "score/mw/com/impl/bindings/lola/skeleton.h"

#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/util/arithmetic_utils.h"

#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_resource.h"
#include "score/os/acl.h"
#include "score/os/stat.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <exception>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

const LolaServiceTypeDeployment& GetLolaServiceTypeDeployment(const InstanceIdentifier& identifier) noexcept
{
    const auto& service_type_depl_info = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment_ptr =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    if (lola_service_type_deployment_ptr == nullptr)
    {
        score::mw::log::LogError("lola")
            << "GetLolaServiceTypeDeployment: Wrong Binding! ServiceTypeDeployment doesn't contain a LoLa deployment!";
        std::terminate();
    }
    return *lola_service_type_deployment_ptr;
}

const LolaServiceInstanceDeployment& GetLolaServiceInstanceDeployment(const InstanceIdentifier& identifier) noexcept
{
    const auto& instance_depl_info = InstanceIdentifierView{identifier}.GetServiceInstanceDeployment();
    const auto* lola_service_instance_deployment_ptr =
        std::get_if<LolaServiceInstanceDeployment>(&instance_depl_info.bindingInfo_);
    if (lola_service_instance_deployment_ptr == nullptr)
    {
        score::mw::log::LogError("lola") << "GetLolaServiceInstanceDeployment: Wrong Binding! ServiceInstanceDeployment "
                                          "doesn't contain a LoLa deployment!";
        std::terminate();
    }
    return *lola_service_instance_deployment_ptr;
}

ServiceDataControl* GetServiceDataControlSkeletonSide(const memory::shared::ManagedMemoryResource& control) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule. The rule declares:
    // An object with integer type or pointer to void type shall not be converted to an object with pointer type.
    // The "ServiceDataStorage" type is strongly defined as shared IPC data between Proxy and Skeleton.
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto* const service_data_control = static_cast<ServiceDataControl*>(control.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_data_control != nullptr, "Could not retrieve service data control.");
    return service_data_control;
}

ServiceDataStorage* GetServiceDataStorageSkeletonSide(const memory::shared::ManagedMemoryResource& data) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule. The rule declares:
    // An object with integer type or pointer to void type shall not be converted to an object with pointer type.
    // The "ServiceDataStorage" type is strongly defined as shared IPC data between Proxy and Skeleton.
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto* const service_data_storage = static_cast<ServiceDataStorage*>(data.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_data_storage != nullptr,
                           "Could not retrieve service data storage within shared-memory.");
    return service_data_storage;
}

enum class ShmObjectType : std::uint8_t
{
    kControl_QM = 0x00,
    kControl_ASIL_B = 0x01,
    kData = 0x02,
};

std::uint64_t CalculateMemoryResourceId(const LolaServiceTypeDeployment::ServiceId lola_service_id,
                                        const LolaServiceInstanceId::InstanceId lola_instance_id,
                                        const ShmObjectType object_type) noexcept
{
    return ((static_cast<std::uint64_t>(lola_service_id) << 24U) +
            (static_cast<std::uint64_t>(lola_instance_id) << 8U) + static_cast<std::uint8_t>(object_type));
}

bool CreatePartialRestartDirectory(const score::filesystem::Filesystem& filesystem,
                                   const IPartialRestartPathBuilder& partial_restart_path_builder) noexcept
{
    const auto partial_restart_dir_path = partial_restart_path_builder.GetLolaPartialRestartDirectoryPath();

    constexpr auto permissions =
        os::Stat::Mode::kReadWriteExecUser | os::Stat::Mode::kReadWriteExecGroup | os::Stat::Mode::kReadWriteExecOthers;
    const auto create_dir_result = filesystem.utils->CreateDirectories(partial_restart_dir_path, permissions);
    if (!create_dir_result.has_value())
    {
        score::mw::log::LogError("lola") << create_dir_result.error().Message()
                                       << ":CreateDirectories failed:" << create_dir_result.error().UserMessage();
        return false;
    }
    return true;
}

std::optional<memory::shared::LockFile> CreateOrOpenServiceInstanceExistenceMarkerFile(
    const LolaServiceInstanceId::InstanceId lola_instance_id,
    const IPartialRestartPathBuilder& partial_restart_path_builder) noexcept
{
    auto service_instance_existence_marker_file_path =
        partial_restart_path_builder.GetServiceInstanceExistenceMarkerFilePath(lola_instance_id);

    // The instance existence marker file can be opened in the case that another skeleton of the same service currently
    // exists or that a skeleton of the same service previously crashed. We cannot determine which is true until we try
    // to flock the file. Therefore, we do not take ownership on construction and take ownership later if we can
    // exclusively flock the file.
    bool take_ownership{false};
    return memory::shared::LockFile::CreateOrOpen(std::move(service_instance_existence_marker_file_path),
                                                  take_ownership);
}

std::optional<memory::shared::LockFile> CreateOrOpenServiceInstanceUsageMarkerFile(
    const LolaServiceInstanceId::InstanceId lola_instance_id,
    const IPartialRestartPathBuilder& partial_restart_path_builder) noexcept
{
    auto service_instance_usage_marker_file_path =
        partial_restart_path_builder.GetServiceInstanceUsageMarkerFilePath(lola_instance_id);

    // The instance usage marker file should be created if the skeleton is starting up for the very first time and
    // opened in all other cases.
    // We should never take ownership of the file so that it remains in the filesystem indefinitely. This is because
    // proxies might still have a shared lock on the file while destructing the skeleton. It is imperative to retain
    // this knowledge between skeleton restarts.
    constexpr bool take_ownership{false};
    return memory::shared::LockFile::CreateOrOpen(service_instance_usage_marker_file_path, take_ownership);
}

std::string GetControlChannelShmPath(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                     const QualityType quality_type,
                                     const IShmPathBuilder& shm_path_builder) noexcept
{
    const auto instance_id = lola_service_instance_deployment.instance_id_.value().GetId();
    return shm_path_builder.GetControlChannelShmName(instance_id, quality_type);
}

std::string GetDataChannelShmPath(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                  const IShmPathBuilder& shm_path_builder) noexcept
{
    const auto instance_id = lola_service_instance_deployment.instance_id_.value().GetId();
    return shm_path_builder.GetDataChannelShmName(instance_id);
}

}  // namespace

namespace detail_skeleton
{

bool HasAsilBSupport(const InstanceIdentifier& identifier) noexcept
{
    return (InstanceIdentifierView{identifier}.GetServiceInstanceDeployment().asilLevel_ == QualityType::kASIL_B);
}

}  // namespace detail_skeleton

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'service_instance_existence_marker_file.value()' in case the
// 'service_instance_existence_marker_file' doesn't have value but as we check before with 'has_value()' so no way for
// throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<Skeleton> Skeleton::Create(
    const InstanceIdentifier& identifier,
    score::filesystem::Filesystem filesystem,
    std::unique_ptr<IShmPathBuilder> shm_path_builder,
    std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(partial_restart_path_builder != nullptr,
                                 "Skeleton::Create: partial restart path builder pointer is Null");
    const auto partial_restart_dir_creation_result =
        CreatePartialRestartDirectory(filesystem, *partial_restart_path_builder);
    if (!partial_restart_dir_creation_result)
    {
        score::mw::log::LogError("lola") << "Could not create partial restart directory.";
        return nullptr;
    }

    const auto& lola_service_instance_deployment = GetLolaServiceInstanceDeployment(identifier);
    const auto lola_instance_id = lola_service_instance_deployment.instance_id_.value().GetId();
    auto service_instance_existence_marker_file =
        CreateOrOpenServiceInstanceExistenceMarkerFile(lola_instance_id, *partial_restart_path_builder);
    if (!service_instance_existence_marker_file.has_value())
    {
        score::mw::log::LogError("lola") << "Could not create or open service instance existence marker file.";
        return nullptr;
    }

    auto service_instance_existence_mutex_and_lock =
        std::make_unique<memory::shared::FlockMutexAndLock<memory::shared::ExclusiveFlockMutex>>(
            *service_instance_existence_marker_file);
    if (!service_instance_existence_mutex_and_lock->TryLock())
    {
        score::mw::log::LogError("lola")
            << "Flock try_lock failed: Another Skeleton could have already flocked the marker file and is "
               "actively offering the same service instance.";
        return nullptr;
    }

    const auto& lola_service_type_deployment = GetLolaServiceTypeDeployment(identifier);
    // Since we were able to flock the existence marker file, it means that either we created it or the skeleton that
    // created it previously crashed. Either way, we take ownership of the LockFile so that it's destroyed when this
    // Skeleton is destroyed.
    service_instance_existence_marker_file.value().TakeOwnership();
    return std::make_unique<lola::Skeleton>(identifier,
                                            lola_service_instance_deployment,
                                            lola_service_type_deployment,
                                            std::move(filesystem),
                                            std::move(shm_path_builder),
                                            std::move(partial_restart_path_builder),
                                            std::move(service_instance_existence_marker_file),
                                            std::move(service_instance_existence_mutex_and_lock));
}

Skeleton::Skeleton(const InstanceIdentifier& identifier,
                   const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                   const LolaServiceTypeDeployment& lola_service_type_deployment,
                   score::filesystem::Filesystem filesystem,
                   std::unique_ptr<IShmPathBuilder> shm_path_builder,
                   std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder,
                   std::optional<memory::shared::LockFile> service_instance_existence_marker_file,
                   std::unique_ptr<memory::shared::FlockMutexAndLock<memory::shared::ExclusiveFlockMutex>>
                       service_instance_existence_flock_mutex_and_lock)
    : SkeletonBinding{},
      identifier_{identifier},
      lola_service_instance_deployment_{lola_service_instance_deployment},
      lola_service_type_deployment_{lola_service_type_deployment},
      lola_instance_id_{lola_service_instance_deployment_.instance_id_.value().GetId()},
      lola_service_id_{lola_service_type_deployment.service_id_},
      data_storage_path_{},
      data_control_qm_path_{},
      data_control_asil_path_{},
      storage_{nullptr},
      control_qm_{nullptr},
      control_asil_b_{nullptr},
      storage_resource_{},
      control_qm_resource_{},
      control_asil_resource_{},
      shm_path_builder_{std::move(shm_path_builder)},
      partial_restart_path_builder_{std::move(partial_restart_path_builder)},
      service_instance_existence_marker_file_{std::move(service_instance_existence_marker_file)},
      service_instance_usage_marker_file_{},
      service_instance_existence_flock_mutex_and_lock_{std::move(service_instance_existence_flock_mutex_and_lock)},
      was_old_shm_region_reopened_{false},
      filesystem_{std::move(filesystem)}
{
}

auto Skeleton::PrepareOffer(SkeletonEventBindings& events,
                            SkeletonFieldBindings& fields,
                            std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept
    -> ResultBlank
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(partial_restart_path_builder_ != nullptr,
                                 "Skeleton::PrepareOffer: partial restart path builder pointer is Null");
    service_instance_usage_marker_file_ =
        CreateOrOpenServiceInstanceUsageMarkerFile(lola_instance_id_, *partial_restart_path_builder_);
    if (!service_instance_usage_marker_file_.has_value())
    {
        score::mw::log::LogError("lola") << "Could not create or open service instance usage marker file.";
        /// \todo Use logical error code
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    memory::shared::ExclusiveFlockMutex service_instance_usage_mutex{*service_instance_usage_marker_file_};
    std::unique_lock<memory::shared::ExclusiveFlockMutex> service_instance_usage_lock{service_instance_usage_mutex,
                                                                                      std::defer_lock};
    const bool previous_shm_region_unused_by_proxies = service_instance_usage_lock.try_lock();
    was_old_shm_region_reopened_ = !previous_shm_region_unused_by_proxies;
    if (previous_shm_region_unused_by_proxies)
    {

        score::mw::log::LogDebug("lola") << "Recreating SHM of Skeleton (S:" << lola_service_id_
                                       << "I:" << lola_instance_id_ << ")";
        // Since the previous shared memory region is not being currently used by proxies, this can mean 2 things: (1)
        // The previous shared memory was properly created and OfferService finished (the SkeletonBinding and all
        // Skeleton service elements finished their PrepareOffer calls) and either no Proxies subscribed or they have
        // all since unsubscribed. Or, (2), the previous Skeleton crashed while setting up the shared memory. Since we
        // don't differentiate between the 2 cases and because it's unused anyway, we simply remove the old memory
        // region and re-create it.
        RemoveStaleSharedMemoryArtefacts();

        const auto create_result = CreateSharedMemory(events, fields, std::move(register_shm_object_trace_callback));
        return create_result;
    }
    else
    {
        score::mw::log::LogDebug("lola") << "Reusing SHM of Skeleton (S:" << lola_service_id_ << "I:" << lola_instance_id_
                                       << ")";
        // Since the previous shared memory region is being currently used by proxies, it must have been properly
        // created and OfferService finished. Therefore, we can simply re-open it and cleanup any previous in-writing
        // transactions by the previous skeleton.
        const auto open_result = OpenExistingSharedMemory(std::move(register_shm_object_trace_callback));
        if (!open_result.has_value())
        {
            return open_result;
        }
        CleanupSharedMemoryAfterCrash();
        return {};
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto Skeleton::PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_callback) noexcept
    -> void
{
    if (unregister_shm_object_callback.has_value())
    {
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        unregister_shm_object_callback.value()(
            tracing::TracingRuntime::kDummyElementNameForShmRegisterCallback,
            // Suppress "AUTOSAR C++14 A4-5-1" rule findings. This rule states: "Expressions with type enum or enum
            // class shall not be used as operands to built-in and overloaded operators other than the subscript
            // operator [ ], the assignment operator =, the equality operators == and ! =, the unary & operator, and the
            // relational operators <,
            // <=, >, >=.". The enum is not an operand, its a function parameter.
            // coverity[autosar_cpp14_a4_5_1_violation : FALSE]
            tracing::TracingRuntime::kDummyElementTypeForShmRegisterCallback);
    }

    memory::shared::ExclusiveFlockMutex service_instance_usage_mutex{*service_instance_usage_marker_file_};
    std::unique_lock<memory::shared::ExclusiveFlockMutex> service_instance_usage_lock{service_instance_usage_mutex,
                                                                                      std::defer_lock};
    if (!service_instance_usage_lock.try_lock())
    {
        score::mw::log::LogInfo("lola")
            << "Skeleton::RemoveSharedMemory(): Could not exclusively lock service instance usage "
               "marker file indicating that some proxies are still subscribed. Will not remove shared memory.";
        return;
    }
    else
    {
        RemoveSharedMemory();
        service_instance_usage_lock.unlock();
        service_instance_usage_marker_file_.reset();
    }

    storage_ = nullptr;
    control_qm_ = nullptr;
    control_asil_b_ = nullptr;
}

auto Skeleton::CreateSharedMemory(
    SkeletonEventBindings& events,
    SkeletonFieldBindings& fields,
    std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept -> ResultBlank
{
    const auto storage_size_calc_result = CalculateShmResourceStorageSizes(events, fields);

    if (!CreateSharedMemoryForControl(
            lola_service_instance_deployment_, QualityType::kASIL_QM, storage_size_calc_result.control_qm_size))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not create shared memory object for control QM");
    }

    if (detail_skeleton::HasAsilBSupport(identifier_) &&
        (!CreateSharedMemoryForControl(lola_service_instance_deployment_,
                                       QualityType::kASIL_B,
                                       storage_size_calc_result.control_asil_b_size.value())))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle,
                              "Could not create shared memory object for control ASIL-B");
    }

    if (!CreateSharedMemoryForData(lola_service_instance_deployment_,
                                   storage_size_calc_result.data_size,
                                   std::move(register_shm_object_trace_callback)))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not create shared memory object for data");
    }
    return {};
}

auto Skeleton::OpenExistingSharedMemory(
    std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept -> ResultBlank
{
    if (!OpenSharedMemoryForControl(QualityType::kASIL_QM))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for control QM");
    }

    if (detail_skeleton::HasAsilBSupport(identifier_) && (!OpenSharedMemoryForControl(QualityType::kASIL_B)))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for control ASIL-B");
    }

    if (!OpenSharedMemoryForData(std::move(register_shm_object_trace_callback)))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for data");
    }
    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool Skeleton::CreateSharedMemoryForData(
    const LolaServiceInstanceDeployment& instance,
    const std::size_t shm_size,
    std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept
{
    memory::shared::SharedMemoryFactory::UserPermissionsMap permissions{};
    for (const auto& allowed_consumer : instance.allowed_consumer_)
    {
        for (const auto& user_identifier : allowed_consumer.second)
        {
            permissions[score::os::Acl::Permission::kRead].push_back(user_identifier);
        }
    }

    const auto path = shm_path_builder_->GetDataChannelShmName(lola_instance_id_);
    const bool use_typed_memory = register_shm_object_trace_callback.has_value();
    const memory::shared::SharedMemoryFactory::UserPermissions user_permissions =
        (permissions.empty()) && (instance.strict_permissions_ == false)
            ? memory::shared::SharedMemoryFactory::WorldReadable{}
            : memory::shared::SharedMemoryFactory::UserPermissions{permissions};
    const auto memory_resource = score::memory::shared::SharedMemoryFactory::Create(
        path,
        [this](std::shared_ptr<score::memory::shared::ISharedMemoryResource> memory) {
            this->InitializeSharedMemoryForData(memory);
        },
        shm_size,
        user_permissions,
        use_typed_memory);
    if (memory_resource == nullptr)
    {
        return false;
    }
    data_storage_path_ = path;
    if (register_shm_object_trace_callback.has_value() && memory_resource->IsShmInTypedMemory())
    {
        // only if the memory_resource could be successfully allocated in typed-memory, we call back the
        // register_shm_object_trace_callback, because only then the shm-object can be accessed by tracing
        // subsystem.
        // Since LoLa creates shm-objects on the granularity of whole service-instances (including ALL its service
        // elements), we call register_shm_object_trace_callback once and hand over a dummy element name/type!
        // Other bindings, which might create shm-objects per service-element would call
        // register_shm_object_trace_callback for each service-element and then use their "real" name and type ...
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        register_shm_object_trace_callback.value()(
            tracing::TracingRuntime::kDummyElementNameForShmRegisterCallback,
            // Suppress "AUTOSAR C++14 A4-5-1" rule findings. This rule states: "Expressions with type enum or enum
            // class shall not be used as operands to built-in and overloaded operators other than the subscript
            // operator [ ], the assignment operator =, the equality operators == and ! =, the unary & operator, and the
            // relational operators <,
            // <=, >, >=.". The enum is not an operand, its a function parameter.
            // coverity[autosar_cpp14_a4_5_1_violation : FALSE]
            tracing::TracingRuntime::kDummyElementTypeForShmRegisterCallback,
            memory_resource->GetFileDescriptor(),
            memory_resource->getBaseAddress());
    }

    score::mw::log::LogDebug("lola") << "Created shared-memory-object for DATA (S: " << lola_service_id_
                                   << " I:" << lola_instance_id_ << ")";
    return true;
}

bool Skeleton::CreateSharedMemoryForControl(const LolaServiceInstanceDeployment& instance,
                                            const QualityType asil_level,
                                            const std::size_t shm_size) noexcept
{
    const auto path = shm_path_builder_->GetControlChannelShmName(lola_instance_id_, asil_level);

    const auto consumer = instance.allowed_consumer_.find(asil_level);
    auto& control_resource = (asil_level == QualityType::kASIL_QM) ? control_qm_resource_ : control_asil_resource_;
    auto& data_control_path = (asil_level == QualityType::kASIL_QM) ? data_control_qm_path_ : data_control_asil_path_;

    memory::shared::SharedMemoryFactory::UserPermissionsMap permissions{};
    if (consumer != instance.allowed_consumer_.cend())
    {
        for (const auto& user_identifier : consumer->second)
        {
            permissions[score::os::Acl::Permission::kRead].push_back(user_identifier);
            permissions[score::os::Acl::Permission::kWrite].push_back(user_identifier);
        }
    }

    const memory::shared::SharedMemoryFactory::UserPermissions user_permissions =
        (permissions.empty()) && (instance.strict_permissions_ == false)
            ? memory::shared::SharedMemoryFactory::WorldWritable{}
            : memory::shared::SharedMemoryFactory::UserPermissions{permissions};
    control_resource = score::memory::shared::SharedMemoryFactory::Create(
        path,
        [this, asil_level](std::shared_ptr<score::memory::shared::ManagedMemoryResource> memory) {
            this->InitializeSharedMemoryForControl(asil_level, memory);
        },
        shm_size,
        user_permissions);
    if (control_resource == nullptr)
    {
        return false;
    }
    data_control_path = path;
    return true;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool Skeleton::OpenSharedMemoryForData(
    const std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(shm_path_builder_ != nullptr,
                                 "Skeleton::OpenSharedMemoryForData: shared memory path builder pointer is Null");
    const auto path = GetDataChannelShmPath(lola_service_instance_deployment_, *shm_path_builder_);

    const auto memory_resource = score::memory::shared::SharedMemoryFactory::Open(path, true);
    if (memory_resource == nullptr)
    {
        return false;
    }
    data_storage_path_ = path;
    storage_resource_ = memory_resource;

    const auto& memory_resource_ref = *memory_resource.get();
    storage_ = GetServiceDataStorageSkeletonSide(memory_resource_ref);

    // Our pid will have changed after re-start and we now have to update it in the re-opened DATA section.
    const auto pid = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetPid();
    score::mw::log::LogDebug("lola") << "Updating PID of Skeleton (S: " << lola_service_id_ << " I:" << lola_instance_id_
                                   << ") with:" << pid;
    storage_->skeleton_pid_ = pid;

    if (register_shm_object_trace_callback.has_value() && memory_resource->IsShmInTypedMemory())
    {
        // only if the memory_resource could be successfully allocated in typed-memory, we call back the
        // register_shm_object_trace_callback, because only then the shm-object can be accessed by tracing
        // subsystem.
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        register_shm_object_trace_callback.value()(
            tracing::TracingRuntime::kDummyElementNameForShmRegisterCallback,
            // Suppress "AUTOSAR C++14 A4-5-1" rule findings. This rule states: "Expressions with type enum or enum
            // class shall not be used as operands to built-in and overloaded operators other than the subscript
            // operator [ ], the assignment operator =, the equality operators == and ! =, the unary & operator, and the
            // relational operators <,
            // <=, >, >=.". The enum is not an operand, its a function parameter.
            // coverity[autosar_cpp14_a4_5_1_violation : FALSE]
            tracing::TracingRuntime::kDummyElementTypeForShmRegisterCallback,
            memory_resource->GetFileDescriptor(),
            memory_resource->getBaseAddress());
    }
    return true;
}

bool Skeleton::OpenSharedMemoryForControl(const QualityType asil_level) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(shm_path_builder_ != nullptr,
                                 "Skeleton::OpenSharedMemoryForControl: shared memory path builder pointer is Null");
    const auto path = GetControlChannelShmPath(lola_service_instance_deployment_, asil_level, *shm_path_builder_);

    auto& control_resource = (asil_level == QualityType::kASIL_QM) ? control_qm_resource_ : control_asil_resource_;
    auto& data_control_path = (asil_level == QualityType::kASIL_QM) ? data_control_qm_path_ : data_control_asil_path_;

    control_resource = score::memory::shared::SharedMemoryFactory::Open(path, true);
    if (control_resource == nullptr)
    {
        return false;
    }
    data_control_path = path;

    auto& control = (asil_level == QualityType::kASIL_QM) ? control_qm_ : control_asil_b_;

    const auto& control_resource_ref = *control_resource.get();
    control = GetServiceDataControlSkeletonSide(control_resource_ref);

    return true;
}

void Skeleton::RemoveSharedMemory() noexcept
{
    constexpr auto RemoveMemoryIfExists = [](const score::cpp::optional<std::string>& path) -> void {
        if (path.has_value())
        {
            score::memory::shared::SharedMemoryFactory::Remove(path.value());
        }
    };
    RemoveMemoryIfExists(data_control_qm_path_);
    RemoveMemoryIfExists(data_control_asil_path_);
    RemoveMemoryIfExists(data_storage_path_);

    storage_resource_.reset();
    control_qm_resource_.reset();
    control_asil_resource_.reset();
}

void Skeleton::RemoveStaleSharedMemoryArtefacts() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        shm_path_builder_ != nullptr,
        "Skeleton::RemoveStaleSharedMemoryArtefacts: shared memory path builder pointer is Null");
    const auto control_qm_path =
        GetControlChannelShmPath(lola_service_instance_deployment_, QualityType::kASIL_QM, *shm_path_builder_);
    const auto control_asil_b_path =
        GetControlChannelShmPath(lola_service_instance_deployment_, QualityType::kASIL_B, *shm_path_builder_);
    const auto data_path = GetDataChannelShmPath(lola_service_instance_deployment_, *shm_path_builder_);

    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(control_qm_path);
    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(control_asil_b_path);
    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(data_path);
}

Skeleton::ShmResourceStorageSizes Skeleton::CalculateShmResourceStorageSizesBySimulation(
    SkeletonEventBindings& events,
    SkeletonFieldBindings& fields) noexcept
{
    using NewDeleteDelegateMemoryResource = memory::shared::NewDeleteDelegateMemoryResource;

    // we create up to 3 DryRun Memory Resources and then do the "normal" initialization of control and data
    // shm-objects on it
    control_qm_resource_ = std::make_shared<NewDeleteDelegateMemoryResource>(
        CalculateMemoryResourceId(lola_service_id_, lola_instance_id_, ShmObjectType::kControl_QM));

    storage_resource_ = std::make_shared<NewDeleteDelegateMemoryResource>(
        CalculateMemoryResourceId(lola_service_id_, lola_instance_id_, ShmObjectType::kData));

    // Note, that it is important to have all DryRun Memory Resources "active" in parallel as the upcoming calls to
    // PrepareOffer() for the events triggers all SkeletonEvents to register themselves at their parent Skeleton
    // (lola::Skeleton::Register()), which leads to updates/allocation within ctrl AND data resources!
    InitializeSharedMemoryForControl(QualityType::kASIL_QM, control_qm_resource_);

    if (detail_skeleton::HasAsilBSupport(identifier_))
    {
        control_asil_resource_ = std::make_shared<NewDeleteDelegateMemoryResource>(
            CalculateMemoryResourceId(lola_service_id_, lola_instance_id_, ShmObjectType::kControl_ASIL_B));
        InitializeSharedMemoryForControl(QualityType::kASIL_B, control_asil_resource_);
    }
    InitializeSharedMemoryForData(storage_resource_);

    // Offer events to calculate the shared memory allocated for the control and data segments for each event
    for (auto& event : events)
    {
        score::cpp::ignore = event.second.get().PrepareOffer();
    }
    for (auto& field : fields)
    {
        score::cpp::ignore = field.second.get().PrepareOffer();
    }

    const auto control_qm_size = control_qm_resource_->GetUserAllocatedBytes();
    const auto control_data_size = storage_resource_->GetUserAllocatedBytes();

    // PrepareStopOffer events to clean up the events/fields again for the real/next offer done after simulation.
    for (auto& event : events)
    {
        event.second.get().PrepareStopOffer();
    }
    for (auto& field : fields)
    {
        field.second.get().PrepareStopOffer();
    }

    const auto control_asil_b_size = detail_skeleton::HasAsilBSupport(identifier_)
                                         ? score::cpp::optional<std::size_t>{control_asil_resource_->GetUserAllocatedBytes()}
                                         : score::cpp::optional<std::size_t>{};

    return ShmResourceStorageSizes{control_data_size, control_qm_size, control_asil_b_size};
}

Skeleton::ShmResourceStorageSizes Skeleton::CalculateShmResourceStorageSizes(SkeletonEventBindings& events,
                                                                             SkeletonFieldBindings& fields) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetShmSizeCalculationMode() ==
                               ShmSizeCalculationMode::kSimulation,
                           "No other shm size calculation mode is currently suppored");
    if ((lola_service_instance_deployment_.shared_memory_size_.has_value()) &&
        (lola_service_instance_deployment_.control_asil_b_memory_size_.has_value()) &&
        (lola_service_instance_deployment_.control_qm_memory_size_.has_value()))
    {
        score::mw::log::LogInfo("lola") << "shm-size, control-asil-b-shm-size and control-qm-shm-size manually specified "
                                         "for service_id:instance_id "
                                      << lola_service_id_
                                      << ":"
                                      // coverity[autosar_cpp14_a18_9_2_violation]
                                      << lola_instance_id_
                                      << "- Make sure that this value is sufficiently big to"
                                         "avoid aborts at runtime.";
        return {lola_service_instance_deployment_.shared_memory_size_.value(),
                lola_service_instance_deployment_.control_qm_memory_size_.value(),
                lola_service_instance_deployment_.control_asil_b_memory_size_.value()};
    }

    auto required_shm_storage_size = CalculateShmResourceStorageSizesBySimulation(events, fields);

    const std::size_t control_asil_b_size_result = required_shm_storage_size.control_asil_b_size.has_value()
                                                       ? required_shm_storage_size.control_asil_b_size.value()
                                                       : 0U;

    // Suppress "AUTOSAR C++14 A18-9-2" rule finding: "Forwarding values to other functions shall be done via:
    // (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
    // reference".
    // Passing result of std::move() as a const reference argument, no move will actually happen.
    // coverity[autosar_cpp14_a18_9_2_violation]
    score::mw::log::LogInfo("lola") << "Calculated sizes of shm-objects for service_id:instance_id " << lola_service_id_
                                  << ":"
                                  // coverity[autosar_cpp14_a18_9_2_violation]
                                  << lola_instance_id_
                                  << " are as follows:\nQM-Ctrl: " << required_shm_storage_size.control_qm_size
                                  << ", ASIL_B-Ctrl: " << control_asil_b_size_result
                                  << ", Data: " << required_shm_storage_size.data_size;

    if (lola_service_instance_deployment_.shared_memory_size_.has_value())
    {
        score::mw::log::LogInfo("lola") << "shm-size manually specified for service_id:instance_id " << lola_service_id_
                                      << ":"
                                      // coverity[autosar_cpp14_a18_9_2_violation]
                                      << lola_instance_id_
                                      << "- Make sure that this value is sufficiently big to"
                                         "avoid aborts at runtime.";
        required_shm_storage_size.data_size = lola_service_instance_deployment_.shared_memory_size_.value();
    }

    if (lola_service_instance_deployment_.control_asil_b_memory_size_.has_value())
    {
        score::mw::log::LogInfo("lola") << "control-asil-b-shm-size manually specified for service_id:instance_id "
                                      << lola_service_id_
                                      << ":"
                                      // coverity[autosar_cpp14_a18_9_2_violation]
                                      << lola_instance_id_
                                      << "- Make sure that this value is sufficiently big to"
                                         "avoid aborts at runtime.";
        required_shm_storage_size.control_asil_b_size =
            lola_service_instance_deployment_.control_asil_b_memory_size_.value();
    }

    if (lola_service_instance_deployment_.control_qm_memory_size_.has_value())
    {
        score::mw::log::LogInfo("lola") << "control-qm-shm-size manually specified for service_id:instance_id "
                                      << lola_service_id_
                                      << ":"
                                      // coverity[autosar_cpp14_a18_9_2_violation]
                                      << lola_instance_id_
                                      << "- Make sure that this value is sufficiently big to"
                                         "avoid aborts at runtime.";
        required_shm_storage_size.control_qm_size = lola_service_instance_deployment_.control_qm_memory_size_.value();
    }

    return required_shm_storage_size;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the
// key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw
// an exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::cpp::optional<EventMetaInfo> Skeleton::GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept
{
    auto search = storage_->events_metainfo_.find(element_fq_id);
    if (search == storage_->events_metainfo_.cend())
    {
        return score::cpp::nullopt;
    }
    else
    {
        // Suppress "AUTOSAR C++14 A5-3-2" rule finding. This rule declares: "Null pointers shall not be dereferenced.".
        // The "search" variable is an iterator of interprocess map returned by "find" method.
        // Performed the check, that iterator is not equal to map.end(). A check is made that the iterator is not equal
        // to map.end(). Therefore, the call to "search->" does not return nullptr.
        // coverity[autosar_cpp14_a5_3_2_violation]
        return search->second;
    }
}

QualityType Skeleton::GetInstanceQualityType() const noexcept
{
    return InstanceIdentifierView{identifier_}.GetServiceInstanceDeployment().asilLevel_;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void Skeleton::CleanupSharedMemoryAfterCrash() noexcept
{
    for (auto& event : control_qm_->event_controls_)
    {
        event.second.data_control.RemoveAllocationsForWriting();
    }

    if (control_asil_b_ != nullptr)
    {
        for (auto& event : control_asil_b_->event_controls_)
        {
            event.second.data_control.RemoveAllocationsForWriting();
        }
    }
}

void Skeleton::DisconnectQmConsumers() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(GetInstanceQualityType() == QualityType::kASIL_B,
                           "DisconnectQmConsumers() called on a QualityType::kASIL_QM instance!");

    auto result = impl::Runtime::getInstance().GetServiceDiscovery().StopOfferService(
        identifier_, IServiceDiscovery::QualityTypeSelector::kAsilQm);
    if (!result.has_value())
    {
        score::mw::log::LogWarn("lola")
            << __func__ << __LINE__
            << "Disconnecting unsafe QM consumers via StopOffer of ASIL-QM part of service instance failed.";
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void Skeleton::InitializeSharedMemoryForData(
    const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory) noexcept
{
    storage_ = memory->construct<ServiceDataStorage>(memory->getMemoryResourceProxy());
    storage_resource_ = memory;
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // There is no variable instantiation.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(storage_resource_ != nullptr,
                           "storage_resource_ must be no nullptr, otherwise the callback would not be invoked.");
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void Skeleton::InitializeSharedMemoryForControl(
    const QualityType asil_level,
    const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory) noexcept
{
    auto& control = (asil_level == QualityType::kASIL_QM) ? control_qm_ : control_asil_b_;
    control = memory->construct<ServiceDataControl>(memory->getMemoryResourceProxy());
}

}  // namespace score::mw::com::impl::lola
