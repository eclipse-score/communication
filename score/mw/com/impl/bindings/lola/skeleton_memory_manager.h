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
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/initialize_sample_callback.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/managed_memory_resource.h"

#include <score/assert.hpp>

#include <sys/types.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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

    /// \brief The EventControl(s) composing a CoherencyType::kNonCoherent event's control section: a QM part (always
    /// present) and, only if the Skeleton is of ASIL-B quality type, an ASIL-B part.
    struct EventControlNonCoherentPair
    {
        std::reference_wrapper<EventControl> qm;
        EventControl* asil_b;  ///< nullptr unless the Skeleton is of QualityType::kASIL_B.
    };

    /// \brief The EventControlCoherent(s) composing a CoherencyType::kCoherent event's control section: a QM part
    /// (always present) and, only if the Skeleton is of ASIL-B quality type, an ASIL-B part.
    /// \details Just like a CoherencyType::kNonCoherent event, a CoherencyType::kCoherent event is placed in both
    /// control sections (QM and, if present, ASIL-B) - never just in one of them. Even though kCoherent consumers
    /// never alter any per-slot state in the CONTROL segment, the two control sections still need to remain
    /// strictly isolated: access control to each section is configured independently (an ASIL-B consumer might not
    /// even have read access to the QM section), and the QM section is anyway writable by QM consumers of other,
    /// kNonCoherent, events of the same service-instance.
    struct EventControlCoherentPair
    {
        std::reference_wrapper<EventControlCoherent> qm;
        EventControlCoherent* asil_b;  ///< nullptr unless the Skeleton is of QualityType::kASIL_B.
    };

    /// \brief The EventControlMixed(s) composing a CoherencyType::kMixed event's control section: a QM part (always
    /// present) and, only if the Skeleton is of ASIL-B quality type, an ASIL-B part.
    /// \details Just like a CoherencyType::kCoherent event, a CoherencyType::kMixed event is placed in both control
    /// sections (QM and, if present, ASIL-B) - never just in one of them, for the same reasons (strict isolation of
    /// the QM and ASIL-B control sections, see EventControlCoherentPair).
    struct EventControlMixedPair
    {
        std::reference_wrapper<EventControlMixed> qm;
        EventControlMixed* asil_b;  ///< nullptr unless the Skeleton is of QualityType::kASIL_B.
    };

    /// \brief Result of creating / retrieving the EventControl(s) for a specific event: either an
    /// EventControlNonCoherentPair (CoherencyType::kNonCoherent), an EventControlCoherentPair
    /// (CoherencyType::kCoherent) or an EventControlMixedPair (CoherencyType::kMixed). Which alternative is active is
    /// implied by the active variant index, making the previously implicit "exactly one of these pointers is
    /// non-null" invariant explicit in the type.
    using EventControlsResult =
        std::variant<EventControlNonCoherentPair, EventControlCoherentPair, EventControlMixedPair>;

    /// \brief Create and initialize data and control shared memory segments
    ///
    /// This function is called by a Skeleton during PrepareOffer in case we aren't in a partial restart case (or we are
    /// but there are no proxies connected to the old shared memory region). It will create the data and control shared
    /// memory regions and will initialize the ServiceDataControl and ServiceDataStorage structures in the created
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

    /// \brief Creates the EventControl(s) for a specific event in the shared memory regions created with
    /// CreateSharedMemory.
    ///
    /// An EventControl (CoherencyType::kNonCoherent) or EventControlCoherent (CoherencyType::kCoherent) is emplaced
    /// for QM and, if the Skeleton is ASIL-B, also for ASIL-B (see EventControlsResult).
    auto CreateEventControlsInCreatedSharedMemory(const ElementFqId element_fq_id,
                                                  const SkeletonEventProperties& element_properties)
        -> EventControlsResult;

    /// \brief Creates (type erased) shared memory storage for an event.
    /// \details Since the event data storage on binding layer is generally type-erased, this functionality is used for
    ///          generic events as well as for "normal"/typed events.
    /// \param element_fq_id The full qualified ID of the element.
    /// \param element_properties Properties of the event.
    /// \param sample_size_info Information about the size and alignment of the data sample.
    /// \param initialize_sample_callback Optional callback used to initialize (default-construct) every newly
    ///        created storage slot.
    /// \return A reference to the EventDataStorage.
    auto CreateEventDataInCreatedSharedMemory(
        const ElementFqId element_fq_id,
        const SkeletonEventProperties& element_properties,
        memory::DataTypeSizeInfo sample_size_info,
        const std::optional<InitializeSampleCallback>& initialize_sample_callback = std::nullopt) -> EventDataStorage&;

    /// \brief Opens the EventControl(s) for a specific event that were created by a previous skeleton.
    ///
    /// The EventControl(s) are retrieved from the ServiceDataControl in the shared memory regions that were opened
    /// with OpenExistingSharedMemory. \p element_properties is used to determine the event's coherency (and hence
    /// which map / EventControl(s) to retrieve). See EventControlsResult for details.
    auto RetrieveEventControlsFromOpenedSharedMemory(const ElementFqId element_fq_id,
                                                     const SkeletonEventProperties& element_properties)
        -> EventControlsResult;

    /// \brief Returns an EventDataStorage for a specific event that was created by a previous skeleton.
    ///
    /// The EventDataStorage are retrieved from the ServiceDataStorage in the shared memory region that was opened with
    /// OpenExistingSharedMemory.
    auto RetrieveEventDataFromOpenedSharedMemory(const ElementFqId element_fq_id) -> EventDataStorage&;

    /// \brief Rolls back any existing operations in the TransactionLog corresponding to a SkeletonEvent
    ///
    /// The TransactionLog would only exist if a SkeletonEvent in a crashed process had tracing enabled. If tracing was
    /// not enabled, then this function will simply do nothing.
    void RollbackSkeletonTracingTransactions(EventControl& event_control);

    /// \brief Rolls back any existing operations in the TransactionLog corresponding to a CoherencyType::kMixed
    /// SkeletonEvent, mirroring RollbackSkeletonTracingTransactions(EventControl&).
    /// \details Unlike CoherencyType::kCoherent (which has no transaction log at all), a CoherencyType::kMixed event
    /// has a full transaction_log_set_ (see EventControlMixed), so the same crash-recovery rollback is needed as for
    /// a CoherencyType::kNonCoherent event.
    void RollbackSkeletonTracingTransactions(EventControlMixed& event_control);

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
        std::optional<std::size_t> control_asil_b_size;
    };

    /// \brief Calculates needed sizes for shm-objects for data and ctrl either via simulation or an analytic estimation
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

    /// \brief Calculates the needed size for the data shm-object (holding the ServiceDataStorage) analytically.
    /// \details Does NOT allocate any (heap) memory and does not create a ServiceDataStorage. It collects the exact
    /// per service-element sizing information (exact slot-array size/alignment, distinguishing typed and generic
    /// events/fields) from the handed-over event/field bindings and the deployment configuration and delegates the
    /// actual layout math to CalculateServiceDataStorageShmSize (located next to ServiceDataStorage), which computes
    /// the exact size needed.
    /// \return needed size (in bytes) for the data shm-object.
    std::size_t CalculateDataShmResourceStorageSize(SkeletonBinding::SkeletonEventBindings& events,
                                                    SkeletonBinding::SkeletonFieldBindings& fields) const;

    /// \brief Calculates the needed size for a control shm-object (holding a ServiceDataControl) analytically.
    /// \details Does NOT allocate any (heap) memory and does not create a ServiceDataControl. It collects the per
    /// service-element sizing information (number of slots, max-subscribers) from the handed-over event/field bindings
    /// and the deployment configuration and delegates the actual layout math to CalculateServiceDataControlShmSize
    /// (located next to ServiceDataControl), which computes the exact size needed.
    /// The same size applies to the QM and (if present) the ASIL-B control shm-object, as both hold a
    /// ServiceDataControl created with the very same configuration.
    /// \return needed size (in bytes) for a single control shm-object.
    std::size_t CalculateControlShmResourceStorageSize(SkeletonBinding::SkeletonEventBindings& events,
                                                       SkeletonBinding::SkeletonFieldBindings& fields) const;

    /// \brief Looks up the configured number of sample-slots for the given service-element.
    /// \param service_element_name name of the event/field.
    std::size_t GetNumberOfSampleSlotsFromConfig(const std::string_view service_element_name,
                                                 const bool is_field) const;

    /// \brief Looks up the configured maximum number of subscribers for the given service-element.
    /// \param service_element_name name of the event/field.
    std::size_t GetMaxSubscribersFromConfig(const std::string_view service_element_name, const bool is_field) const;

    /// \brief Looks up the configured coherency for the given service-element.
    /// \param service_element_name name of the event/field.
    CoherencyType GetCoherencyFromConfig(const std::string_view service_element_name, const bool is_field) const;

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

    EventControl& EmplaceEventControl(const QualityType asil_level,
                                      ElementFqId element_fq_id,
                                      const SkeletonEventProperties& element_properties);

    /// \brief Emplaces a single EventControlCoherent into the event_controls_coherent_ map of the control section
    /// identified by asil_level (QM or ASIL-B), mirroring EmplaceEventControl.
    EventControlCoherent& EmplaceEventControlCoherent(const QualityType asil_level,
                                                      ElementFqId element_fq_id,
                                                      const SkeletonEventProperties& element_properties);

    /// \brief Emplaces a single EventControlMixed into the event_controls_mixed_ map of the control section
    /// identified by asil_level (QM or ASIL-B), mirroring EmplaceEventControlCoherent.
    EventControlMixed& EmplaceEventControlMixed(const QualityType asil_level,
                                                ElementFqId element_fq_id,
                                                const SkeletonEventProperties& element_properties);

    QualityType quality_type_;
    const IShmPathBuilder& shm_path_builder_;
    const LolaServiceInstanceDeployment& lola_service_instance_deployment_;
    const LolaServiceTypeDeployment& lola_service_type_deployment_;
    LolaServiceInstanceId::InstanceId lola_instance_id_;
    LolaServiceTypeDeployment::ServiceId lola_service_id_;

    std::optional<std::string> data_storage_path_;
    std::optional<std::string> data_control_qm_path_;
    std::optional<std::string> data_control_asil_path_;

    ServiceDataStorage* storage_;
    ServiceDataControl* control_qm_;
    ServiceDataControl* control_asil_b_;

    /// \brief Number of events + fields of the service-instance.
    /// \details Determines the fixed capacity of the ServiceDataStorage container (data shm-object), which is
    /// agnostic of an event's/field's configured coherency. Empty until it is set in CreateSharedMemory (before the
    /// ServiceDataStorage / ServiceDataControl are constructed). Every read site asserts that the value has been set,
    /// so an accidental use before initialization is caught deterministically instead of silently defaulting to a
    /// (potentially valid) capacity of 0.
    std::optional<std::size_t> number_of_events_and_fields_;

    /// \brief Number of CoherencyType::kNonCoherent events + fields of the service-instance.
    /// \details Determines the fixed capacity of the event_controls_ container within ServiceDataControl. Set
    /// alongside number_of_events_and_fields_ in CreateSharedMemory.
    std::optional<std::size_t> number_of_non_coherent_events_and_fields_;

    /// \brief Number of CoherencyType::kCoherent events + fields of the service-instance.
    /// \details Determines the fixed capacity of the event_controls_coherent_ container within ServiceDataControl.
    /// Set alongside number_of_events_and_fields_ in CreateSharedMemory.
    std::optional<std::size_t> number_of_coherent_events_and_fields_;

    /// \brief Number of CoherencyType::kMixed events + fields of the service-instance.
    /// \details Determines the fixed capacity of the event_controls_mixed_ container within ServiceDataControl. Set
    /// alongside number_of_events_and_fields_ in CreateSharedMemory.
    std::optional<std::size_t> number_of_mixed_events_and_fields_;

    std::shared_ptr<score::memory::shared::ManagedMemoryResource> storage_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_qm_resource_;
    std::shared_ptr<score::memory::shared::ManagedMemoryResource> control_asil_resource_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_MEMORY_MANAGER_H
