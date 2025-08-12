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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/i_partial_restart_path_builder.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/runtime.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/filesystem/filesystem.h"
#include "score/memory/shared/flock/exclusive_flock_mutex.h"
#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/lock_file.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <score/assert.hpp>
#include <score/optional.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief LoLa Skeleton implement all binding specific functionalities that are needed by a Skeleton.
/// This includes all actions that need to be performed on Service offerings, as also the possibility to register events
/// dynamically at this skeleton.
class Skeleton final : public SkeletonBinding
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "SkeletonAttorney" class is a helper, which sets the internal state of "Skeleton" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonAttorney;

  public:
    static std::unique_ptr<Skeleton> Create(
        const InstanceIdentifier& identifier,
        score::filesystem::Filesystem filesystem,
        std::unique_ptr<IShmPathBuilder> shm_path_builder,
        std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder) noexcept;

    /// \brief Construct a Skeleton instance for this specific instance with possibility of passing mock objects during
    /// construction. It is only for testing. For production code Skeleton::Create shall be used.
    Skeleton(const InstanceIdentifier& identifier,
             const LolaServiceInstanceDeployment& lola_service_instance_deployment,
             const LolaServiceTypeDeployment& lola_service_type_deployment,
             score::filesystem::Filesystem filesystem,
             std::unique_ptr<IShmPathBuilder> shm_path_builder,
             std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder,
             std::optional<memory::shared::LockFile> service_instance_existence_marker_file,
             std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::ExclusiveFlockMutex>>
                 service_instance_existence_flock_mutex_and_lock);

    ~Skeleton() noexcept override = default;

    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;
    Skeleton(Skeleton&& other) = delete;
    Skeleton& operator=(Skeleton&& other) = delete;

    ResultBlank PrepareOffer(
        SkeletonEventBindings& events,
        SkeletonFieldBindings& fields,
        std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept override;

    void PrepareStopOffer(
        std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_callback) noexcept override;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    };

    /// \brief Enables dynamic registration of Events at the Skeleton.
    /// \tparam SampleType The type of the event
    /// \param element_fq_id The full qualified of the element (event or field) that shall be registered
    /// \param element_properties properties of the element, which are currently event specific properties.
    /// \return The registered data structures within the Skeleton
    /// (first: where to store data, second: control data
    ///         access) If PrepareOffer created the shared memory, then will create an EventDataControl (for QM and
    ///         optionally for ASIL B) and an EventDataStorage which will be returned. If PrepareOffer opened the shared
    ///         memory, then the opened event data from the existing shared memory will be returned.
    template <typename SampleType>
    std::pair<EventDataStorage<SampleType>*, EventDataControlComposite> Register(
        const ElementFqId element_fq_id,
        SkeletonEventProperties element_properties) noexcept;

    /// \brief Returns the meta-info for the given registered event.
    /// \param element_fq_id identification of the event.
    /// \return Events meta-info, if it has been registered, null else.
    score::cpp::optional<EventMetaInfo> GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept;

    QualityType GetInstanceQualityType() const noexcept;

    /// \brief Cleans up all allocated slots for this SkeletonEvent of any previous running instance
    /// \details Note: Only invoke _after_ a crash was detected!
    void CleanupSharedMemoryAfterCrash() noexcept;

    /// \brief "Disconnects" unsafe QM consumers by stop-offering the service instances QM part.
    /// \details Only supported/valid for a skeleton instance, where GetInstanceQualityType() returns
    ///          QualityType::kASIL_B. Calling it for a skeleton, which returns QualityType::kASIL_QM, will lead to
    ///          an assert/termination.
    void DisconnectQmConsumers() noexcept;

  private:
    ResultBlank OpenExistingSharedMemory(
        std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept;
    ResultBlank CreateSharedMemory(
        SkeletonEventBindings& events,
        SkeletonFieldBindings& fields,
        std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept;

    bool CreateSharedMemoryForData(
        const LolaServiceInstanceDeployment&,
        const std::size_t shm_size,
        std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept;
    bool CreateSharedMemoryForControl(const LolaServiceInstanceDeployment& instance,
                                      const QualityType asil_level,
                                      const std::size_t shm_size) noexcept;
    bool OpenSharedMemoryForData(
        const std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) noexcept;
    bool OpenSharedMemoryForControl(const QualityType asil_level) noexcept;
    void InitializeSharedMemoryForData(
        const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory) noexcept;
    void InitializeSharedMemoryForControl(
        const QualityType asil_level,
        const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory) noexcept;

    template <typename SampleType>
    std::pair<EventDataStorage<SampleType>*, EventDataControlComposite> OpenEventDataFromOpenedSharedMemory(
        const ElementFqId element_fq_id) noexcept;

    template <typename SampleType>
    std::pair<EventDataStorage<SampleType>*, EventDataControlComposite> CreateEventDataFromOpenedSharedMemory(
        const ElementFqId element_fq_id,
        const SkeletonEventProperties& element_properties) noexcept;

    class ShmResourceStorageSizes
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        const std::size_t data_size;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const std::size_t control_qm_size;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const score::cpp::optional<std::size_t> control_asil_b_size;
    };

    /// \brief Calculates needed sizes for shm-objects for data and ctrl either via simulation or a rough estimation
    /// depending on config.
    /// \return storage sizes for the different shm-objects
    ShmResourceStorageSizes CalculateShmResourceStorageSizes(SkeletonEventBindings& events,
                                                             SkeletonFieldBindings& fields) noexcept;
    /// \brief Calculates needed sizes for shm-objects for data and ctrl via simulation of allocations against a heap
    /// backed memory resource.
    /// \return storage sizes for the different shm-objects
    ShmResourceStorageSizes CalculateShmResourceStorageSizesBySimulation(SkeletonEventBindings& events,
                                                                         SkeletonFieldBindings& fields) noexcept;
    /// \brief Calculates needed sizes for shm-objects for data and ctrl via estimation based on sizeof info of related
    /// data types.
    /// \return storage sizes for the different shm-objects
    ShmResourceStorageSizes CalculateShmResourceStorageSizesByEstimation(SkeletonEventBindings& events,
                                                                         SkeletonFieldBindings& fields) const noexcept;

    void RemoveSharedMemory() noexcept;
    void RemoveStaleSharedMemoryArtefacts() const noexcept;

    InstanceIdentifier identifier_;
    const LolaServiceInstanceDeployment& lola_service_instance_deployment_;
    const LolaServiceTypeDeployment& lola_service_type_deployment_;
    LolaServiceInstanceId::InstanceId lola_instance_id_;
    LolaServiceTypeDeployment::ServiceId lola_service_id_;

    score::cpp::optional<std::string> data_storage_path_;
    score::cpp::optional<std::string> data_control_qm_path_;
    score::cpp::optional<std::string> data_control_asil_path_;
    ServiceDataStorage* storage_;
    ServiceDataControl* control_qm_;
    ServiceDataControl* control_asil_b_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> storage_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_qm_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_asil_resource_;

    std::unique_ptr<IShmPathBuilder> shm_path_builder_;
    std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder_;
    std::optional<memory::shared::LockFile> service_instance_existence_marker_file_;
    std::optional<memory::shared::LockFile> service_instance_usage_marker_file_;

    std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::ExclusiveFlockMutex>>
        service_instance_existence_flock_mutex_and_lock_;

    bool was_old_shm_region_reopened_;

    score::filesystem::Filesystem filesystem_;
};

namespace detail_skeleton
{

bool HasAsilBSupport(const InstanceIdentifier& identifier) noexcept;

}  // namespace detail_skeleton

template <typename SampleType>
auto Skeleton::Register(const ElementFqId element_fq_id, SkeletonEventProperties element_properties) noexcept
    -> std::pair<EventDataStorage<SampleType>*, EventDataControlComposite>
{
    // If the skeleton previously crashed and there are active proxies connected to the old shared memory, then we
    // re-open that shared memory in PrepareOffer(). In that case, we should retrieved the EventDataControl and
    // EventDataStorage from the shared memory and attempt to rollback the Skeleton tracing transaction log.
    if (was_old_shm_region_reopened_)
    {
        auto [typed_event_data_storage_ptr, event_data_control_composite] =
            OpenEventDataFromOpenedSharedMemory<SampleType>(element_fq_id);

        auto& event_data_control_qm = event_data_control_composite.GetQmEventDataControl();
        auto rollback_result = event_data_control_qm.GetTransactionLogSet().RollbackSkeletonTracingTransactions(
            [&event_data_control_qm](const TransactionLog::SlotIndexType slot_index) noexcept {
                event_data_control_qm.DereferenceEventWithoutTransactionLogging(slot_index);
            });
        if (!rollback_result.has_value())
        {
            ::score::mw::log::LogWarn("lola")
                << "SkeletonEvent: PrepareOffer failed: Could not rollback tracing consumer after "
                   "crash. Disabling tracing.";
            impl::Runtime::getInstance().GetTracingRuntime()->DisableTracing();
        }
        return {typed_event_data_storage_ptr, event_data_control_composite};
    }
    else
    {
        auto [typed_event_data_storage_ptr, event_data_control_composite] =
            CreateEventDataFromOpenedSharedMemory<SampleType>(element_fq_id, element_properties);

        return {typed_event_data_storage_ptr, event_data_control_composite};
    }
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 M3-2-2" rule finding. This rule declares: "The One Definition Rule shall not be
// violated.".
// The "Skeleton" is a template class with its declaration and definition in different places within the same header
// file, it does not violate the One Definition Rule
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". No way for 'OffsetPtr::get()' which called from 'event_data_storage_it->second.template' to throw an
// exception but we can't mark 'OffsetPtr::get()' as 'noexcept'.
// coverity[autosar_cpp14_m3_2_2_violation]
// coverity[autosar_cpp14_a15_5_3_violation]
auto Skeleton::OpenEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id) noexcept
    -> std::pair<EventDataStorage<SampleType>*, EventDataControlComposite>
{
    // Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
    // called implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception
    // if the key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()'
    // to throw an exception.
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    auto find_element = [](auto& map, const ElementFqId& target_element_fq_id) noexcept -> auto {
        const auto it = map.find(target_element_fq_id);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != map.cend(), "Could not find element fq id in map");
        return it;
    };

    score::cpp::ignore = find_element(storage_->events_metainfo_, element_fq_id);
    const auto event_data_storage_it = find_element(storage_->events_, element_fq_id);
    const auto event_control_qm_it = find_element(control_qm_->event_controls_, element_fq_id);

    EventDataControl* event_data_control_asil_b{nullptr};
    if (detail_skeleton::HasAsilBSupport(identifier_))
    {
        const auto event_control_asil_b_it = find_element(control_asil_b_->event_controls_, element_fq_id);
        // Suppress "AUTOSAR C++14 M7-5-1" rule. This rule declares:
        // A function shall not return a reference or a pointer to an automatic variable (including parameters), defined
        // within the function.
        // Suppress "AUTOSAR C++14 M7-5-2": The address of an object with automatic storage shall not be assigned to
        // another object that may persist after the first object has ceased to exist.
        // The result pointer is still valid outside this method until Skeleton object (as a holder) is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        event_data_control_asil_b = &(event_control_asil_b_it->second.data_control);
    }

    // Suppress "AUTOSAR C++14 A5-3-2" rule finding. This rule declares: "Null pointers shall not be dereferenced.".
    // The "event_data_storage_it" variable is an iterator of interprocess map returned by the "find_element" method.
    // A check is made that the iterator is not equal to map.cend(). Therefore, the call to "event_data_storage_it->"
    // does not return nullptr.
    // coverity[autosar_cpp14_a5_3_2_violation]
    auto* const typed_event_data_storage_ptr =
        event_data_storage_it->second.template get<EventDataStorage<SampleType>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event_data_storage_ptr != nullptr, "Could not get EventDataStorage*");

    // Suppress "AUTOSAR C++14 A3-8-1" rule findings. This rule declares:
    // "An object shall not be accessed outside of its lifetime"
    // The "event_data_control_asil_b" and "typed_event_data_storage_ptr" are still valid lifetime even returned pointer
    // to internal state until Skeleton object is alive.
    // coverity[autosar_cpp14_a3_8_1_violation]
    return {typed_event_data_storage_ptr,
            // The lifetime of the "event_data_control_asil_b" object lasts as long as the Skeleton is alive.
            // coverity[autosar_cpp14_m7_5_1_violation]
            // coverity[autosar_cpp14_m7_5_2_violation]
            // coverity[autosar_cpp14_a3_8_1_violation]
            EventDataControlComposite{&event_control_qm_it->second.data_control, event_data_control_asil_b}};
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 M3-2-2" rule finding. This rule declares: "The One Definition Rule shall not be
// violated.".
// The "Skeleton" is a template class with its declaration and definition in different places within the same header
// file, it does not violate the One Definition Rule
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no way to throw std::bad_variant_access.
// coverity[autosar_cpp14_m3_2_2_violation]
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto Skeleton::CreateEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id,
                                                     const SkeletonEventProperties& element_properties) noexcept
    -> std::pair<EventDataStorage<SampleType>*, EventDataControlComposite>
{
    auto* typed_event_data_storage_ptr = storage_resource_->construct<EventDataStorage<SampleType>>(
        element_properties.number_of_slots,
        memory::shared::PolymorphicOffsetPtrAllocator<SampleType>(storage_resource_->getMemoryResourceProxy()));

    auto inserted_data_slots = storage_->events_.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(element_fq_id),
                                                         std::forward_as_tuple(typed_event_data_storage_ptr));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_data_slots.second, "Couldn't register/emplace event-storage in data-section.");

    constexpr DataTypeMetaInfo sample_meta_info{sizeof(SampleType), static_cast<std::uint8_t>(alignof(SampleType))};
    auto* event_data_raw_array = typed_event_data_storage_ptr->data();
    auto inserted_meta_info =
        storage_->events_metainfo_.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(element_fq_id),
                                           std::forward_as_tuple(sample_meta_info, event_data_raw_array));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_meta_info.second, "Couldn't register/emplace event-meta-info in data-section.");

    auto control_qm =
        control_qm_->event_controls_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(element_fq_id),
                                             std::forward_as_tuple(element_properties.number_of_slots,
                                                                   element_properties.max_subscribers,
                                                                   element_properties.enforce_max_samples,
                                                                   control_qm_resource_->getMemoryResourceProxy()));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(control_qm.second, "Couldn't register/emplace event-meta-info in data-section.");

    EventDataControl* control_asil_result{nullptr};
    if (control_asil_resource_ != nullptr)
    {
        auto iterator = control_asil_b_->event_controls_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(element_fq_id),
            std::forward_as_tuple(element_properties.number_of_slots,
                                  element_properties.max_subscribers,
                                  element_properties.enforce_max_samples,
                                  control_asil_resource_->getMemoryResourceProxy()));

        // Suppress "AUTOSAR C++14 M7-5-1" rule. This rule declares:
        // A function shall not return a reference or a pointer to an automatic variable (including parameters), defined
        // within the function.
        // Suppress "AUTOSAR C++14 M7-5-2": The address of an object with automatic storage shall not be assigned to
        // another object that may persist after the first object has ceased to exist.
        // The result pointer is still valid outside this method until Skeleton object (as a holder) is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        control_asil_result = &iterator.first->second.data_control;
    }
    // clang-format off
    // The lifetime of the "control_asil_result" object lasts as long as the Skeleton is alive.
    // coverity[autosar_cpp14_m7_5_1_violation]
    // coverity[autosar_cpp14_m7_5_2_violation]
    // coverity[autosar_cpp14_a3_8_1_violation]
    return {typed_event_data_storage_ptr, EventDataControlComposite{&control_qm.first->second.data_control, control_asil_result}};
    // clang-format on
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_H
