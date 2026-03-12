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
#include "score/mw/com/impl/bindings/lola/skeleton_memory_manager.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/mw/log/logging.h"
#include "score/os/acl.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/span.hpp>
#include <sys/types.h>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace score::mw::com::impl::lola
{
namespace
{

ServiceDataControl* GetServiceDataControlSkeletonSide(const memory::shared::ManagedMemoryResource& control)
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule. The rule declares:
    // An object with integer type or pointer to void type shall not be converted to an object with pointer type.
    // The "ServiceDataStorage" type is strongly defined as shared IPC data between Proxy and Skeleton.
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto* const service_data_control = static_cast<ServiceDataControl*>(control.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_data_control != nullptr,
                                                "Could not retrieve service data control.");
    return service_data_control;
}

ServiceDataStorage* GetServiceDataStorageSkeletonSide(const memory::shared::ManagedMemoryResource& data)
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

std::string GetControlChannelShmPath(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                     const QualityType quality_type,
                                     const IShmPathBuilder& shm_path_builder)
{
    const auto instance_id = lola_service_instance_deployment.instance_id_.value().GetId();
    return shm_path_builder.GetControlChannelShmName(instance_id, quality_type);
}

std::string GetDataChannelShmPath(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                  const IShmPathBuilder& shm_path_builder)
{
    const auto instance_id = lola_service_instance_deployment.instance_id_.value().GetId();
    return shm_path_builder.GetDataChannelShmName(instance_id);
}

enum class ShmObjectType : std::uint8_t
{
    kControl_QM = 0x00,
    kControl_ASIL_B = 0x01,
    kData = 0x02,
};

std::uint64_t CalculateMemoryResourceId(const LolaServiceTypeDeployment::ServiceId lola_service_id,
                                        const LolaServiceInstanceId::InstanceId lola_instance_id,
                                        const ShmObjectType object_type)
{
    return ((static_cast<std::uint64_t>(lola_service_id) << 24U) +
            (static_cast<std::uint64_t>(lola_instance_id) << 8U) + static_cast<std::uint8_t>(object_type));
}

}  // namespace

SkeletonMemoryManager::SkeletonMemoryManager(QualityType quality_type,
                                             const IShmPathBuilder& shm_path_builder,
                                             const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                             const LolaServiceTypeDeployment& lola_service_type_deployment,
                                             const LolaServiceInstanceId::InstanceId lola_instance_id,
                                             const LolaServiceTypeDeployment::ServiceId lola_service_id)
    : quality_type_{quality_type},
      shm_path_builder_{shm_path_builder},
      lola_service_instance_deployment_{lola_service_instance_deployment},
      lola_service_type_deployment_{lola_service_type_deployment},
      lola_instance_id_{lola_instance_id},
      lola_service_id_{lola_service_id},
      data_storage_path_{},
      data_control_qm_path_{},
      data_control_asil_path_{},
      storage_{nullptr},
      control_qm_{nullptr},
      control_asil_b_{nullptr},
      skeleton_control_qm_local_{},
      skeleton_control_asil_b_local_{},
      proxy_control_local_{},
      storage_resource_{},
      control_qm_resource_{},
      control_asil_resource_{}
{
}

auto SkeletonMemoryManager::OpenExistingSharedMemory(
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback) -> ResultBlank
{
    if (!OpenSharedMemoryForControl(QualityType::kASIL_QM))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for control QM");
    }

    if ((quality_type_ == QualityType::kASIL_B) && (!OpenSharedMemoryForControl(QualityType::kASIL_B)))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for control ASIL-B");
    }

    if (!OpenSharedMemoryForData(std::move(register_shm_object_trace_callback)))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not open shared memory object for data");
    }
    return {};
}

auto SkeletonMemoryManager::CreateSharedMemory(
    SkeletonBinding::SkeletonEventBindings& events,
    SkeletonBinding::SkeletonFieldBindings& fields,
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback) -> ResultBlank
{
    const auto storage_size_calc_result = CalculateShmResourceStorageSizes(events, fields);

    if (!CreateSharedMemoryForControl(
            lola_service_instance_deployment_, QualityType::kASIL_QM, storage_size_calc_result.control_qm_size))
    {
        return MakeUnexpected(ComErrc::kErroneousFileHandle, "Could not create shared memory object for control QM");
    }

    if ((quality_type_ == QualityType::kASIL_B) &&
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

EventDataControlComposite<> SkeletonMemoryManager::CreateEventControlComposite(
    const ElementFqId element_fq_id,
    const SkeletonEventProperties& element_properties) noexcept
{
    auto [event_data_control_qm, skeleton_event_data_control_local_qm] =
        InsertEventInServiceDataControl(QualityType::kASIL_QM, element_fq_id, element_properties);

    SkeletonEventDataControlLocalView<>* control_asil_local_result{nullptr};
    if (control_asil_resource_ != nullptr)
    {
        auto [_, skeleton_event_data_control_local_asil_b] =
            InsertEventInServiceDataControl(QualityType::kASIL_B, element_fq_id, element_properties);

        // Suppression rationale: The result pointer is still valid outside this method until Skeleton object (as a
        // holder) is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        control_asil_local_result = &skeleton_event_data_control_local_asil_b.get().data_control;
    }

    ProxyEventDataControlLocalView<>* proxy_event_data_control_local{nullptr};
    if (proxy_control_local_.has_value())
    {
        auto control_qm_local =
            proxy_control_local_->event_controls_.emplace(std::piecewise_construct,
                                                          std::forward_as_tuple(element_fq_id),
                                                          std::forward_as_tuple(event_data_control_qm.get()));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            control_qm_local.second, "Couldn't register/emplace ProxyEventControlLocalView in control-section.");

        // Suppressions rationale: The result pointer is still valid outside this method until Skeleton object (as a
        // holder) is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        proxy_event_data_control_local = &control_qm_local.first->second.data_control;
    }

    // The lifetime of the "control_asil_result" object lasts as long as the Skeleton is alive.
    // coverity[autosar_cpp14_m7_5_1_violation]
    // coverity[autosar_cpp14_m7_5_2_violation]
    // coverity[autosar_cpp14_a3_8_1_violation]
    return EventDataControlComposite{&skeleton_event_data_control_local_qm.get().data_control,
                                     control_asil_local_result,
                                     proxy_event_data_control_local};
}

std::pair<void*, EventDataControlComposite<>> SkeletonMemoryManager::CreateEventDataFromOpenedSharedMemory(
    const ElementFqId element_fq_id,
    const SkeletonEventProperties& element_properties,
    size_t sample_size,
    size_t sample_alignment) noexcept
{
    // Guard against over-aligned types (Short-term solution protection)
    if (sample_alignment > alignof(std::max_align_t))
    {
        score::mw::log::LogFatal("Skeleton")
            << "Requested sample alignment (" << sample_alignment << ") exceeds max_align_t ("
            << alignof(std::max_align_t) << "). Safe shared memory layout cannot be guaranteed.";

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(sample_alignment <= alignof(std::max_align_t),
                                                    "Requested sample alignment exceeds maximum supported alignment.");
    }

    // Calculate the aligned size for a single sample to ensure proper padding between slots
    const auto aligned_sample_size = memory::shared::CalculateAlignedSize(sample_size, sample_alignment);
    const auto total_data_size_bytes = aligned_sample_size * element_properties.number_of_slots;

    // Convert total bytes to the number of std::max_align_t elements needed (round up)
    const size_t num_max_align_elements =
        (total_data_size_bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t);

    auto* data_storage = storage_resource_->construct<EventDataStorage<std::max_align_t>>(
        num_max_align_elements, memory::shared::PolymorphicOffsetPtrAllocator<std::max_align_t>(*storage_resource_));

    auto inserted_data_slots = storage_->events_.emplace(
        std::piecewise_construct, std::forward_as_tuple(element_fq_id), std::forward_as_tuple(data_storage));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_data_slots.second,
                                                "Couldn't register/emplace event-storage in data-section.");

    const DataTypeMetaInfo sample_meta_info{sample_size, static_cast<std::uint8_t>(sample_alignment)};
    void* const event_data_raw_array = data_storage->data();

    auto inserted_meta_info =
        storage_->events_metainfo_.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(element_fq_id),
                                           std::forward_as_tuple(sample_meta_info, event_data_raw_array));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_meta_info.second,
                                                "Couldn't register/emplace event-meta-info in data-section.");

    return {data_storage, CreateEventControlComposite(element_fq_id, element_properties)};
}

void SkeletonMemoryManager::RemoveSharedMemory()
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

void SkeletonMemoryManager::RemoveStaleSharedMemoryArtefacts() const
{
    const auto control_qm_path =
        GetControlChannelShmPath(lola_service_instance_deployment_, QualityType::kASIL_QM, shm_path_builder_);
    const auto control_asil_b_path =
        GetControlChannelShmPath(lola_service_instance_deployment_, QualityType::kASIL_B, shm_path_builder_);
    const auto data_path = GetDataChannelShmPath(lola_service_instance_deployment_, shm_path_builder_);

    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(control_qm_path);
    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(control_asil_b_path);
    memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(data_path);
}

// Suppress "AUTOSAR C++14 A15-5-3": std::terminate() should not be called implicitly.
// This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonMemoryManager::CleanupSharedMemoryAfterCrash()
{
    for (auto& event : skeleton_control_qm_local_->event_controls_)
    {
        event.second.data_control.RemoveAllocationsForWriting();
    }

    if (skeleton_control_asil_b_local_.has_value())
    {
        for (auto& event : skeleton_control_asil_b_local_->event_controls_)
        {
            event.second.data_control.RemoveAllocationsForWriting();
        }
    }
}
void SkeletonMemoryManager::Reset()
{
    storage_ = nullptr;
    control_qm_ = nullptr;
    control_asil_b_ = nullptr;
}

SkeletonMemoryManager::ShmResourceStorageSizes SkeletonMemoryManager::CalculateShmResourceStorageSizes(
    SkeletonBinding::SkeletonEventBindings& events,
    SkeletonBinding::SkeletonFieldBindings& fields)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetShmSizeCalculationMode() ==
            ShmSizeCalculationMode::kSimulation,
        "No other shm size calculation mode is currently suppored");
    if ((lola_service_instance_deployment_.shared_memory_size_.has_value()) &&
        (lola_service_instance_deployment_.control_asil_b_memory_size_.has_value()) &&
        (lola_service_instance_deployment_.control_qm_memory_size_.has_value()))
    {
        score::mw::log::LogInfo("lola")
            << "shm-size, control-asil-b-shm-size and control-qm-shm-size manually specified "
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

SkeletonMemoryManager::ShmResourceStorageSizes SkeletonMemoryManager::CalculateShmResourceStorageSizesBySimulation(
    SkeletonBinding::SkeletonEventBindings& events,
    SkeletonBinding::SkeletonFieldBindings& fields)
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
    // (lola::SkeletonMemoryManager::Register()), which leads to updates/allocation within ctrl AND data resources!
    InitializeSharedMemoryForControl(QualityType::kASIL_QM, control_qm_resource_);

    if (quality_type_ == QualityType::kASIL_B)
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

    const auto control_asil_b_size =
        quality_type_ == QualityType::kASIL_B
            ? score::cpp::optional<std::size_t>{control_asil_resource_->GetUserAllocatedBytes()}
            : score::cpp::optional<std::size_t>{};

    return ShmResourceStorageSizes{control_data_size, control_qm_size, control_asil_b_size};
}

// Suppress "AUTOSAR C++14 A15-5-3":
// This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool SkeletonMemoryManager::CreateSharedMemoryForData(
    const LolaServiceInstanceDeployment& instance,
    const std::size_t shm_size,
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback)
{
    memory::shared::SharedMemoryFactory::UserPermissionsMap permissions{};
    for (const auto& allowed_consumer : instance.allowed_consumer_)
    {
        for (const auto& user_identifier : allowed_consumer.second)
        {
            permissions[score::os::Acl::Permission::kRead].push_back(user_identifier);
        }
    }

    const auto path = shm_path_builder_.GetDataChannelShmName(lola_instance_id_);
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
        // , (true) or (<true condition>), then it shall not exit with an exception"
        // we can't add  to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        register_shm_object_trace_callback.value()(
            tracing::TracingRuntime::kDummyElementNameForShmRegisterCallback,
            // Suppress "AUTOSAR C++14 A4-5-1": Enums should not be used as operands to operators (other than
            // operator[], operator=, operator==, operator!=, * and relational operators).
            // Justification: The enum is not an operator, it's a function parameter.
            // coverity[autosar_cpp14_a4_5_1_violation : FALSE]
            tracing::TracingRuntime::kDummyElementTypeForShmRegisterCallback,
            memory_resource->GetFileDescriptor(),
            memory_resource->getBaseAddress());
        score::cpp::ignore = proxy_control_local_.emplace(*control_qm_);
    }

    score::mw::log::LogDebug("lola") << "Created shared-memory-object for DATA (S: " << lola_service_id_
                                     << " I:" << lola_instance_id_ << ")";
    return true;
}

bool SkeletonMemoryManager::CreateSharedMemoryForControl(const LolaServiceInstanceDeployment& instance,
                                                         const QualityType asil_level,
                                                         const std::size_t shm_size)
{
    const auto path = shm_path_builder_.GetControlChannelShmName(lola_instance_id_, asil_level);

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
bool SkeletonMemoryManager::OpenSharedMemoryForData(
    const std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback)
{
    const auto path = GetDataChannelShmPath(lola_service_instance_deployment_, shm_path_builder_);

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
    score::mw::log::LogDebug("lola") << "Updating PID of Skeleton (S: " << lola_service_id_
                                     << " I:" << lola_instance_id_ << ") with:" << pid;
    storage_->skeleton_pid_ = pid;

    if (register_shm_object_trace_callback.has_value() && memory_resource->IsShmInTypedMemory())
    {
        // only if the memory_resource could be successfully allocated in typed-memory, we call back the
        // register_shm_object_trace_callback, because only then the shm-object can be accessed by tracing
        // subsystem.
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // , (true) or (<true condition>), then it shall not exit with an exception"
        // we can't add  to score::cpp::callback signature.
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
        /// gtodo: Add test that tracing works after restart
        score::cpp::ignore = proxy_control_local_.emplace(*control_qm_);
    }
    return true;
}

bool SkeletonMemoryManager::OpenSharedMemoryForControl(const QualityType asil_level)
{
    const auto path = GetControlChannelShmPath(lola_service_instance_deployment_, asil_level, shm_path_builder_);

    auto& control_resource = (asil_level == QualityType::kASIL_QM) ? control_qm_resource_ : control_asil_resource_;
    auto& data_control_path = (asil_level == QualityType::kASIL_QM) ? data_control_qm_path_ : data_control_asil_path_;

    control_resource = score::memory::shared::SharedMemoryFactory::Open(path, true);
    if (control_resource == nullptr)
    {
        return false;
    }
    data_control_path = path;

    auto& control = (asil_level == QualityType::kASIL_QM) ? control_qm_ : control_asil_b_;
    auto& skeleton_control_local =
        (asil_level == QualityType::kASIL_QM) ? skeleton_control_qm_local_ : skeleton_control_asil_b_local_;

    const auto& control_resource_ref = *control_resource.get();
    control = GetServiceDataControlSkeletonSide(control_resource_ref);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control != nullptr);
    score::cpp::ignore = skeleton_control_local.emplace(*control);

    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    const bool is_tracing_enabled = (tracing_runtime != nullptr) && tracing_runtime->IsTracingEnabled();
    if ((asil_level == QualityType::kASIL_QM) && is_tracing_enabled)
    {
        score::cpp::ignore = proxy_control_local_.emplace(*control);
    }

    return true;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonMemoryManager::InitializeSharedMemoryForData(
    const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory)
{
    storage_ = memory->construct<ServiceDataStorage>(*memory);
    storage_resource_ = memory;
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // There is no variable instantiation.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        storage_resource_ != nullptr,
        "storage_resource_ must be no nullptr, otherwise the callback would not be invoked.");
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonMemoryManager::InitializeSharedMemoryForControl(
    const QualityType asil_level,
    const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory)
{
    auto& control = (asil_level == QualityType::kASIL_QM) ? control_qm_ : control_asil_b_;
    auto& control_local =
        (asil_level == QualityType::kASIL_QM) ? skeleton_control_qm_local_ : skeleton_control_asil_b_local_;

    control = memory->construct<ServiceDataControl>(*memory);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(control != nullptr);
    score::cpp::ignore = control_local.emplace(*control);

    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    const bool is_tracing_enabled = (tracing_runtime != nullptr) && tracing_runtime->IsTracingEnabled();
    if ((asil_level == QualityType::kASIL_QM) && is_tracing_enabled)
    {
        score::cpp::ignore = proxy_control_local_.emplace(*control);
    }
}

std::pair<std::reference_wrapper<EventControl>, std::reference_wrapper<SkeletonEventControlLocalView>>
SkeletonMemoryManager::InsertEventInServiceDataControl(const QualityType asil_level,
                                                       ElementFqId element_fq_id,
                                                       const SkeletonEventProperties& element_properties)
{
    auto* const service_data_control = (asil_level == QualityType::kASIL_QM) ? control_qm_ : control_asil_b_;
    auto& skeleton_service_data_control_local =
        (asil_level == QualityType::kASIL_QM) ? skeleton_control_qm_local_ : skeleton_control_asil_b_local_;
    auto* const memory_resource =
        (asil_level == QualityType::kASIL_QM) ? control_qm_resource_.get() : control_asil_resource_.get();

    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(service_data_control != nullptr);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(skeleton_service_data_control_local.has_value());
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(memory_resource != nullptr);

    auto control_qm =
        service_data_control->event_controls_.emplace(std::piecewise_construct,
                                                      std::forward_as_tuple(element_fq_id),
                                                      std::forward_as_tuple(element_properties.number_of_slots,
                                                                            element_properties.max_subscribers,
                                                                            element_properties.enforce_max_samples,
                                                                            *memory_resource));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(control_qm.second,
                                                "Couldn't register/emplace EventControl in control-section.");

    auto control_qm_local =
        skeleton_service_data_control_local->event_controls_.emplace(std::piecewise_construct,
                                                                     std::forward_as_tuple(element_fq_id),
                                                                     std::forward_as_tuple(control_qm.first->second));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        control_qm_local.second, "Couldn't register/emplace SkeletonEventControlLocalView in control-section.");
    return {control_qm.first->second, control_qm_local.first->second};
}

}  // namespace score::mw::com::impl::lola
