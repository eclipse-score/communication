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
#include "score/mw/com/impl/bindings/lola/proxy.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/methods/method_data.h"
#include "score/mw/com/impl/bindings/lola/methods/offered_state_machine.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_rollback_executor.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/language/safecpp/safe_atomics/try_atomic_add.h"
#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/flock/shared_flock_mutex.h"
#include "score/memory/shared/lock_file.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/os/acl.h"
#include "score/os/errno_logging.h"
#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/span.hpp>
#include <score/utility.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Checks whether we run over QNX or linux to choose the appropriate path.
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNXNTO__
// Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
// constant-initialized.".
// score::filesystem::Path is not literal class as it has std::vector data member.
// coverity[autosar_cpp14_a3_3_2_violation]
const filesystem::Path kShmPathPrefix{"/dev/shmem"};
// coverity[autosar_cpp14_a16_0_1_violation]
#else
// coverity[autosar_cpp14_a3_3_2_violation]
const filesystem::Path kShmPathPrefix{"/dev/shm"};
// coverity[autosar_cpp14_a16_0_1_violation]
#endif

using memory::DataTypeSizeInfo;

/// \brief Tries to place a shared flock on the given service instance usage marker file
/// \details If shared flocking fails, it does some retries.
///          Reasoning: There can be some race conditions, where a skeleton process crashes, leaving its offering active
///          in the service discovery and then, when restarting, exclusively flocks the usage marker file for some time.
///          The proxy could hit this point in time, when trying to shared flock and then fail, so we do some retries.
/// \param service_instance_usage_marker_file usage marker file
/// \param file_path path of marker file (used for logging output)
/// \param max_retries maximum numberof retries for flock
/// \return nullptr in case shared flocking failed, else a unique ptr to the locked flock mutex.
std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>
PlaceSharedLockOnUsageMarkerFileWithRetry(memory::shared::LockFile& service_instance_usage_marker_file,
                                          std::string_view file_path,
                                          std::uint8_t max_retries)
{
    auto service_instance_usage_mutex_and_lock =
        std::make_unique<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>(
            service_instance_usage_marker_file);
    constexpr std::chrono::milliseconds kRetryBackoffTime{200U};
    std::uint8_t retry_counter{0U};

    // We use while true and manually break within the loop to prevent sleeping an additional time in case retry_counter
    // exceeds max_retries.
    // LCOV_EXCL_BR_LINE: See comment above
    while (true)
    {
        if (service_instance_usage_mutex_and_lock->TryLock())
        {
            return service_instance_usage_mutex_and_lock;
        }

        score::mw::log::LogWarn("lola")
            << "Flock try_lock failed: Skeleton could have already exclusively flocked the usage marker file: "
            << file_path;
        retry_counter++;
        score::mw::log::LogWarn("lola") << "Flock try_lock failed: Retry attempt (" << retry_counter << "/" << max_retries
                                      << ").";
        if (retry_counter >= max_retries)
        {
            score::mw::log::LogWarn("lola") << "Flock try_lock failed: STOP retrying";
            break;
        }
        std::this_thread::sleep_for(kRetryBackoffTime);
    }
    return nullptr;
}

const LolaServiceInstanceDeployment& GetLoLaInstanceDeployment(const score::mw::com::impl::HandleType& handle) noexcept
{
    const auto& instance_identifier = InstanceIdentifierView{handle.GetInstanceIdentifier()};
    const auto& service_instance_deployment = instance_identifier.GetServiceInstanceDeployment();
    return GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(service_instance_deployment);
}

const LolaServiceTypeDeployment& GetLoLaServiceTypeDeployment(const score::mw::com::impl::HandleType& handle) noexcept
{
    const auto& instance_identifier = InstanceIdentifierView{handle.GetInstanceIdentifier()};
    const auto& service_type_deployment = instance_identifier.GetServiceTypeDeployment();
    return GetServiceTypeDeploymentBinding<LolaServiceTypeDeployment>(service_type_deployment);
}

std::pair<std::shared_ptr<memory::shared::ManagedMemoryResource>,
          std::shared_ptr<memory::shared::ManagedMemoryResource>>
OpenSharedMemory(const LolaServiceInstanceDeployment& instance_deployment,
                 QualityType quality_type,
                 const LolaServiceTypeDeployment& lola_service_deployment,
                 const LolaServiceInstanceId& lola_service_instance_id) noexcept
{
    std::optional<score::cpp::span<const uid_t>> providers{std::nullopt};
    const auto found = instance_deployment.allowed_provider_.find(quality_type);
    if (found != instance_deployment.allowed_provider_.end())
    {
        providers = std::make_optional(score::cpp::span<const uid_t>{found->second});
    }

    ShmPathBuilder shm_path_builder{lola_service_deployment.service_id_};
    const auto control_shm = shm_path_builder.GetControlChannelShmName(lola_service_instance_id.GetId(), quality_type);
    const auto data_shm = shm_path_builder.GetDataChannelShmName(lola_service_instance_id.GetId());

    const std::shared_ptr<memory::shared::ManagedMemoryResource> control =
        score::memory::shared::SharedMemoryFactory::Open(control_shm, true, providers);
    const std::shared_ptr<memory::shared::ManagedMemoryResource> data =
        score::memory::shared::SharedMemoryFactory::Open(data_shm, false, providers);
    if ((control == nullptr) || (data == nullptr))
    {
        score::mw::log::LogError("lola") << "Could not create Proxy: Opening shared memory failed.";
        return std::make_pair(nullptr, nullptr);
    }

    return std::make_pair(control, data);
}

ServiceDataControl& GetServiceDataControlProxySide(const memory::shared::ManagedMemoryResource& control) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule. The rule declares:
    // An object with integer type or pointer to void type shall not be converted to an object with pointer type.
    // The "ServiceDataStorage" type is strongly defined as shared IPC data between Proxy and Skeleton.
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto* const service_data_control = static_cast<ServiceDataControl*>(control.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_data_control != nullptr, "Could not retrieve service data control.");
    return *service_data_control;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::ResultBlank ExecutePartialRestartLogic(QualityType quality_type,
                                            const memory::shared::ManagedMemoryResource& control,
                                            const memory::shared::ManagedMemoryResource& data) noexcept
{
    auto& service_data_storage = detail_proxy::GetServiceDataStorage(data);

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);

    // The transaction log is identified by the application's unique identifier, which is either the configured
    // 'applicationID' or the process UID as a fallback.
    const TransactionLogId transaction_log_id{static_cast<TransactionLogId>(lola_runtime.GetApplicationId())};
    auto& service_data_control = GetServiceDataControlProxySide(control);
    TransactionLogRollbackExecutor transaction_log_rollback_executor{
        service_data_control, quality_type, service_data_storage.skeleton_pid_, transaction_log_id};
    const auto rollback_result = transaction_log_rollback_executor.RollbackTransactionLogs();
    if (!rollback_result.has_value())
    {
        score::mw::log::LogError("lola") << "Could not create Proxy: Rolling back transaction log failed.";
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not create Proxy: Rolling back transaction log failed.");
    }

    return {};
}

}  // namespace

namespace detail_proxy
{

ServiceDataStorage& GetServiceDataStorage(const memory::shared::ManagedMemoryResource& data) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule. The rule declares:
    // An object with integer type or pointer to void type shall not be converted to an object with pointer type.
    // The "ServiceDataStorage" type is strongly defined as shared IPC data between Proxy and Skeleton.
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto* const service_data_storage = static_cast<ServiceDataStorage*>(data.getUsableBaseAddress());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_data_storage != nullptr,
                           "Could not retrieve service data storage within shared-memory.");
    return *service_data_storage;
}

}  // namespace detail_proxy

class FindServiceGuard final
{
  public:
    // Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
    // called implicitly". std::terminate() is implicitly called from 'find_service_handle_result.value()' in case
    // find_service_handle_result doesn't have a value but as we check before with 'has_value()' so no way for throwing
    // std::bad_optional_access which leds to std::terminate().
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    FindServiceGuard(FindServiceHandler<HandleType> find_service_handler,
                     EnrichedInstanceIdentifier enriched_instance_identifier)
        : service_availability_change_handle_{nullptr}
    {
        auto& service_discovery = impl::Runtime::getInstance().GetServiceDiscovery();
        const auto find_service_handle_result = service_discovery.StartFindService(
            std::move(find_service_handler), std::move(enriched_instance_identifier));
        if (!find_service_handle_result.has_value())
        {
            score::mw::log::LogFatal("lola")
                << "StartFindService failed with error" << find_service_handle_result.error() << ". Terminating.";
            std::terminate();
        }
        service_availability_change_handle_ = std::make_unique<FindServiceHandle>(find_service_handle_result.value());
    }

    ~FindServiceGuard() noexcept
    {
        auto& service_discovery = impl::Runtime::getInstance().GetServiceDiscovery();
        const auto stop_find_service_result = service_discovery.StopFindService(*service_availability_change_handle_);
        if (!stop_find_service_result.has_value())
        {
            score::mw::log::LogError("lola")
                << "StopFindService failed with error" << stop_find_service_result.error() << ". Ignoring error.";
        }
    }

    FindServiceGuard(const FindServiceGuard&) = delete;
    FindServiceGuard& operator=(const FindServiceGuard&) = delete;
    FindServiceGuard(FindServiceGuard&& other) = delete;
    FindServiceGuard& operator=(FindServiceGuard&& other) = delete;

  private:
    std::unique_ptr<FindServiceHandle> service_availability_change_handle_;
};

std::atomic<ProxyInstanceIdentifier::ProxyInstanceCounter> Proxy::current_proxy_instance_counter_{0U};

ElementFqId Proxy::EventNameToElementFqIdConverter::Convert(const std::string_view event_name) const noexcept
{
    const auto& events = events_.get();
    const auto event_it = events.find(std::string{event_name});

    std::stringstream sstream{};
    sstream << "Event name " << event_name << " does not exists in event map.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_it != events.end(), sstream.str().c_str());
    return {service_id_, event_it->second, instance_id_, ServiceElementType::EVENT};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". The std::bad_optional_access could be thrown from 'service_instance_usage_marker_file.value()',
// in case 'service_instance_usage_marker_file' doesn't have value but as we check before with 'has_value()'
// so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<Proxy> Proxy::Create(const HandleType handle) noexcept
{
    const auto& instance_deployment = GetLoLaInstanceDeployment(handle);
    const auto& lola_service_deployment = GetLoLaServiceTypeDeployment(handle);

    auto service_instance_id = handle.GetInstanceId();
    const auto lola_service_instance_id = GetServiceInstanceIdBinding<LolaServiceInstanceId>(service_instance_id);

    PartialRestartPathBuilder partial_restart_builder{lola_service_deployment.service_id_};
    const auto service_instance_usage_marker_file_path =
        partial_restart_builder.GetServiceInstanceUsageMarkerFilePath(lola_service_instance_id.GetId());

    auto service_instance_usage_marker_file = memory::shared::LockFile::Open(service_instance_usage_marker_file_path);
    if (!service_instance_usage_marker_file.has_value())
    {
        score::mw::log::LogError("lola") << "Could not open marker file: " << service_instance_usage_marker_file_path;
        return nullptr;
    }

    constexpr std::uint8_t kMaxFlockRetries{3U};
    auto service_instance_usage_mutex_and_lock =
        PlaceSharedLockOnUsageMarkerFileWithRetry(service_instance_usage_marker_file.value(),
                                                  std::string_view(service_instance_usage_marker_file_path),
                                                  kMaxFlockRetries);
    if (!service_instance_usage_mutex_and_lock)
    {
        return nullptr;
    }

    QualityType quality_type{handle.GetServiceInstanceDeployment().asilLevel_};

    const auto shared_memory =
        OpenSharedMemory(instance_deployment, quality_type, lola_service_deployment, lola_service_instance_id);

    if ((shared_memory.first == nullptr) || (shared_memory.second == nullptr))
    {
        return nullptr;
    }
    const auto& control_ref = *shared_memory.first.get();
    const auto& data_ref = *shared_memory.second.get();

    const auto partial_restart_result = ExecutePartialRestartLogic(quality_type, control_ref, data_ref);

    if (!partial_restart_result.has_value())
    {
        return nullptr;
    }

    const auto proxy_instance_counter_result =
        safe_atomics::TryAtomicAdd<ProxyInstanceIdentifier::ProxyInstanceCounter>(current_proxy_instance_counter_, 1U);
    if (!(proxy_instance_counter_result.has_value()))
    {
        score::mw::log::LogError("lola")
            << "Could not create proxy: Proxy instance counter overflowed. This can occur if more than"
            << std::numeric_limits<ProxyInstanceIdentifier::ProxyInstanceCounter>::max()
            << "proxies were created during the process lifetime. No more proxies can be created.";
        return nullptr;
    }

    EventNameToElementFqIdConverter event_name_to_element_fq_id_converter{lola_service_deployment,
                                                                          lola_service_instance_id.GetId()};
    const auto filesystem = filesystem::FilesystemFactory{}.CreateInstance();
    return std::make_unique<Proxy>(shared_memory.first,
                                   shared_memory.second,
                                   quality_type,
                                   event_name_to_element_fq_id_converter,
                                   handle,
                                   std::move(service_instance_usage_marker_file),
                                   std::move(service_instance_usage_mutex_and_lock),
                                   filesystem,
                                   proxy_instance_counter_result.value());
}

Proxy::Proxy(std::shared_ptr<memory::shared::ManagedMemoryResource> control,
             std::shared_ptr<memory::shared::ManagedMemoryResource> data,
             const QualityType quality_type,
             EventNameToElementFqIdConverter event_name_to_element_fq_id_converter,
             HandleType handle,
             std::optional<memory::shared::LockFile> service_instance_usage_marker_file,
             std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>
                 service_instance_usage_flock_mutex_and_lock,
             score::filesystem::Filesystem filesystem,
             ProxyInstanceIdentifier::ProxyInstanceCounter proxy_instance_counter) noexcept
    : ProxyBinding{},
      control_{std::move(control)},
      data_{std::move(data)},
      method_shm_resource_{nullptr},
      quality_type_{quality_type},
      event_name_to_element_fq_id_converter_{std::move(event_name_to_element_fq_id_converter)},
      handle_{std::move(handle)},
      event_bindings_{},
      proxy_event_registration_mutex_{},
      is_service_instance_available_{false},
      service_instance_usage_marker_file_{std::move(service_instance_usage_marker_file)},
      service_instance_usage_flock_mutex_and_lock_{std::move(service_instance_usage_flock_mutex_and_lock)},
      proxy_methods_{},
      method_data_{nullptr},
      proxy_instance_identifier_{GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetApplicationId(),
                                 proxy_instance_counter},
      offered_state_machine_{},
      are_proxy_methods_setup_{false},
      filesystem_{filesystem},
      find_service_guard_{std::make_unique<FindServiceGuard>(
          [this](ServiceHandleContainer<HandleType> service_handle_container, FindServiceHandle) {
              // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
              // initialization. This is a false positive, we don't use auto here
              // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
              std::lock_guard lock{proxy_event_registration_mutex_};
              is_service_instance_available_ = !service_handle_container.empty();
              ServiceAvailabilityChangeHandler(is_service_instance_available_);
          },
          EnrichedInstanceIdentifier{handle_})}
{
}

Proxy::~Proxy() = default;

void Proxy::ServiceAvailabilityChangeHandler(const bool is_service_available)
{
    for (auto& event_binding : event_bindings_)
    {
        event_binding.second.get().NotifyServiceInstanceChangedAvailability(is_service_available, GetSourcePid());
    }

    // Update the state machine to track offered/stop-offered/re-offered transitions. This must happen before the
    // early return check below to ensure the state machine correctly tracks skeleton restarts even if the proxy method
    // has not yet been setup.
    if (is_service_available)
    {
        offered_state_machine_.Offer();
    }
    else
    {
        offered_state_machine_.StopOffer();
    }

    // If the methods have not been setup in SetupMethods() then we can ignore this call. SetupMethods() is
    // guaranteed to be called on construction of a Proxy which will itself call SubscribeServiceMethod so we don't
    // need to call SubscribeServiceMethod here.
    if (!are_proxy_methods_setup_.load())
    {
        return;
    }

    // When we get a notification that the service StopOffered, then we mark the ProxyMethods as unsubscribed so that
    // any calls on them will return errors. (Note: if calls are made on them after the Skeleton was StopOffered but
    // before the ProxyMethods are unsubscribed below, then the calls will still fail due to errors returned by message
    // passing. However, by marking them ProxyMethods explicitly unsubscribed, it allows early returns which avoids
    // dispatching to message passing and allows more specific error handling / logging).
    if (offered_state_machine_.GetCurrentState() == OfferedStateMachine::State::STOP_OFFERED)
    {
        for (auto& proxy_method : proxy_methods_)
        {
            proxy_method.second.get().MarkUnsubscribed();
        }
    }
    else if (offered_state_machine_.GetCurrentState() == OfferedStateMachine::State::RE_OFFERED)
    {
        // When a skeleton restarts, it needs to re-open the methods shared memory region that was created by the
        // Proxy on construction (in SetupMethods()). To open the shared memory region, it needs the UID of this
        // Proxy (to check that it's in the allowed_consumer list in the Skeleton's configuration and to add to the
        // allowed_provider list in SharedMemoryFactory::Create()). It also needs to be notified that the Proxy has
        // subscribed to one or more of its methods (which would normally be done on Proxy creation). Therefore, we
        // resend the notification to the Skeleton that this Proxy wants to subscribe to its methods.
        auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
        auto& lola_message_passing = lola_runtime.GetLolaMessaging();
        const SkeletonInstanceIdentifier skeleton_instance_identifier{
            GetLoLaServiceTypeDeployment(handle_).service_id_,
            LolaServiceInstanceId{GetLoLaInstanceDeployment(handle_).instance_id_.value()}.GetId()};

        const auto subscribe_service_method_result = lola_message_passing.SubscribeServiceMethod(
            quality_type_, skeleton_instance_identifier, proxy_instance_identifier_, GetSourcePid());
        if (!(subscribe_service_method_result.has_value()))
        {
            // This SubscribeServiceMethod call can only be called after SetupMethods() has been called (so the methods
            // shared memory region exists) and the skeleton has stop offered and reoffered (either via a manual
            // StopOfferService or a crash restart of the process containing the Skeleton). This handler would have
            // already been called when the skeleton was stop offered which would have marked the ProxyMethods as
            // unsubscribed. Therefore, if this call fails, we simply log an error and leave the ProxyMethods
            // unsubscribed and SubscribeServiceMethod can be retried if the Skeleton restarts again.
            score::mw::log::LogError("lola")
                << __func__ << __LINE__ << "ServiceAvailabilityChangeHandler: SubscribeServiceMethod failed with error:"
                << subscribe_service_method_result.error() << "";
        }
        else
        {
            for (auto& proxy_method : proxy_methods_)
            {
                proxy_method.second.get().MarkSubscribed();
            }
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the key
// value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw an
// exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
EventControl& Proxy::GetEventControl(const ElementFqId element_fq_id) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(control_ != nullptr, "Proxy::GetEventControl: Managed memory control pointer is Null");
    auto& service_data_control = GetServiceDataControlProxySide(*control_);
    const auto event_entry = service_data_control.event_controls_.find(element_fq_id);
    if (event_entry == service_data_control.event_controls_.end())
    {
        score::mw::log::LogFatal("lola") << __func__ << __LINE__
                                       << "Unable to find control channel for given event instance. Terminating.";
        std::terminate();
    }
    // Suppress "AUTOSAR C++14 M7-5-1" rule: "A function shall not return a reference or a pointer to an automatic
    // variable (including parameters), defined within the function".
    // Suppress "AUTOSAR C++14 M7-5-2" rule: "The address of an object with automatic storage shall not be assigned
    // to another object that may persist after the first object has ceased to exist.".
    // The result pointer is still valid outside this method until "control" parameter is alive.
    // Suppress AUTOSAR C++14 A3-8-1 rule: "An object shall not be accessed outside of its lifetime.".
    // Despite the returned object reference, the object's lifetime is still handled externally by the caller.
    // coverity[autosar_cpp14_m7_5_1_violation]
    // coverity[autosar_cpp14_m7_5_2_violation]
    // coverity[autosar_cpp14_a3_8_1_violation]
    return event_entry->second;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the key
// value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw an
// exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
const EventMetaInfo& Proxy::GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(data_ != nullptr, "Proxy::GetEventMetaInfo: Managed memory data pointer is Null");
    auto& service_data_storage = detail_proxy::GetServiceDataStorage(*data_);
    const auto event_meta_info_entry = service_data_storage.events_metainfo_.find(element_fq_id);
    if (event_meta_info_entry == service_data_storage.events_metainfo_.end())
    {
        score::mw::log::LogFatal("lola") << __func__ << __LINE__
                                       << "Unable to find meta info for given event instance. Terminating.";
        std::terminate();
    }
    // Suppress "AUTOSAR C++14 A5-3-2" rule finding. This rule declares: "Null pointers shall not be dereferenced.".
    // The "event_meta_info_entry" variable is an iterator of interprocess map returned by the "find" method.
    // A check is made that the iterator is not equal to map.end(). Therefore, the call to "event_meta_info_entry->"
    // does not return nullptr.
    // Suppress "AUTOSAR C++14 A3-8-1" rule finding: "An object shall not be accessed outside of its lifetime.".
    // Despite the returned object reference, the object's lifetime is still valid until this class is destroyed.
    // coverity[autosar_cpp14_a5_3_2_violation]
    // coverity[autosar_cpp14_a3_8_1_violation]
    return event_meta_info_entry->second;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the key
// value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw an
// exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool Proxy::IsEventProvided(const std::string_view event_name) const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(control_ != nullptr,
                                 "ExecutePartialRestartLogic: Managed memory control pointer is Null");
    auto& service_data_control = GetServiceDataControlProxySide(*control_);
    const auto element_fq_id = event_name_to_element_fq_id_converter_.Convert(event_name);
    const auto event_entry = service_data_control.event_controls_.find(element_fq_id);
    const bool event_exists = (event_entry != service_data_control.event_controls_.end());
    return event_exists;
}

void Proxy::RegisterEventBinding(const std::string_view service_element_name,
                                 ProxyEventBindingBase& proxy_event_binding) noexcept
{
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    std::lock_guard lock{proxy_event_registration_mutex_};
    const auto insert_result = event_bindings_.emplace(service_element_name, proxy_event_binding);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Failed to insert proxy event binding into event binding map.");
    proxy_event_binding.NotifyServiceInstanceChangedAvailability(is_service_instance_available_, GetSourcePid());
}

void Proxy::UnregisterEventBinding(const std::string_view service_element_name) noexcept
{
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    std::lock_guard lock{proxy_event_registration_mutex_};
    const auto number_of_elements_removed = event_bindings_.erase(service_element_name);
    if (number_of_elements_removed == 0U)
    {
        score::mw::log::LogWarn("lola") << "UnregisterEventBinding that was never registered. Ignoring.";
    }
}

score::ResultBlank Proxy::SetupMethods(const std::vector<std::string_view>& enabled_method_names)
{
    if (enabled_method_names.empty())
    {
        return {};
    }
    const auto enabled_method_data = GetMethodIdAndQueueSizeFromNames(enabled_method_names);

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    auto& lola_message_passing = lola_runtime.GetLolaMessaging();
    const SkeletonInstanceIdentifier skeleton_instance_identifier{
        GetLoLaServiceTypeDeployment(handle_).service_id_,
        LolaServiceInstanceId{GetLoLaInstanceDeployment(handle_).instance_id_.value()}.GetId()};

    const auto method_shm_path_name = GetMethodChannelShmName();

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(filesystem_.standard != nullptr);

    const auto are_in_restart_context_result = filesystem_.standard->Exists(kShmPathPrefix / method_shm_path_name);
    if (!(are_in_restart_context_result.has_value()))
    {
        score::mw::log::LogWarn("lola") << "Failed to check if method shm path already exists. Exiting.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    if (are_in_restart_context_result.value())
    {
        // If the shared memory region already exists, then we are in a restart case in which the process containing
        // a proxy crashed and restarted. Since the memory region is 1:1 between a proxy instance and a skeleton
        // instance, the old shared memory region is only being used by a single skeleton instance. We can safely
        // unlink the region since the skeleton still has it mapped in memory. We will then create a new memory
        // region and notify the skeleton instance which will then close the old region and open the new one.
        memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(method_shm_path_name);
    }

    const auto type_erased_element_infos = GetTypeErasedElementInfoForEnabledMethods(enabled_method_data);
    const auto required_shm_size = CalculateRequiredShmSize(type_erased_element_infos);

    const auto skeleton_shm_permissions = GetSkeletonShmPermissions();
    method_shm_resource_ = memory::shared::SharedMemoryFactory::Create(
        method_shm_path_name,
        [this, &enabled_method_data, &type_erased_element_infos](
            std::shared_ptr<score::memory::shared::ManagedMemoryResource> memory) {
            InitializeSharedMemoryForMethods(*memory, enabled_method_data, type_erased_element_infos);
        },
        required_shm_size,
        skeleton_shm_permissions);
    if (method_shm_resource_ == nullptr)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    // We set the are_proxy_methods_setup_ flag to true here to indicate that the methods shared memory has been
    // created, regardless of the success of the SubscribeServiceMethodCall. SubscribeServiceMethod may fail (e.g. if
    // the skeleton has crashed) but the flag should still be true in the case because the
    // ServiceAvailabilityChangeHandler will then try to resend SubscribeServiceMethod on skeleton restart. However, the
    // ProxyMethods are only marked as subscribed if SubscribeServiceMethod succeeded.
    are_proxy_methods_setup_.store(true);
    const auto subscription_result = lola_message_passing.SubscribeServiceMethod(
        quality_type_, skeleton_instance_identifier, proxy_instance_identifier_, GetSourcePid());
    if (subscription_result.has_value())
    {
        for (auto& proxy_method : proxy_methods_)
        {
            proxy_method.second.get().MarkSubscribed();
        }
    }
    return subscription_result;
}

memory::shared::SharedMemoryFactory::UserPermissions Proxy::GetSkeletonShmPermissions() const
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(data_ != nullptr, "Proxy::GetSourcePid: Managed memory data pointer is Null");
    const auto& service_data_storage = detail_proxy::GetServiceDataStorage(*data_);
    const auto skeleton_uid = service_data_storage.skeleton_uid_;

    const memory::shared::SharedMemoryFactory::UserPermissionsMap permissions_map{
        {os::Acl::Permission::kRead, {skeleton_uid}}, {os::Acl::Permission::kWrite, {skeleton_uid}}};
    return {permissions_map};
}

std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>> Proxy::GetMethodIdAndQueueSizeFromNames(
    const std::vector<std::string_view>& enabled_method_names) const
{
    std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>> method_data{};
    std::transform(enabled_method_names.cbegin(),
                   enabled_method_names.cend(),
                   std::back_inserter(method_data),
                   [this](const auto& method_name) -> std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize> {
                       const std::string method_name_string{method_name};
                       const auto& lola_service_type_deployment = GetLoLaServiceTypeDeployment(handle_);
                       const auto method_id = GetServiceElementId<ServiceElementType::METHOD>(
                           lola_service_type_deployment, method_name_string);

                       const auto& lola_service_instance_deployment = GetLoLaInstanceDeployment(handle_);
                       const auto& method_instance_deployment =
                           GetServiceElementInstanceDeployment<ServiceElementType::METHOD>(
                               lola_service_instance_deployment, method_name_string);
                       SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_instance_deployment.queue_size_.has_value(),
                                              "Method instance deployment must contain queue_size on proxy side!");
                       const auto queue_size = method_instance_deployment.queue_size_.value();

                       return {method_id, queue_size};
                   });
    return method_data;
}

std::size_t Proxy::CalculateRequiredShmSize(
    std::vector<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_infos)
{

    std::vector<DataTypeSizeInfo> data_type_infos{};

    // Size of Method data
    DataTypeSizeInfo method_data_info{sizeof(MethodData), alignof(MethodData)};
    data_type_infos.push_back(method_data_info);

    // Size of Method data elements (since MethodData contains a NonRelocatableVector, it will allocate memory for
    // the elements but not actually initialize them.
    using MethodDataElement = decltype(MethodData::method_call_queues_)::value_type;
    std::for_each(type_erased_element_infos.begin(), type_erased_element_infos.end(), [&data_type_infos](auto) {
        DataTypeSizeInfo method_data_element_info{sizeof(MethodDataElement), alignof(MethodDataElement)};
        data_type_infos.push_back(method_data_element_info);
    });

    // Size of memory allocated by elements of the NonRelocatableVector in MethodData when they're constructed.
    for (auto& type_erased_element_info : type_erased_element_infos)
    {
        if (type_erased_element_info.in_arg_type_info.has_value())
        {
            const auto& in_arg_type_info = type_erased_element_info.in_arg_type_info.value();
            const DataTypeSizeInfo in_arg_type_queue_info{in_arg_type_info.Size() * type_erased_element_info.queue_size,
                                                          in_arg_type_info.Alignment()};
            data_type_infos.push_back(in_arg_type_queue_info);
        }

        if (type_erased_element_info.return_type_info.has_value())
        {
            const auto& result_type_info = type_erased_element_info.return_type_info.value();
            const DataTypeSizeInfo result_type_queue_info{result_type_info.Size() * type_erased_element_info.queue_size,
                                                          result_type_info.Alignment()};
            data_type_infos.push_back(result_type_queue_info);
        }
    }

    return memory::shared::CalculateAlignedSizeOfSequence(data_type_infos);
}

void Proxy::InitializeSharedMemoryForMethods(
    memory::shared::ManagedMemoryResource& memory_resource,
    const std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>>& method_data,
    const std::vector<TypeErasedCallQueue::TypeErasedElementInfo>& type_erased_element_infos)
{
    const auto number_of_method_ids = type_erased_element_infos.size();
    method_data_ = memory_resource.construct<MethodData>(number_of_method_ids, memory_resource);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(method_data_ != nullptr);

    for (std::size_t i = 0; i < number_of_method_ids; ++i)
    {
        const auto method_id = method_data[i].first;
        auto& emplaced_element = method_data_->method_call_queues_.emplace_back(
            std::piecewise_construct,
            std::forward_as_tuple(method_id),
            std::forward_as_tuple(*memory_resource.getMemoryResourceProxy(), type_erased_element_infos[i]));

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            proxy_methods_.count(method_id) != 0U,
            "Defensive programming: This was already checked in GetTypeErasedElementInfoForEnabledMethods");
        auto& proxy_method = proxy_methods_.at(method_id).get();
        proxy_method.SetInArgsAndReturnStorages(emplaced_element.second.GetInArgValuesQueueStorage(),
                                                emplaced_element.second.GetReturnValueQueueStorage());
    }
}

std::vector<TypeErasedCallQueue::TypeErasedElementInfo> Proxy::GetTypeErasedElementInfoForEnabledMethods(
    const std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>>& enabled_method_data) const
{
    std::vector<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_infos{};
    for (auto [method_id, queue_size] : enabled_method_data)
    {
        score::cpp::ignore = queue_size;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_methods_.count(method_id) != 0U);
        auto& proxy_method = proxy_methods_.at(method_id).get();

        const auto type_erased_data_info = proxy_method.GetTypeErasedElementInfo();
        type_erased_element_infos.push_back(type_erased_data_info);
    }
    return type_erased_element_infos;
}

std::string Proxy::GetMethodChannelShmName() const
{
    const auto& lola_instance_deployment = GetLoLaInstanceDeployment(handle_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(lola_instance_deployment.instance_id_.has_value());
    const auto lola_instance_id = lola_instance_deployment.instance_id_.value();

    const auto& lola_service_deployment = GetLoLaServiceTypeDeployment(handle_);
    ShmPathBuilder shm_path_builder{lola_service_deployment.service_id_};
    return shm_path_builder.GetMethodChannelShmName(lola_instance_id.GetId(), proxy_instance_identifier_);
}

QualityType Proxy::GetQualityType() const noexcept
{
    return quality_type_;
}

pid_t Proxy::GetSourcePid() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(data_ != nullptr, "Proxy::GetSourcePid: Managed memory data pointer is Null");
    auto& service_data_storage = detail_proxy::GetServiceDataStorage(*data_);
    return service_data_storage.skeleton_pid_;
}

void Proxy::RegisterMethod(const ElementFqId::ElementId method_id, ProxyMethod& proxy_method) noexcept
{
    const auto [ignorable, was_inserted] = proxy_methods_.insert({method_id, proxy_method});
    score::cpp::ignore = ignorable;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(was_inserted, "Method IDs must be unique!");
}

}  // namespace score::mw::com::impl::lola
