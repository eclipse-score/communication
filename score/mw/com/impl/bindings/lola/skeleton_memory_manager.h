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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_MEMORY_MANAGER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_MEMORY_MANAGER_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/proxy_service_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/skeleton_service_data_control_local_view.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <score/assert.hpp>
#include <score/optional.hpp>

#include <sys/types.h>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief SkeletonMemoryManager manages shared memory related functionality of a Skeleton.
///
/// A SkeletonMemoryManager is owned and dispatched to by a Skeleton.
class SkeletonMemoryManager final
{
    // Suppress "AUTOSAR C++14 A11-3-1": Forbids the use of friend declarations.
    // Justification: The "SkeletonMemoryManagerTestAttorney" class is a helper, which sets the internal state of
    // "Skeleton" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonMemoryManagerTestAttorney;

  public:
    SkeletonMemoryManager(QualityType quality_type,
                          const IShmPathBuilder& shm_path_builder,
                          const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                          const LolaServiceTypeDeployment& lola_service_type_deployment,
                          const LolaServiceInstanceId::InstanceId lola_instance_id,
                          const LolaServiceTypeDeployment::ServiceId lola_service_id);

    ~SkeletonMemoryManager() noexcept = default;

    SkeletonMemoryManager(const SkeletonMemoryManager&) = delete;
    SkeletonMemoryManager& operator=(const SkeletonMemoryManager&) = delete;
    SkeletonMemoryManager(SkeletonMemoryManager&& other) = delete;
    SkeletonMemoryManager& operator=(SkeletonMemoryManager&& other) = delete;

    ResultBlank OpenExistingSharedMemory(
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);
    ResultBlank CreateSharedMemory(
        SkeletonBinding::SkeletonEventBindings& events,
        SkeletonBinding::SkeletonFieldBindings& fields,
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);

    template <typename SampleType>
    std::pair<EventDataStorage<SampleType>*, EventDataControlComposite<>> CreateEventDataFromOpenedSharedMemory(
        const ElementFqId element_fq_id,
        const SkeletonEventProperties& element_properties);

    /// \brief Creates the control structures (QM and optional ASIL-B) for an event.
    /// \param element_fq_id The full qualified ID of the element.
    /// \param element_properties Properties of the event.
    /// \return The EventDataControlComposite containing pointers to the control structures.
    EventDataControlComposite<> CreateEventControlComposite(const ElementFqId element_fq_id,
                                                            const SkeletonEventProperties& element_properties) noexcept;

    /// \brief Creates shared memory storage for a generic (type-erased) event.
    /// \param element_fq_id The full qualified ID of the element.
    /// \param element_properties Properties of the event.
    /// \param sample_size The size of a single data sample.
    /// \param sample_alignment The alignment of the data sample.
    /// \return A pair containing the data storage pointer (void*) and the control composite.
    std::pair<void*, EventDataControlComposite<>> CreateEventDataFromOpenedSharedMemory(
        const ElementFqId element_fq_id,
        const SkeletonEventProperties& element_properties,
        size_t sample_size,
        size_t sample_alignment) noexcept;

    template <typename SampleType>
    std::pair<EventDataStorage<SampleType>*, EventDataControlComposite<>> OpenEventDataFromOpenedSharedMemory(
        const ElementFqId element_fq_id);

    void RemoveSharedMemory();
    void RemoveStaleSharedMemoryArtefacts() const;

    /// \brief Cleans up all allocated slots for this SkeletonEvent of any previous running instance
    /// \details Note: Only invoke _after_ a crash was detected!
    void CleanupSharedMemoryAfterCrash();

    void Reset();

  private:
    class ShmResourceStorageSizes
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1": All non-POD class types should only have private member data.
        // Justification: There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::size_t data_size;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::size_t control_qm_size;
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::optional<std::size_t> control_asil_b_size;
    };

    /// \brief Calculates needed sizes for shm-objects for data and ctrl either via simulation or a rough estimation
    /// depending on config.
    /// \return storage sizes for the different shm-objects
    ShmResourceStorageSizes CalculateShmResourceStorageSizes(SkeletonBinding::SkeletonEventBindings& events,
                                                             SkeletonBinding::SkeletonFieldBindings& fields);
    /// \brief Calculates needed sizes for shm-objects for data and ctrl via simulation of allocations against a heap
    /// backed memory resource.
    /// \return storage sizes for the different shm-objects
    ShmResourceStorageSizes CalculateShmResourceStorageSizesBySimulation(
        SkeletonBinding::SkeletonEventBindings& events,
        SkeletonBinding::SkeletonFieldBindings& fields);

    bool CreateSharedMemoryForData(
        const LolaServiceInstanceDeployment&,
        const std::size_t shm_size,
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);
    bool CreateSharedMemoryForControl(const LolaServiceInstanceDeployment& instance,
                                      const QualityType asil_level,
                                      const std::size_t shm_size);
    bool OpenSharedMemoryForData(
        const std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);
    bool OpenSharedMemoryForControl(const QualityType asil_level);
    void InitializeSharedMemoryForData(const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory);
    void InitializeSharedMemoryForControl(const QualityType asil_level,
                                          const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory);

    std::pair<std::reference_wrapper<EventControl>, std::reference_wrapper<SkeletonEventControlLocalView>>
    InsertEventInServiceDataControl(const QualityType asil_level,
                                    ElementFqId element_fq_id,
                                    const SkeletonEventProperties& element_properties);

    QualityType quality_type_;
    const IShmPathBuilder& shm_path_builder_;
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
    std::optional<SkeletonServiceDataControlLocalView> skeleton_control_qm_local_;
    std::optional<SkeletonServiceDataControlLocalView> skeleton_control_asil_b_local_;

    // Used for tracing. Only created if tracing is enabled in the current process.
    std::optional<ProxyServiceDataControlLocalView> proxy_control_local_;

    std::shared_ptr<score::memory::shared::ManagedMemoryResource> storage_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_qm_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_asil_resource_;
};

template <typename SampleType>
// Suppress "AUTOSAR C++14 M3-2-2": ODR (One Definition Rule) must not be violated.
// Justification: The "Skeleton" is a template class with its declaration and definition in different places within the
// same header file, it does not violate the One Definition Rule.
// Suppress "AUTOSAR C++14 A15-5-3": std::terminate() should not be called implicitly.
// Justification: This is a false positive, no way to throw std::bad_variant_access.
// coverity[autosar_cpp14_m3_2_2_violation]
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonMemoryManager::CreateEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id,
                                                                  const SkeletonEventProperties& element_properties)
    -> std::pair<EventDataStorage<SampleType>*, EventDataControlComposite<>>
{
    auto* typed_event_data_storage_ptr = storage_resource_->construct<EventDataStorage<SampleType>>(
        element_properties.number_of_slots,
        memory::shared::PolymorphicOffsetPtrAllocator<SampleType>(*storage_resource_));

    auto inserted_data_slots = storage_->events_.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(element_fq_id),
                                                         std::forward_as_tuple(typed_event_data_storage_ptr));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_data_slots.second,
                                                "Couldn't register/emplace event-storage in data-section.");

    constexpr DataTypeMetaInfo sample_meta_info{sizeof(SampleType), static_cast<std::uint8_t>(alignof(SampleType))};
    auto* event_data_raw_array = typed_event_data_storage_ptr->data();
    auto inserted_meta_info =
        storage_->events_metainfo_.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(element_fq_id),
                                           std::forward_as_tuple(sample_meta_info, event_data_raw_array));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_meta_info.second,
                                                "Couldn't register/emplace event-meta-info in data-section.");

    return {typed_event_data_storage_ptr, CreateEventControlComposite(element_fq_id, element_properties)};
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 M3-2-2":
// Justification: Same justification as above.
// Suppress "AUTOSAR C++14 A15-5-3":
// Justification: No way for 'OffsetPtr::get()' which called from 'event_data_storage_it->second.template' to throw an
// exception but we can't mark 'OffsetPtr::get()' as ''.
// coverity[autosar_cpp14_m3_2_2_violation]
// coverity[autosar_cpp14_a15_5_3_violation]
auto SkeletonMemoryManager::OpenEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id)
    -> std::pair<EventDataStorage<SampleType>*, EventDataControlComposite<>>
{
    // Suppress "AUTOSAR C++14 A15-5-3":
    // Justification: This is a false positive, std::less which is used by std::map::find could throw an exception if
    // the key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to
    // throw an exception.
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    auto find_element = [](auto& map, const ElementFqId& target_element_fq_id) -> auto {
        const auto it = map.find(target_element_fq_id);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != map.cend(), "Could not find element fq id in map");
        return it;
    };

    score::cpp::ignore = find_element(storage_->events_metainfo_, element_fq_id);
    const auto event_data_storage_it = find_element(storage_->events_, element_fq_id);
    const auto event_control_qm_it = find_element(skeleton_control_qm_local_->event_controls_, element_fq_id);

    SkeletonEventDataControlLocalView<>* event_data_control_asil_b_local{nullptr};
    if (quality_type_ == QualityType::kASIL_B)
    {
        const auto event_control_asil_b_it =
            find_element(skeleton_control_asil_b_local_->event_controls_, element_fq_id);
        // Suppress "AUTOSAR C++14 M7-5-1": Functions should not return references or pointers to automatic variables
        // defined in the function.
        // Suppress "AUTOSAR C++14 M7-5-2": Should not assign an object with automatic storage to another which could
        // persist after the first objects has been destroyed.
        // Suppress "AUTOSAR C++14 A3-8-1": Don't access an object before construction or after destruction.
        // Jutification: The lifetime of the object whose address is assigned to "event_data_control_asil_b" is owned by
        // the Skeleton itself. The pointer is passed to EventDataControlComposite which is then owned by a
        // SkeletonEvent which is guaranteed to be destroyed before the Skeleton is destroyed (since it's a member of
        // the parent Skeleton).
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        event_data_control_asil_b_local = &(event_control_asil_b_it->second.data_control);
    }

    // Suppress "AUTOSAR C++14 A5-3-2": Don't dereference null pointers.
    // Justification: The "event_data_storage_it" variable is an iterator of interprocess map returned by the
    // "find_element" method. A check is made that the iterator is not equal to map.cend(). Therefore, the call to
    // "event_data_storage_it->" does not return nullptr.
    // coverity[autosar_cpp14_a5_3_2_violation]
    auto* const typed_event_data_storage_ptr =
        event_data_storage_it->second.template get<EventDataStorage<SampleType>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event_data_storage_ptr != nullptr,
                                                "Could not get EventDataStorage*");

    ProxyEventDataControlLocalView<>* proxy_event_data_control_local{nullptr};
    if (proxy_control_local_.has_value())
    {
        const auto proxy_event_control_local_it = find_element(proxy_control_local_->event_controls_, element_fq_id);
        // Suppression rationale: The result pointer is still valid outside this method until Skeleton object (as a
        // holder) is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        proxy_event_data_control_local = &(proxy_event_control_local_it->second.data_control);
    }

    // Suppress "AUTOSAR C++14 A3-8-1":
    // Justification: The "event_data_control_asil_b" and "typed_event_data_storage_ptr" are still valid lifetime even
    // returned pointer to internal state until Skeleton object is alive.
    // coverity[autosar_cpp14_a3_8_1_violation]
    return {
        typed_event_data_storage_ptr,
        // The lifetime of the "event_data_control_asil_b" object lasts as long as the Skeleton is alive.
        // coverity[autosar_cpp14_m7_5_1_violation]
        // coverity[autosar_cpp14_m7_5_2_violation]
        // coverity[autosar_cpp14_a3_8_1_violation]
        EventDataControlComposite{
            event_control_qm_it->second.data_control, event_data_control_asil_b_local, proxy_event_data_control_local}};
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_MEMORY_MANAGER_H
