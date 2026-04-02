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

#include "score/memory/shared/managed_memory_resource.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/skeleton_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/runtime.h"
#include "score/result/result.h"

#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/mw/log/logging.h"
#include "score/os/stat.h"

#include <score/assert.hpp>
#include <score/span.hpp>
#include <sys/types.h>

#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

const LolaServiceTypeDeployment& GetLolaServiceTypeDeployment(const InstanceIdentifier& identifier)
{
    const auto& service_type_depl_info = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment_ptr =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    if (lola_service_type_deployment_ptr == nullptr)
    {
        score::mw::log::LogError("lola") << "GetLolaServiceTypeDeployment: Wrong Binding! ServiceTypeDeployment "
                                            "doesn't contain a LoLa deployment!";
        std::terminate();
    }
    return *lola_service_type_deployment_ptr;
}

const LolaServiceInstanceDeployment& GetLolaServiceInstanceDeployment(const InstanceIdentifier& identifier)
{
    const auto& instance_depl_info = InstanceIdentifierView{identifier}.GetServiceInstanceDeployment();
    const auto* lola_service_instance_deployment_ptr =
        std::get_if<LolaServiceInstanceDeployment>(&instance_depl_info.bindingInfo_);
    if (lola_service_instance_deployment_ptr == nullptr)
    {
        score::mw::log::LogError("lola")
            << "GetLolaServiceInstanceDeployment: Wrong Binding! ServiceInstanceDeployment "
               "doesn't contain a LoLa deployment!";
        std::terminate();
    }
    return *lola_service_instance_deployment_ptr;
}

bool CreatePartialRestartDirectory(const score::filesystem::Filesystem& filesystem,
                                   const IPartialRestartPathBuilder& partial_restart_path_builder)
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
    const IPartialRestartPathBuilder& partial_restart_path_builder)
{
    auto service_instance_existence_marker_file_path =
        partial_restart_path_builder.GetServiceInstanceExistenceMarkerFilePath(lola_instance_id);

    // The instance existence marker file can be opened in the case that another skeleton of the same service
    // currently exists or that a skeleton of the same service previously crashed. We cannot determine which is true
    // until we try to flock the file. Therefore, we do not take ownership on construction and take ownership later
    // if we can exclusively flock the file.
    bool take_ownership{false};
    return memory::shared::LockFile::CreateOrOpen(std::move(service_instance_existence_marker_file_path),
                                                  take_ownership);
}

std::optional<memory::shared::LockFile> CreateOrOpenServiceInstanceUsageMarkerFile(
    const LolaServiceInstanceId::InstanceId lola_instance_id,
    const IPartialRestartPathBuilder& partial_restart_path_builder)
{
    auto service_instance_usage_marker_file_path =
        partial_restart_path_builder.GetServiceInstanceUsageMarkerFilePath(lola_instance_id);

    // The instance usage marker file should be created if the skeleton is starting up for the very first time and
    // opened in all other cases.
    // Initially, when creating/opening the usage marker file, we do not take ownership of it!
    // Because we don't want to have automatic file-system unlinking in place, when we destroy/stop-offer the
    // skeleton instance! We decide, whether to unlink the file or not, in the PrepareStopOffer() method of the
    // Skeleton, based on whether there are still active proxies connected to the skeleton instance or not.
    constexpr bool take_ownership{false};
    return memory::shared::LockFile::CreateOrOpen(service_instance_usage_marker_file_path, take_ownership);
}

template <typename T>
T& DereferenceWithNullCheck(std::unique_ptr<T>& ptr)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(ptr != nullptr, "Cannot dereference pointer as it is Null");
    return *ptr;
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'service_instance_existence_marker_file.value()'
// in case the 'service_instance_existence_marker_file' doesn't have value but as we check before with 'has_value()'
// so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<Skeleton> Skeleton::Create(const InstanceIdentifier& identifier,
                                           score::filesystem::Filesystem filesystem,
                                           std::unique_ptr<IShmPathBuilder> shm_path_builder,
                                           std::unique_ptr<IPartialRestartPathBuilder> partial_restart_path_builder)
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
    // Since we were able to flock the existence marker file, it means that either we created it or the skeleton
    // that created it previously crashed. Either way, we take ownership of the LockFile so that it's destroyed when
    // this Skeleton is destroyed.
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
      quality_type_{InstanceIdentifierView{identifier_}.GetServiceInstanceDeployment().asilLevel_},
      lola_instance_id_{lola_service_instance_deployment.instance_id_.value().GetId()},
      lola_service_id_{lola_service_type_deployment.service_id_},
      shm_path_builder_{std::move(shm_path_builder)},
      memory_manager_{GetInstanceQualityType(),
                      DereferenceWithNullCheck(shm_path_builder_),
                      lola_service_instance_deployment,
                      lola_service_type_deployment,
                      lola_instance_id_,
                      lola_service_id_},
      partial_restart_path_builder_{std::move(partial_restart_path_builder)},
      service_instance_existence_marker_file_{std::move(service_instance_existence_marker_file)},
      service_instance_usage_marker_file_{},
      service_instance_existence_flock_mutex_and_lock_{std::move(service_instance_existence_flock_mutex_and_lock)},
      on_service_methods_subscribed_mutex_{},
      method_resources_{},
      skeleton_methods_{},
      method_subscription_registration_guard_qm_{nullptr},
      method_subscription_registration_guard_asil_b_{nullptr},
      was_old_shm_region_reopened_{false},
      filesystem_{std::move(filesystem)},
      method_call_handler_scope_{},
      on_service_method_subscribed_handler_scope_{}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(shm_path_builder_ != nullptr,
                                                      "Shared memory path builder pointer is Null");
}

auto Skeleton::PrepareOffer(SkeletonEventBindings& events,
                            SkeletonFieldBindings& fields,
                            std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback)
    -> ResultBlank
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        partial_restart_path_builder_ != nullptr,
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
        // Since the previous shared memory region is not being currently used by proxies, this can mean 2 things:
        // (1) The previous shared memory was properly created and OfferService finished (the SkeletonBinding and
        // all Skeleton service elements finished their PrepareOffer calls) and either no Proxies subscribed or they
        // have all since unsubscribed. Or, (2), the previous Skeleton crashed while setting up the shared memory.
        // Since we don't differentiate between the 2 cases and because it's unused anyway, we simply remove the old
        // memory region and re-create it.
        memory_manager_.RemoveStaleSharedMemoryArtefacts();

        const auto create_result =
            memory_manager_.CreateSharedMemory(events, fields, std::move(register_shm_object_trace_callback));
        if (!(create_result.has_value()))
        {
            score::mw::log::LogError("lola") << "Could not create shared memory region for Skeleton.";
            return create_result;
        }
    }
    else
    {
        score::mw::log::LogDebug("lola") << "Reusing SHM of Skeleton (S:" << lola_service_id_
                                         << "I:" << lola_instance_id_ << ")";
        // Since the previous shared memory region is being currently used by proxies, it must have been properly
        // created and OfferService finished. Therefore, we can simply re-open it and cleanup any previous
        // in-writing transactions by the previous skeleton.
        const auto open_result =
            memory_manager_.OpenExistingSharedMemory(std::move(register_shm_object_trace_callback));
        if (!open_result.has_value())
        {
            score::mw::log::LogError("lola") << "Could not open existing shared memory region for Skeleton.";
            return open_result;
        }
        memory_manager_.CleanupSharedMemoryAfterCrash();
    }

    // If there are no registered SkeletonMethods, then we don't need to register a method subscribed handler and
    // can therefore exit early.
    if (skeleton_methods_.empty())
    {
        return {};
    }

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    auto& lola_message_passing = lola_runtime.GetLolaMessaging();
    const SkeletonInstanceIdentifier skeleton_instance_identifier{lola_service_id_, lola_instance_id_};

    // Register a handler with message passing which will open methods shared memory regions when the proxy notifies
    // via message passing that it has finished setting up the regions. We always register a handler for QM proxies
    // and also register a handler for ASIL-B proxies if this skeleton is ASIL-B.
    auto allowed_consumers_qm = GetAllowedConsumers(QualityType::kASIL_QM);
    auto qm_registration_result = lola_message_passing.RegisterOnServiceMethodSubscribedHandler(
        QualityType::kASIL_QM,
        skeleton_instance_identifier,
        IMessagePassingService::ServiceMethodSubscribedHandler{
            on_service_method_subscribed_handler_scope_,
            [this](const ProxyInstanceIdentifier proxy_instance_identifier,
                   const uid_t proxy_uid,
                   const pid_t proxy_pid) -> ResultBlank {
                return OnServiceMethodsSubscribed(
                    proxy_instance_identifier, proxy_uid, QualityType::kASIL_QM, proxy_pid);
            }},
        allowed_consumers_qm);
    if (!(qm_registration_result.has_value()))
    {
        score::mw::log::LogError("lola") << "Could not register QM service method handler. Returning error.";
        return MakeUnexpected<Blank>(qm_registration_result.error());
    }
    method_subscription_registration_guard_qm_ = std::move(qm_registration_result).value();

    if (quality_type_ == QualityType::kASIL_B)
    {
        auto allowed_consumers_asil_b = GetAllowedConsumers(QualityType::kASIL_B);
        auto asil_b_registration_result = lola_message_passing.RegisterOnServiceMethodSubscribedHandler(
            QualityType::kASIL_B,
            skeleton_instance_identifier,
            IMessagePassingService::ServiceMethodSubscribedHandler{
                on_service_method_subscribed_handler_scope_,
                [this](const ProxyInstanceIdentifier proxy_instance_identifier,
                       const uid_t proxy_uid,
                       const pid_t proxy_pid) -> ResultBlank {
                    return OnServiceMethodsSubscribed(
                        proxy_instance_identifier, proxy_uid, QualityType::kASIL_B, proxy_pid);
                }},
            allowed_consumers_asil_b);
        if (!(asil_b_registration_result))
        {
            method_subscription_registration_guard_qm_.reset();
            score::mw::log::LogError("lola") << "Could not register ASIL-B service method handler. Returning error.";
            return MakeUnexpected<Blank>(asil_b_registration_result.error());
        }
        method_subscription_registration_guard_asil_b_ = std::move(asil_b_registration_result).value();
    }

    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". This is a false positive, all results which are accessed with '.value()' that could
// implicitly call 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()',
// so no way for throwing std::bad_optional_access which leds to std::terminate(). This suppression should be
// removed after fixing [Ticket-173043](broken_link_j/Ticket-173043) coverity[autosar_cpp14_a15_5_3_violation :
// FALSE]
auto Skeleton::PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_callback) noexcept
    -> void
{
    if (unregister_shm_object_callback.has_value())
    {
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // , (true) or (<true condition>), then it shall not exit with an exception"
        // we can't add  to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        unregister_shm_object_callback.value()(
            tracing::TracingRuntime::kDummyElementNameForShmRegisterCallback,
            // Suppress "AUTOSAR C++14 A4-5-1" rule findings. This rule states: "Expressions with type enum or enum
            // class shall not be used as operands to built-in and overloaded operators other than the subscript
            // operator [ ], the assignment operator =, the equality operators == and ! =, the unary & operator, and
            // the relational operators <,
            // <=, >, >=.". The enum is not an operand, it is a function parameter.
            // coverity[autosar_cpp14_a4_5_1_violation : FALSE]
            tracing::TracingRuntime::kDummyElementTypeForShmRegisterCallback);
    }

    // Unregister any MethodCallHandlers that were registered by the SkeletonMethods and destroy registration guards
    // which will destroy any registered ServiceMethodSubscribedHandlers. Expiring the scopes below will try to
    // acquire a write lock on a mutex, while any calls to handlers will try to acquire a read lock. Therefore, if
    // handlers can still be called while calling Expire(), then it's possible that the Expire() call will be
    // blocked for a longer period of time (in case we get new handler calls) depending on thread/mutex scheduling.
    // Therefore, we first unregister all handlers and then expire the scopes.
    method_subscription_registration_guard_qm_.reset();
    method_subscription_registration_guard_asil_b_.reset();
    for (auto& skeleton_method : skeleton_methods_)
    {
        skeleton_method.second.get().UnregisterMethodCallHandlers();
    }

    // Clean up method resources
    // Expiring the method subscribed handler scope will wait until any current subscription calls are finished and
    // will block any new subscription calls once it returns.
    on_service_method_subscribed_handler_scope_.Expire();

    // Expiring the method call handler scope will wait until all current method calls are finished and will block
    // any new handlers from being called once it returns.
    method_call_handler_scope_.Expire();

    // Destroy our pointers to all opened shared memory regions. We can call this without locking a mutex since
    // OnServiceMethodsSubscribed (in which method_resources_ is also modified) cannot be called after its scope
    // (on_service_method_subscribed_handler_scope_) is expired above.
    method_resources_.Clear();

    memory::shared::ExclusiveFlockMutex service_instance_usage_mutex{*service_instance_usage_marker_file_};
    std::unique_lock<memory::shared::ExclusiveFlockMutex> service_instance_usage_lock{service_instance_usage_mutex,
                                                                                      std::defer_lock};
    if (!service_instance_usage_lock.try_lock())
    {
        score::mw::log::LogInfo("lola")
            << "Skeleton::RemoveSharedMemory(): Could not exclusively lock service instance usage "
               "marker file indicating that some proxies are still subscribed. Will not remove shared memory and "
               "will "
               "keep the service-instance-usage-marker-file for future skeletons to reuse.";
        return;
    }
    else
    {
        // Since we were able to exclusively lock the usage marker file, it means that no proxies are currently
        // using the shared memory region.
        memory_manager_.RemoveSharedMemory();
        // We take ownership of the usage marker file so that it gets unlinked from the fs, when we destroy it.
        service_instance_usage_marker_file_.value().TakeOwnership();
        // Unlock the usage marker file before destroying it so that the destructor doesn't try to unlock an already
        // invalid file/fd.
        service_instance_usage_lock.unlock();
        // this effectively deletes the usage marker file from filesystem
        service_instance_usage_marker_file_.reset();
    }

    memory_manager_.Reset();
}

QualityType Skeleton::GetInstanceQualityType() const
{
    return quality_type_;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". This is a false positive, there is no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void Skeleton::CleanupSharedMemoryAfterCrash()
{
    memory_manager_.CleanupSharedMemoryAfterCrash();
}

void Skeleton::DisconnectQmConsumers()
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

void Skeleton::RegisterMethod(const LolaMethodId method_id, SkeletonMethod& skeleton_method)
{
    const auto [ignorable, was_inserted] = skeleton_methods_.insert({method_id, skeleton_method});
    score::cpp::ignore = ignorable;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(was_inserted, "Method IDs must be unique!");
}

bool Skeleton::VerifyAllMethodsRegistered() const
{
    for (const auto& [ignorable, method_reference] : skeleton_methods_)
    {
        score::cpp::ignore = ignorable;
        if (!method_reference.get().IsRegistered())
        {
            return false;
        }
    }
    return true;
}

std::pair<void*, EventDataControlComposite<>> Skeleton::RegisterGeneric(
    const ElementFqId element_fq_id,
    const SkeletonEventProperties& element_properties,
    const size_t sample_size,
    const size_t sample_alignment) noexcept
{
    if (was_old_shm_region_reopened_)
    {
        auto [data_storage, control_composite] =
            memory_manager_.OpenEventDataFromOpenedSharedMemory<std::uint8_t>(element_fq_id);

        auto& event_data_control_qm = control_composite.GetQmEventDataControlLocal();
        auto rollback_result = event_data_control_qm.GetTransactionLogSet().RollbackSkeletonTracingTransactions(
            [&event_data_control_qm](const TransactionLog::SlotIndexType slot_index) {
                event_data_control_qm.DereferenceEventWithoutTransactionLogging(slot_index);
            });
        if (!rollback_result.has_value())
        {
            ::score::mw::log::LogWarn("lola")
                << "SkeletonEvent: PrepareOffer failed: Could not rollback tracing consumer after "
                   "crash. Disabling tracing.";
            impl::Runtime::getInstance().GetTracingRuntime()->DisableTracing();
        }

        return {data_storage, control_composite};
    }

    return memory_manager_.CreateEventDataFromOpenedSharedMemory(
        element_fq_id, element_properties, sample_size, sample_alignment);
}

ResultBlank Skeleton::OnServiceMethodsSubscribed(const ProxyInstanceIdentifier& proxy_instance_identifier,
                                                 const uid_t proxy_uid,
                                                 const QualityType asil_level,
                                                 const pid_t proxy_pid)
{
    // Note. we currently call the entirety of the funcitonality within OnServiceMethodsSubscribed within the mutex.
    // We potentially could optimise this and call some functionality outside of the mutex. However, we haven't
    // currenlty analysed all of the potential race conditions that could occur when all of these actions are not
    // synchronised. So, for the moment, we leave all actions within the mutex and will optimise in future if we
    // identify that this is a performance bottleneck.
    std::lock_guard lock{on_service_methods_subscribed_mutex_};
    if (method_resources_.Contains(proxy_instance_identifier, proxy_pid))
    {
        score::mw::log::LogDebug("lola") << "Method" << proxy_instance_identifier.application_id << "/"
                                         << proxy_instance_identifier.proxy_instance_counter << "with PID:" << proxy_pid
                                         << "already subscribed. Not re-opening shared memory region";
        return {};
    }

    const auto method_channel_shm_name =
        shm_path_builder_->GetMethodChannelShmName(lola_instance_id_, proxy_instance_identifier);
    const bool is_read_write{true};

    method_resources_.CleanUpOldRegions(proxy_instance_identifier, proxy_pid);  // Per Proxy

    const std::vector<uid_t> allowed_providers{proxy_uid};
    auto opened_shm_region =
        memory::shared::SharedMemoryFactory::Open(method_channel_shm_name, is_read_write, allowed_providers);
    if (opened_shm_region == nullptr)
    {
        score::mw::log::LogError("lola") << "Failed to open methods shared memory region at" << method_channel_shm_name;
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    const auto resource_it = method_resources_.Insert(proxy_instance_identifier, proxy_pid, opened_shm_region);

    auto& method_data = GetMethodData(*(resource_it->second));

    const auto [subscription_result, method_ids_to_unsubscribe] =
        SubscribeMethods(method_data, proxy_instance_identifier, proxy_uid, proxy_pid, asil_level);
    if (!(subscription_result.has_value()))
    {
        UnsubscribeMethods(method_ids_to_unsubscribe, proxy_instance_identifier);
        return subscription_result;
    }
    return {};
}

auto Skeleton::SubscribeMethods(const MethodData& method_data,
                                const ProxyInstanceIdentifier proxy_instance_identifier,
                                const uid_t proxy_uid,
                                const pid_t proxy_pid,
                                const QualityType asil_level) -> std::pair<score::ResultBlank, MethodIdsToUnsubscribe>
{
    const auto& method_call_queues = method_data.method_call_queues_;
    for (std::size_t method_idx = 0U; method_idx != method_call_queues.size(); method_idx++)
    {
        auto& [method_id, type_erased_call_queue] = method_call_queues[method_idx];

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            skeleton_methods_.count(method_id) != 0U,
            "Each method that was stored in shared memory by the proxy must be registered with the Skeleton!");
        auto& skeleton_method = skeleton_methods_.at(method_id);
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier{proxy_instance_identifier, method_id};

        const auto result =
            skeleton_method.get().OnProxyMethodSubscribeFinished(type_erased_call_queue.GetTypeErasedElementInfo(),
                                                                 type_erased_call_queue.GetInArgValuesQueueStorage(),
                                                                 type_erased_call_queue.GetReturnValueQueueStorage(),
                                                                 proxy_method_instance_identifier,
                                                                 method_call_handler_scope_,
                                                                 proxy_uid,
                                                                 proxy_pid,
                                                                 asil_level);
        if (!(result.has_value()))
        {
            score::mw::log::LogError("lola")
                << "Calling OnProxyMethodSubscribeFinished on SkeletonMethod: ProxyMethodInstanceIdentifier:"
                << proxy_method_instance_identifier << "] failed!";

            // If subscription failed for any of the methods, then subscription fails for the entire Proxy.
            // Therefore, we can unsubscribe the methods that were already successfully subscribed.
            std::vector<LolaMethodId> method_ids_to_unsubscribe{};
            for (std::size_t registered_method_idx = 0U; registered_method_idx < method_idx; ++registered_method_idx)
            {
                method_ids_to_unsubscribe.push_back(method_call_queues[registered_method_idx].first);
            }

            return {result, method_ids_to_unsubscribe};
        }
    }
    return {};
}

void Skeleton::UnsubscribeMethods(const std::vector<LolaMethodId>& method_ids,
                                  const ProxyInstanceIdentifier& proxy_instance_identifier)
{
    for (const auto& method_id : method_ids)
    {
        auto& skeleton_method = skeleton_methods_.at(method_id);
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier{proxy_instance_identifier, method_id};
        skeleton_method.get().OnProxyMethodUnsubscribe(proxy_method_instance_identifier);
    }
}

IMessagePassingService::AllowedConsumerUids Skeleton::GetAllowedConsumers(const QualityType asil_level) const
{
    const auto& lola_service_instance_deployment = GetLolaServiceInstanceDeployment(identifier_);
    const auto& allowed_consumers = lola_service_instance_deployment.allowed_consumer_;
    const auto strict_permissions = lola_service_instance_deployment.strict_permissions_;

    const auto allowed_consumer_list_it = allowed_consumers.find(asil_level);
    if (allowed_consumer_list_it == allowed_consumers.cend())
    {
        // If strict_permissions is false, then an empty allowed_consumers list means that anyone is an allowed
        // consumer. Otherwise, it means that noone is an allowed consumer.
        if (strict_permissions)
        {
            score::mw::log::LogDebug("lola") << "Quality type:" << ToString(asil_level)
                                             << "does not exist in allowed_consumer list in configuration!";
            return std::set<uid_t>{};
        }
        else
        {
            return std::optional<std::set<uid_t>>{};
        }
    }

    // Check if the proxy_uid is in the allowed consumer list for the specified quality
    const auto& allowed_consumer_vector = allowed_consumer_list_it->second;
    return std::set<uid_t>{allowed_consumer_vector.begin(), allowed_consumer_vector.end()};
}

MethodData& Skeleton::GetMethodData(const memory::shared::ManagedMemoryResource& resource)
{
    auto* const method_data_storage = static_cast<MethodData*>(resource.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_data_storage != nullptr,
                                                "Could not retrieve method data within shared-memory.");
    return *method_data_storage;
}

}  // namespace score::mw::com::impl::lola
