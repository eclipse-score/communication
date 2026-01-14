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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/methods/method_data.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"

#include "score/filesystem/filesystem_struct.h"
#include "score/filesystem/i_standard_filesystem.h"
#include "score/memory/shared/flock/flock_mutex_and_lock.h"
#include "score/memory/shared/flock/shared_flock_mutex.h"
#include "score/memory/shared/lock_file.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <sched.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl::lola
{

class IShmPathBuilder;
class FindServiceGuard;

namespace detail_proxy
{

ServiceDataStorage& GetServiceDataStorage(const memory::shared::ManagedMemoryResource& data) noexcept;

}

/// \brief Proxy binding implementation for all Lola proxies.
class Proxy : public ProxyBinding
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "ProxyTestAttorney" class is a helper, which sets the internal state of "Proxy" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyTestAttorney;

  public:
    /// \brief Class to convert an event name to an event fq id given the information already known to a Proxy
    ///
    /// We create a separate class to encapsulate the data that is only required for the conversion within this class.
    class EventNameToElementFqIdConverter
    {
      public:
        EventNameToElementFqIdConverter(const LolaServiceTypeDeployment& lola_service_type_deployment,
                                        LolaServiceInstanceId::InstanceId instance_id) noexcept
            : service_id_{lola_service_type_deployment.service_id_},
              events_{lola_service_type_deployment.events_},
              instance_id_{instance_id}
        {
        }

        ElementFqId Convert(const std::string_view event_name) const noexcept;

      private:
        const std::uint16_t service_id_;
        const std::reference_wrapper<const LolaServiceTypeDeployment::EventIdMapping> events_;
        LolaServiceInstanceId::InstanceId instance_id_;
    };

    Proxy(const Proxy&) noexcept = delete;
    Proxy& operator=(const Proxy&) noexcept = delete;
    Proxy(Proxy&&) noexcept = delete;
    Proxy& operator=(Proxy&&) noexcept = delete;

    // Suppress "AUTOSAR C++14 M3-2-2". This rule states: "The One Definition Rule shall not be violated."
    // "AUTOSAR C++14 M3-2-4": "An identifier with external linkage shall have exactly one definition.".
    // This is a false-positive, because this destructor has only one defition
    // coverity[autosar_cpp14_m3_2_2_violation]
    // coverity[autosar_cpp14_m3_2_4_violation]
    ~Proxy() override;

    static std::unique_ptr<Proxy> Create(const HandleType handle) noexcept;

    Proxy(std::shared_ptr<memory::shared::ManagedMemoryResource> control,
          std::shared_ptr<memory::shared::ManagedMemoryResource> data,
          const QualityType quality_type,
          EventNameToElementFqIdConverter event_name_to_element_fq_id_converter,
          HandleType handle,
          std::optional<memory::shared::LockFile> service_instance_usage_marker_file,
          std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>
              service_instance_usage_flock_mutex_and_lock,
          score::filesystem::Filesystem filesystem) noexcept;

    /// Returns the address of the control structure, for the given event ID.
    ///
    /// Terminates if the event control structure cannot be found.
    ///
    /// \param element_fq_id The Event ID.
    /// \return A reference to the event control structure.
    EventControl& GetEventControl(const ElementFqId element_fq_id) noexcept;

    /// Retrieves a reference to the event data storage area for a given ElementFqId.
    ///
    /// \param element_fq_id The Event ID.
    /// \return A reference to the EventDataStorage.
    template <typename EventSampleType>
    auto GetEventDataStorage(const ElementFqId element_fq_id) const noexcept
        -> const EventDataStorage<EventSampleType>&;

    /// Retrieves an event data meta info.
    ///
    /// The event meta info can be used to iterate over events in the event data storage when the type is not known e.g.
    /// when dealing with a GenericProxyEvent. Terminates if the event meta info cannot be found.
    ///
    /// \param element_fq_id The Event ID.
    /// \return An event data meta info.
    const EventMetaInfo& GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept;

    /// Checks whether the event corresponding to event_name is provided
    ///
    /// It does this by checking whether the event corresponding to event_name exists in shared memory.
    /// \param event_name The event name to check.
    /// \return True if the event name exists, otherwise, false
    bool IsEventProvided(const std::string_view event_name) const noexcept override;

    /// \brief Adds a reference to a Proxy service element binding to an internal map
    ///
    /// Will insert the provided ProxyEventBindingBase& into a map stored within the class which will be used to call
    /// NotifyServiceInstanceChangedAvailability on all saved Proxy service elements by the FindServiceHandler of
    /// find_service_guard_. It will then call NotifyServiceInstanceChangedAvailability on the provided
    /// ProxyEventBindingBase. Since this function first locks proxy_event_registration_mutex_, it is ensured that the
    /// provided Proxy service element will be notified synchronously about the availability of the provider and will
    /// then be notified of any future changes via the callback, without missing any notifications.
    void RegisterEventBinding(const std::string_view service_element_name,
                              ProxyEventBindingBase& proxy_event_binding) noexcept override;

    /// \brief Removes the reference to a Proxy service element binding from an internal map
    ///
    /// This must be called by a Proxy service element before destructing to ensure that the FindService handler in
    /// find_service_guard_ does not call NotifyServiceInstanceChangedAvailability on a Proxy service element after it's
    /// been destructed.
    void UnregisterEventBinding(const std::string_view service_element_name) noexcept override;

    score::ResultBlank SetupMethods(const std::vector<std::string_view>& enabled_method_names) override;

    QualityType GetQualityType() const noexcept;

    /// \brief Returns pid of provider/skeleton side, this proxy is "connected" with.
    /// \return
    pid_t GetSourcePid() const noexcept;

    ProxyInstanceIdentifier GetProxyInstanceIdentifier() const noexcept
    {
        return proxy_instance_identifier_;
    }

    void RegisterMethod(const ElementFqId::ElementId method_id, ProxyMethod& proxy_method) noexcept;

  private:
    static std::atomic<ProxyInstanceIdentifier::ProxyInstanceCounter> current_proxy_instance_counter_;

    void ServiceAvailabilityChangeHandler(const bool is_service_available) noexcept;
    void InitializeSharedMemoryForMethods(
        memory::shared::ManagedMemoryResource& memory_resource,
        const std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>>& method_data,
        const std::vector<TypeErasedCallQueue::TypeErasedElementInfo>& type_erased_element_infos);

    static bool DoElementInfosContainInArgsOrReturn(
        const std::vector<TypeErasedCallQueue::TypeErasedElementInfo>& type_erased_element_infos);
    static std::size_t CalculateRequiredShmSize(
        std::vector<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_infos);

    memory::shared::SharedMemoryFactory::UserPermissions GetSkeletonShmPermissions() const;
    std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>> GetMethodIdAndQueueSizeFromNames(
        const std::vector<std::string_view>& enabled_method_names) const;
    std::vector<TypeErasedCallQueue::TypeErasedElementInfo> GetTypeErasedElementInfoForEnabledMethods(
        const std::vector<std::pair<LolaMethodId, LolaMethodInstanceDeployment::QueueSize>>& enabled_method_data) const;
    std::string GetMethodChannelShmName() const;

    std::shared_ptr<memory::shared::ManagedMemoryResource> control_;
    std::shared_ptr<memory::shared::ManagedMemoryResource> data_;
    std::shared_ptr<memory::shared::ManagedMemoryResource> method_shm_resource_;

    QualityType quality_type_;
    EventNameToElementFqIdConverter event_name_to_element_fq_id_converter_;
    HandleType handle_;
    std::unordered_map<std::string_view, std::reference_wrapper<ProxyEventBindingBase>> event_bindings_;

    /// Mutex which synchronises registration of Proxy service elements via Proxy::RegisterEventBinding with the
    /// FindServiceHandler in find_service_guard_ which will call NotifyServiceInstanceChangedAvailability on all
    /// currently registered Proxy service elements.
    std::mutex proxy_event_registration_mutex_;
    bool is_service_instance_available_;
    std::unique_ptr<FindServiceGuard> find_service_guard_;
    std::optional<memory::shared::LockFile> service_instance_usage_marker_file_;
    std::unique_ptr<score::memory::shared::FlockMutexAndLock<score::memory::shared::SharedFlockMutex>>
        service_instance_usage_flock_mutex_and_lock_;
    std::unordered_map<LolaMethodId, std::reference_wrapper<ProxyMethod>> proxy_methods_;
    MethodData* method_data_;
    ProxyInstanceIdentifier proxy_instance_identifier_;

    score::filesystem::Filesystem filesystem_;
};

template <typename EventSampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::less which is used by std::map::find could throw an exception if the
// key value is not comparable and in our case the key is comparable. so no way for 'event_controls_.find()' to throw
// an exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto Proxy::GetEventDataStorage(const ElementFqId element_fq_id) const noexcept
    -> const EventDataStorage<EventSampleType>&
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(data_ != nullptr, "Proxy::GetEventDataStorage: Managed memory data pointer is Null");
    auto& service_data_storage = detail_proxy::GetServiceDataStorage(*data_);
    const auto event_entry = service_data_storage.events_.find(element_fq_id);
    if (event_entry == service_data_storage.events_.end())
    {
        score::mw::log::LogFatal("lola") << __func__ << __LINE__
                                       << "Unable to find data storage for given event instance. Terminating.";
        std::terminate();
    }
    // Suppress "AUTOSAR C++14 A5-3-2" rule finding. This rule declares: "Null pointers shall not be dereferenced.".
    // The "event_entry" variable is an iterator of interprocess map returned by the "find" method.
    // A check is made that the iterator is not equal to map.end(). Therefore, the call to "event_entry->"
    // does not return nullptr.
    // coverity[autosar_cpp14_a5_3_2_violation]
    const auto* event_data_storage_ptr = event_entry->second.template get<EventDataStorage<EventSampleType>>();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(event_data_storage_ptr != nullptr, "Could not get EventDataStorage from OffsetPtr");
    return *event_data_storage_ptr;
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_H
