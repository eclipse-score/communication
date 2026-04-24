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
#include "score/mw/com/impl/bindings/lola/messaging/method_call_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_subscription_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_unsubscription_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/methods/method_data.h"
#include "score/mw/com/impl/bindings/lola/methods/method_resource_map.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/skeleton_memory_manager.h"
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include "score/filesystem/filesystem.h"
#include "score/memory/shared/flock/exclusive_flock_mutex.h"
#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/lock_file.h"

#include <score/assert.hpp>
#include <score/optional.hpp>

#include <sys/types.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief LoLa Skeleton implement all binding specific functionalities that are needed by a Skeleton.
/// This includes all actions that need to be performed on Service offerings, as also the possibility to register
/// events dynamically at this skeleton.
class Skeleton final : public SkeletonBinding
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "SkeletonAttorney" class is a helper, which sets the internal state of "Skeleton" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonAttorney;

  public:
    template <typename SampleType>
    struct RegistrationResult
    {
        EventDataStorage<SampleType>& event_data_storage;
        EventDataControlComposite<> event_data_control_composite;
    };

    struct GenericRegistrationResult
    {
        void* type_erased_event_data_storage_ptr;
        EventDataControlComposite<> event_data_control_composite;
    };

    static std::unique_ptr<Skeleton> Create(const InstanceIdentifier& identifier,
                                            score::filesystem::Filesystem filesystem,
                                            std::unique_ptr<IShmPathBuilder> shm_path_builder,
                                            std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder);

    /// \brief Construct a Skeleton instance for this specific instance with possibility of passing mock objects
    /// during construction. It is only for testing. For production code Skeleton::Create shall be used.
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

    ResultBlank PrepareOffer(SkeletonEventBindings& events,
                             SkeletonFieldBindings& fields,
                             std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) override;

    void PrepareStopOffer(
        std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_callback) noexcept override;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    };

    /// \brief Enables dynamic registration of Generic (type-erased) Events at the Skeleton.
    /// \param element_fq_id The full qualified ID of the element (event) that shall be registered.
    /// \param element_properties Properties of the element (e.g. number of slots, max subscribers).
    /// \param sample_size The size of a single data sample in bytes.
    /// \param sample_alignment The alignment requirement of the data sample in bytes.
    /// \return A pair containing:
    ///         - An type erased pointer to the allocated data storage (void*).
    ///         - The EventDataControlComposite for managing the event's control data.
    auto RegisterGeneric(const ElementFqId element_fq_id,
                         const SkeletonEventProperties& element_properties,
                         const size_t sample_size,
                         const size_t sample_alignment) noexcept -> GenericRegistrationResult;

    /// \brief Enables dynamic registration of Events at the Skeleton.
    /// \tparam SampleType The type of the event
    /// \param element_fq_id The full qualified of the element (event or field) that shall be registered
    /// \param element_properties properties of the element, which are currently event specific properties.
    /// \return The registered data structures within the Skeleton
    /// (first: where to store data, second: control data
    ///         access) If PrepareOffer created the shared memory, then will create an EventDataControl (for QM and
    ///         optionally for ASIL B) and an EventDataStorage which will be returned. If PrepareOffer opened the
    ///         shared memory, then the opened event data from the existing shared memory will be returned.
    template <typename SampleType>
    auto Register(const ElementFqId element_fq_id, SkeletonEventProperties element_properties)
        -> RegistrationResult<SampleType>;

    QualityType GetInstanceQualityType() const;

    /// \brief Cleans up all allocated slots for this SkeletonEvent of any previous running instance
    /// \details Note: Only invoke _after_ a crash was detected!
    void CleanupSharedMemoryAfterCrash();

    /// \brief "Disconnects" unsafe QM consumers by stop-offering the service instances QM part.
    /// \details Only supported/valid for a skeleton instance, where GetInstanceQualityType() returns
    ///          QualityType::kASIL_B. Calling it for a skeleton, which returns QualityType::kASIL_QM, will lead to
    ///          an assert/termination.
    void DisconnectQmConsumers();

    /// \brief Function allowing a SkeletonMethod to register itself with its parent skeleton.
    ///
    /// This registration is required so that the Skeleton can access its owned methods.
    ///
    /// Synchronisation: This registration must be done in the constructor of the SkeletonMethod which is guaranteed to
    /// be called during construction of the full Skeleton created by the user. Therefore, the map of skeleton methods
    /// will not be modified after this construction phase and so can be used in any function (except for the
    /// constructor of Skeleton) without locking a mutex.
    void RegisterMethod(const UniqueMethodIdentifier method_id, SkeletonMethod& skeleton_method);

    bool VerifyAllMethodsRegistered() const override;

  private:
    ResultBlank OnServiceMethodsSubscribed(const ProxyInstanceIdentifier& proxy_instance_identifier,
                                           const uid_t proxy_uid,
                                           const QualityType asil_level,
                                           const pid_t proxy_pid);

    ResultBlank OnServiceMethodsUnsubscribed(const ProxyInstanceIdentifier& proxy_instance_identifier);

    using MethodIdsToUnsubscribe = std::vector<UniqueMethodIdentifier>;

    std::pair<score::ResultBlank, MethodIdsToUnsubscribe> SubscribeMethods(
        const MethodData& method_data,
        const ProxyInstanceIdentifier proxy_instance_identifier,
        const uid_t proxy_uid,
        const pid_t proxy_pid,
        const QualityType asil_level);
    void UnsubscribeMethods(const std::vector<UniqueMethodIdentifier>& method_ids,
                            const ProxyInstanceIdentifier& proxy_instance_identifier);

    static MethodData& GetMethodData(const memory::shared::ManagedMemoryResource& resource);

    /// \brief Gets the set of allowed proxy consumer IDs from the configuration
    IMessagePassingService::AllowedConsumerUids GetAllowedConsumers(const QualityType asil_level) const;

    InstanceIdentifier identifier_;
    QualityType quality_type_;
    LolaServiceInstanceId::InstanceId lola_instance_id_;
    LolaServiceTypeDeployment::ServiceId lola_service_id_;

    std::unique_ptr<IShmPathBuilder> shm_path_builder_;

    SkeletonMemoryManager memory_manager_;

    std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder_;
    std::optional<memory::shared::LockFile> service_instance_existence_marker_file_;
    std::optional<memory::shared::LockFile> service_instance_usage_marker_file_;

    std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::ExclusiveFlockMutex>>
        service_instance_existence_flock_mutex_and_lock_;

    /// \brief Mutex to synchronise calls to OnServiceMethodsSubscribed
    ///
    /// OnServiceMethodsSubscribed can be called by the message passing concurrently, since different Proxies could
    /// subscribe at the same time or a single Proxy may send the same message multiple times (See
    /// score/docs/features/ipc/lola/method/README.md for details).
    std::mutex on_service_methods_subscribed_mutex_;
    MethodResourceMap method_resources_;
    std::unordered_map<UniqueMethodIdentifier, std::reference_wrapper<SkeletonMethod>> skeleton_methods_;

    /// \brief RAII guard objects which will unregister a ServiceMethodSubscribedHandler/RegisterMethodCallHandler
    /// on destruction
    ///
    /// Each guard corresponds to the method subscription / method call handler which was registered in
    /// Skeleton::PrepareOffer() (in case any methods were registered). The guard objects will be destroyed in
    /// Skeleton::PrepareStopOffer()
    std::optional<MethodSubscriptionRegistrationGuard> method_subscription_registration_guard_qm_;
    std::optional<MethodSubscriptionRegistrationGuard> method_subscription_registration_guard_asil_b_;

    /// \brief RAII guard objects which will unregister a ServiceMethodUnsubscribedHandler on destruction
    ///
    /// Each guard corresponds to the method unsubscription handler which was registered in Skeleton::PrepareOffer().
    /// The guard objects will be destroyed in Skeleton::PrepareStopOffer().
    std::optional<MethodUnsubscriptionRegistrationGuard> method_unsubscription_registration_guard_qm_;
    std::optional<MethodUnsubscriptionRegistrationGuard> method_unsubscription_registration_guard_asil_b_;

    bool was_old_shm_region_reopened_;

    score::filesystem::Filesystem filesystem_;

    /// \brief Scope that is passed to the MethodCallHandler handler that is registered for each ProxyMethod
    ///
    /// This scope will be manually expired in PrepareStopOffer which will prevent any ProxyMethod from calling a
    /// method.
    safecpp::Scope<> method_call_handler_scope_;

    /// \brief Scope that is passed to the ServiceMethodSubscribedHandler
    ///
    /// This scope will be manually expired in PrepareStopOffer which will prevent any Proxy from subscribing to
    /// this method.
    safecpp::Scope<> on_service_method_subscribed_handler_scope_;
};

template <typename SampleType>
auto Skeleton::Register(const ElementFqId element_fq_id, SkeletonEventProperties element_properties)
    -> RegistrationResult<SampleType>
{
    // If the skeleton previously crashed and there are active proxies connected to the old shared memory, then we
    // re-open that shared memory in PrepareOffer(). In that case, we should retrieved the EventDataControl and
    // EventDataStorage from the shared memory and attempt to rollback the Skeleton tracing transaction log.
    if (was_old_shm_region_reopened_)
    {
        auto event_data_control_composite =
            memory_manager_.OpenEventDataControlCompositeFromOpenedSharedMemory(element_fq_id);

        auto& skeleton_event_data_control_local_qm = event_data_control_composite.GetQmEventDataControlLocal();
        memory_manager_.RollbackSkeletonTracingTransactions(
            skeleton_event_data_control_local_qm, skeleton_event_data_control_local_qm.GetTransactionLogSet());

        auto& event_data_storage = memory_manager_.OpenEventDataFromOpenedSharedMemory<SampleType>(element_fq_id);
        return {event_data_storage, event_data_control_composite};
    }

    auto& event_data_storage =
        memory_manager_.CreateEventDataInCreatedSharedMemory<SampleType>(element_fq_id, element_properties);
    auto event_data_control_composite =
        memory_manager_.CreateEventDataControlCompositeInCreatedSharedMemory(element_fq_id, element_properties);

    return RegistrationResult<SampleType>{event_data_storage, event_data_control_composite};
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_H
