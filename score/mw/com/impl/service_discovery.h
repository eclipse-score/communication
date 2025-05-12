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
#ifndef SCORE_MW_COM_IMPL_SERVICE_DISCOVERY_H
#define SCORE_MW_COM_IMPL_SERVICE_DISCOVERY_H

#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_runtime.h"
#include "score/mw/com/impl/i_service_discovery.h"
#include "score/mw/com/impl/i_service_discovery_client.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/result/result.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl
{

class ServiceDiscovery final : public IServiceDiscovery
{
  public:
    explicit ServiceDiscovery(IRuntime&);
    ServiceDiscovery(const ServiceDiscovery&) noexcept = delete;
    ServiceDiscovery& operator=(const ServiceDiscovery&) noexcept = delete;
    ServiceDiscovery(ServiceDiscovery&&) noexcept = delete;
    ServiceDiscovery& operator=(ServiceDiscovery&&) noexcept = delete;
    ~ServiceDiscovery() override;

    [[nodiscard]] ResultBlank OfferService(InstanceIdentifier) noexcept override;
    [[nodiscard]] ResultBlank StopOfferService(InstanceIdentifier) noexcept override;
    [[nodiscard]] ResultBlank StopOfferService(InstanceIdentifier, QualityTypeSelector quality_type) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>,
                                               const InstanceSpecifier) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>, InstanceIdentifier) noexcept override;
    Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType>,
                                               const EnrichedInstanceIdentifier) noexcept override;
    [[nodiscard]] ResultBlank StopFindService(const FindServiceHandle) noexcept override;
    [[nodiscard]] Result<ServiceHandleContainer<HandleType>> FindService(
        InstanceIdentifier instance_identifier) noexcept override;
    [[nodiscard]] Result<ServiceHandleContainer<HandleType>> FindService(
        InstanceSpecifier instance_specifier) noexcept override;

  private:
    /// \brief Dispatches to BindingSpecificStartFindService and processes a binding error if returned
    ///
    /// This functionality within this function itself is threadsafe. HOWEVER, the thread safety of the binding specific
    /// StartFindService call depends on the binding itself. For a Lola binding, this function is completely thread
    /// safe.
    Result<FindServiceHandle> StartFindServiceImpl(FindServiceHandle,
                                                   std::weak_ptr<FindServiceHandler<HandleType>> handler_weak_ptr,
                                                   const EnrichedInstanceIdentifier&) noexcept;

    /// \brief Generates the next available FindServiceHandle
    ///
    /// This function is threadsafe.
    FindServiceHandle GetNextFreeFindServiceHandle() noexcept;

    /// \brief Store the user callback provided to StartFindService
    ///
    /// This function is NOT threadsafe and should be called with container_mutex_ locked.
    std::weak_ptr<FindServiceHandler<HandleType>> StoreUserCallback(const FindServiceHandle&,
                                                                    FindServiceHandler<HandleType>) noexcept;

    /// \brief Store the InstanceIdentifier corresponding to a FindServiceHandle to represent an ongoing search (with
    /// StartFindService).
    ///
    /// This function is NOT threadsafe and should be called with container_mutex_ locked.
    void StoreInstanceIdentifier(const FindServiceHandle&, const EnrichedInstanceIdentifier&) noexcept;

    IServiceDiscoveryClient& GetServiceDiscoveryClient(const InstanceIdentifier&) noexcept;

    /// \brief Call the binding specific StartFindService
    ///
    /// This functionality within this function itself is threadsafe. HOWEVER, the thread safety of the binding specific
    /// StartFindService call depends on the binding itself. For a Lola binding, this function is completely thread
    /// safe.
    ResultBlank BindingSpecificStartFindService(FindServiceHandle,
                                                std::weak_ptr<FindServiceHandler<HandleType>> handler_weak_ptr,
                                                const EnrichedInstanceIdentifier&) noexcept;

    /// \brief Removes any InstanceIdentifiers which were added to handle_to_instances_ but were never processed since
    /// StartFindService on another binding failed and we returned early.
    ///
    /// This function is thread safe as it locks container_mutex_ internally as it updates handle_to_instances_.
    void RemoveUnusedInstanceIdentifiers(
        FindServiceHandle find_service_handle,
        std::vector<EnrichedInstanceIdentifier>::const_iterator last_used_iterator,
        std::vector<EnrichedInstanceIdentifier>::const_iterator container_end) noexcept;

    IRuntime& runtime_;
    std::atomic<std::size_t> next_free_uid_;

    /// \brief Mutex to synchronise modification of user_callbacks_ and handle_to_instances_ in StartFindService and
    /// StopFindService.
    ///
    /// It has to be a recursive mutex as StartFindService / StopFindService can be called within the synchronous call
    /// to the user callback within StartFindService.
    std::recursive_mutex container_mutex_;

    /// \brief Container to store handlers that are registered with StartFindService
    ///
    /// The handlers are stored as shared_ptrs. When a handler needs to be called by the bindings, a weak_ptr to the
    /// handler is passed. This ensures that the handler will not be destroyed as long as the handler is being held by
    /// the binding (which only happens for the duration of the call to the binding).
    std::unordered_map<FindServiceHandle, std::shared_ptr<FindServiceHandler<HandleType>>> user_callbacks_;
    std::unordered_multimap<FindServiceHandle, EnrichedInstanceIdentifier> handle_to_instances_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERVICE_DISCOVERY_H
