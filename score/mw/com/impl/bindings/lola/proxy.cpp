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
#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_rollback_executor.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/runtime.h"

#include "score/filesystem/filesystem.h"
#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/flock/shared_flock_mutex.h"
#include "score/memory/shared/lock_file.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/os/errno_logging.h"
#include "score/os/fcntl.h"
#include "score/os/glob.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/span.hpp>
#include <score/utility.hpp>

#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace score::mw::com::impl::lola
{

namespace
{

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
    std::uint8_t retry_counter{0};

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

const LolaServiceInstanceDeployment* GetLoLaInstanceDeployment(const score::mw::com::impl::HandleType& handle) noexcept
{
    const auto* const instance_deployment =
        std::get_if<LolaServiceInstanceDeployment>(&handle.GetServiceInstanceDeployment().bindingInfo_);
    return instance_deployment;
}

const LolaServiceTypeDeployment* GetLoLaServiceTypeDeployment(const score::mw::com::impl::HandleType& handle) noexcept
{
    const auto* lola_service_deployment = std::get_if<LolaServiceTypeDeployment>(
        &InstanceIdentifierView{handle.GetInstanceIdentifier()}.GetServiceTypeDeployment().binding_info_);
    return lola_service_deployment;
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

    auto* const lola_runtime =
        dynamic_cast<lola::IRuntime*>(mw::com::impl::Runtime::getInstance().GetBindingRuntime(BindingType::kLoLa));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_runtime != nullptr, "No LoLa Runtime available although we are creating a LoLa proxy!");

    const TransactionLogId transaction_log_id{lola_runtime->GetUid()};
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
                     EnrichedInstanceIdentifier enriched_instance_identifier) noexcept
        : service_availability_change_handle_{nullptr}
    {
        auto& service_discovery = impl::Runtime::getInstance().GetServiceDiscovery();
        const auto find_service_handle_result = service_discovery.StartFindService(
            std::move(find_service_handler), std::move(enriched_instance_identifier));
        if (!find_service_handle_result.has_value())
        {
            score::mw::log::LogFatal("lola")
                << "StartFindService failed with error" << find_service_handle_result.error() << ". Terminating.";

            // To avoid the previous message not being logged due to the terminate, we sleep for 2 seconds.  This is a
            // temporary workaround due to a logging bug and will be removed in Ticket-184950
            std::this_thread::sleep_for(std::chrono::seconds{2U});
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

ElementFqId Proxy::EventNameToElementFqIdConverter::Convert(const score::cpp::string_view event_name) const noexcept
{
    const auto& events = events_.get();
    const auto event_it = events.find(std::string{event_name.data()});

    std::stringstream sstream{};
    sstream << "Event name " << event_name.data() << " does not exists in event map.";
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_it != events.end(), sstream.str().c_str());
    return {service_id_, event_it->second, instance_id_, ElementType::EVENT};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". The std::bad_optional_access could be thrown from 'service_instance_usage_marker_file.value()',
// in case 'service_instance_usage_marker_file' doesn't have value but as we check before with 'has_value()'
// so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<Proxy> Proxy::Create(const HandleType handle) noexcept
{
    const auto* const instance_deployment = GetLoLaInstanceDeployment(handle);
    if (instance_deployment == nullptr)
    {
        score::mw::log::LogError("lola") << "Could not create Proxy: lola service instance deployment does not exist.";
        return nullptr;
    }
    const auto& instance_deployment_ref = *instance_deployment;

    const auto* const lola_service_deployment = GetLoLaServiceTypeDeployment(handle);
    if (lola_service_deployment == nullptr)
    {
        score::mw::log::LogError("lola") << "Could not create Proxy: lola service type deployment does not exist.";
        return nullptr;
    }
    const auto& lola_service_deployment_ref = *lola_service_deployment;

    const auto service_instance_id = handle.GetInstanceId();
    const auto* const lola_service_instance_id = std::get_if<LolaServiceInstanceId>(&service_instance_id.binding_info_);
    if (lola_service_instance_id == nullptr)
    {
        score::mw::log::LogError("lola") << "Could not create Proxy: lola service instance id does not exist.";
        return nullptr;
    }
    const auto& lola_service_instance_id_ref = *lola_service_instance_id;

    PartialRestartPathBuilder partial_restart_builder{lola_service_deployment_ref.service_id_};
    const auto service_instance_usage_marker_file_path =
        partial_restart_builder.GetServiceInstanceUsageMarkerFilePath(lola_service_instance_id_ref.GetId());

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

    const auto shared_memory = OpenSharedMemory(
        instance_deployment_ref, quality_type, lola_service_deployment_ref, lola_service_instance_id_ref);

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

    EventNameToElementFqIdConverter event_name_to_element_fq_id_converter{lola_service_deployment_ref,
                                                                          lola_service_instance_id_ref.GetId()};
    return std::make_unique<Proxy>(shared_memory.first,
                                   shared_memory.second,
                                   quality_type,
                                   event_name_to_element_fq_id_converter,
                                   handle,
                                   std::move(service_instance_usage_marker_file),
                                   std::move(service_instance_usage_mutex_and_lock));
}

Proxy::Proxy(std::shared_ptr<memory::shared::ManagedMemoryResource> control,
             std::shared_ptr<memory::shared::ManagedMemoryResource> data,
             const QualityType quality_type,
             EventNameToElementFqIdConverter event_name_to_element_fq_id_converter,
             HandleType handle,
             std::optional<memory::shared::LockFile> service_instance_usage_marker_file,
             std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>
                 service_instance_usage_flock_mutex_and_lock) noexcept
    : ProxyBinding{},
      control_{std::move(control)},
      data_{std::move(data)},
      quality_type_{quality_type},
      event_name_to_element_fq_id_converter_{std::move(event_name_to_element_fq_id_converter)},
      handle_{std::move(handle)},
      event_bindings_{},
      proxy_event_registration_mutex_{},
      is_service_instance_available_{false},
      find_service_guard_{std::make_unique<FindServiceGuard>(
          [this](ServiceHandleContainer<HandleType> service_handle_container, FindServiceHandle) noexcept {
              // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
              // initialization. This is a false positive, we don't use auto here
              // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
              std::lock_guard lock{proxy_event_registration_mutex_};
              is_service_instance_available_ = !service_handle_container.empty();
              ServiceAvailabilityChangeHandler(is_service_instance_available_);
          },
          EnrichedInstanceIdentifier{handle_})},
      service_instance_usage_marker_file_{std::move(service_instance_usage_marker_file)},
      service_instance_usage_flock_mutex_and_lock_{std::move(service_instance_usage_flock_mutex_and_lock)}
{
}

Proxy::~Proxy() = default;

void Proxy::ServiceAvailabilityChangeHandler(const bool is_service_available) noexcept
{
    for (auto& event_binding : event_bindings_)
    {
        event_binding.second.get().NotifyServiceInstanceChangedAvailability(is_service_available, GetSourcePid());
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the
// key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw
// an exception.
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
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the
// key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw
// an exception.
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
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the
// key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw
// an exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool Proxy::IsEventProvided(const score::cpp::string_view event_name) const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(control_ != nullptr,
                                 "ExecutePartialRestartLogic: Managed memory control pointer is Null");
    auto& service_data_control = GetServiceDataControlProxySide(*control_);
    const auto element_fq_id = event_name_to_element_fq_id_converter_.Convert(event_name);
    const auto event_entry = service_data_control.event_controls_.find(element_fq_id);
    const bool event_exists = (event_entry != service_data_control.event_controls_.end());
    return event_exists;
}

void Proxy::RegisterEventBinding(const score::cpp::string_view service_element_name,
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

void Proxy::UnregisterEventBinding(const score::cpp::string_view service_element_name) noexcept
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

}  // namespace score::mw::com::impl::lola
