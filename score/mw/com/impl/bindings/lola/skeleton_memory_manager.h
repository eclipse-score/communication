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

    /// \brief Create and initialize data and control shared memory segments
    ///
    /// This function is called by a Skeleton during PrepareOffer in case we aren't in a partial restart case (or we are
    /// but there are no proxies connected to the old shared memory region). It will create the data and control shared
    /// memory regions and will initialise the ServiceDataControl and ServiceDataStorage structures in the created
    /// shared memory.
    Result<void> CreateSharedMemory(
        SkeletonBinding::SkeletonEventBindings& events,
        SkeletonBinding::SkeletonFieldBindings& fields,
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);

    /// \brief Open data and control shared memory segments that were created by a previous Skeleton
    ///
    /// This function is called by a Skeleton during PrepareOffer in case we are in a partial restart case and there are
    /// still proxies connected to the old shared memory region. It will open the data and control shared memory regions
    /// and will store pointers to the already existing ServiceDataControl and ServiceDataStorage structures in the
    /// opened shared memory.
    Result<void> OpenExistingSharedMemory(
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);

    /// \brief Creates an EventDataControlComposite and TransactionLogSet for a specific event.
    ///
    /// The EventDataControlComposite and TransactionLogSet are emplaced into the ServiceDataControl in the shared
    /// memory region that was created with CreateSharedMemory.
    auto CreateEventDataControlCompositeInCreatedSharedMemory(const ElementFqId element_fq_id,
                                                              const SkeletonEventProperties& element_properties)
        -> EventDataControlComposite<>;

    /// \brief Creates an EventDataStorage for a specific event.
    ///
    /// The EventDataStorage are emplaced into the ServiceDataStorage in the shared memory region that was created with
    /// CreateSharedMemory.
    template <typename SampleType>
    auto CreateEventDataInCreatedSharedMemory(const ElementFqId element_fq_id,
                                              const SkeletonEventProperties& element_properties)
        -> EventDataStorage<SampleType>&;

    /// \brief Creates shared memory storage for a generic (type-erased) event.
    /// \param element_fq_id The full qualified ID of the element.
    /// \param element_properties Properties of the event.
    /// \param sample_size The size of a single data sample.
    /// \param sample_alignment The alignment of the data sample.
    /// \return A pair containing the data storage pointer (void*) and the control composite.
    auto CreateGenericEventDataInCreatedSharedMemory(const ElementFqId element_fq_id,
                                                     const SkeletonEventProperties& element_properties,
                                                     size_t sample_size,
                                                     size_t sample_alignment) noexcept -> void*;

    /// \brief Opens an EventDataControlComposite and TransactionLogSet for a specific event that were created by a
    /// previous skeleton.
    ///
    /// The EventDataControlComposite and TransactionLogSet are retrieved from the ServiceDataControl in the shared
    /// memory region that was opened with OpenExistingSharedMemory.
    auto OpenEventDataControlCompositeFromOpenedSharedMemory(const ElementFqId element_fq_id)
        -> EventDataControlComposite<>;

    /// \brief Opens an EventDataStorage for a specific event that was created by a previous skeleton.
    ///
    /// The EventDataStorage are retrieved from the ServiceDataStorage in the shared memory region that was opened with
    /// OpenExistingSharedMemory.
    template <typename SampleType>
    auto OpenEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id) -> EventDataStorage<SampleType>&;

    /// \brief Rolls back any existing operations in the TransactionLog corresponding to a SkeletonEvent
    ///
    /// The TransactionLog would only exist if a SkeletonEvent in a crashed process had tracing enabled.
    void RollbackSkeletonTracingTransactions(SkeletonEventDataControlLocalView<>& skeleton_event_data_control_local,
                                             TransactionLogSet& transaction_log_set);

    /// \brief Remove the control and data shared memory regions
    void RemoveSharedMemory();

    /// \brief Removes stale shared memory artefacts from the filesystem in case a process crashed while creating a
    ///        SharedMemoryResource.
    void RemoveStaleSharedMemoryArtefacts() const;

    /// \brief Cleans up all allocated slots for this SkeletonEvent of any previous running instance
    /// \details Note: Only invoke _after_ a crash was detected!
    void CleanupSharedMemoryAfterCrash();

    /// \brief Resets internal state of SkeletonMemoryManager.
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

    /// Functions for creating / opening / initializing shared memory within PrepareOffer.
    bool CreateSharedMemoryForData(
        const LolaServiceInstanceDeployment& lola_service_instance_deployment,
        const std::size_t shm_size,
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);
    bool CreateSharedMemoryForControl(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                                      const QualityType asil_level,
                                      const std::size_t shm_size);
    bool OpenSharedMemoryForData(
        const std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback);
    bool OpenSharedMemoryForControl(const QualityType asil_level);
    void InitializeSharedMemoryForData(const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory);
    void InitializeSharedMemoryForControl(const QualityType asil_level,
                                          const std::shared_ptr<score::memory::shared::ManagedMemoryResource>& memory);

    /// Functions for creating / opening event data controls and storages for events registered via Register() or
    /// RegisterGeneric().
    std::pair<std::reference_wrapper<SkeletonEventControlLocalView>, SkeletonEventControlLocalView*>
    EmplaceQmAndAsilBEventControlAndLocalView(const ElementFqId element_fq_id,
                                              const SkeletonEventProperties& element_properties);

    SkeletonEventControlLocalView& EmplaceEventControlAndLocalView(const QualityType asil_level,
                                                                   ElementFqId element_fq_id,
                                                                   const SkeletonEventProperties& element_properties);

    template <typename SampleType>
    EventDataStorage<SampleType>& EmplaceEventDataStorage(const ElementFqId element_fq_id,
                                                          const SkeletonEventProperties& element_properties);
    ProxyEventControlLocalView* EmplaceTracingEventControl(const ElementFqId element_fq_id);

    EventMetaInfo& EmplaceEventMetaInfo(const ElementFqId element_fq_id,
                                        const DataTypeMetaInfo& sample_meta_info,
                                        void* type_erased_event_data_storage);

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
auto SkeletonMemoryManager::CreateEventDataInCreatedSharedMemory(const ElementFqId element_fq_id,
                                                                 const SkeletonEventProperties& element_properties)
    -> EventDataStorage<SampleType>&
{
    auto& event_data_storage = EmplaceEventDataStorage<SampleType>(element_fq_id, element_properties);

    constexpr DataTypeMetaInfo sample_meta_info{sizeof(SampleType), static_cast<std::uint8_t>(alignof(SampleType))};
    auto* const event_data_raw_array = event_data_storage.data();
    score::cpp::ignore = EmplaceEventMetaInfo(element_fq_id, sample_meta_info, event_data_raw_array);

    return event_data_storage;
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
    -> EventDataStorage<SampleType>&
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

    // Suppress "AUTOSAR C++14 A5-3-2": Don't dereference null pointers.
    // Justification: The "event_data_storage_it" variable is an iterator of interprocess map returned by the
    // "find_element" method. A check is made that the iterator is not equal to map.cend(). Therefore, the call to
    // "event_data_storage_it->" does not return nullptr.
    // coverity[autosar_cpp14_a5_3_2_violation]
    auto* const typed_event_data_storage_ptr =
        event_data_storage_it->second.template get<EventDataStorage<SampleType>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event_data_storage_ptr != nullptr,
                                                "Could not get EventDataStorage*");

    return *typed_event_data_storage_ptr;
}

template <typename SampleType>
EventDataStorage<SampleType>& SkeletonMemoryManager::EmplaceEventDataStorage(
    const ElementFqId element_fq_id,
    const SkeletonEventProperties& element_properties)
{
    auto* typed_event_data_storage_ptr = storage_resource_->construct<EventDataStorage<SampleType>>(
        element_properties.number_of_slots,
        memory::shared::PolymorphicOffsetPtrAllocator<SampleType>(*storage_resource_));

    auto inserted_data_slots = storage_->events_.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(element_fq_id),
                                                         std::forward_as_tuple(typed_event_data_storage_ptr));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_data_slots.second,
                                                "Couldn't register/emplace event-storage in data-section.");
    return *typed_event_data_storage_ptr;
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_MEMORY_MANAGER_H
