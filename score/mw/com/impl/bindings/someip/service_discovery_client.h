/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H

#include "score/message_passing/i_client_factory.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_service_discovery_client.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/someip/sd_client/sd_client.h"

#include "score/result/result.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief Service discovery client of the SOME/IP binding.
///
/// \details Offers are sent to the configured SD daemon. Event offering remains unsupported until a SOME/IP transport
///          is integrated.
///          The client/runtime must outlive its callbacks. Callbacks may stop searches but must not destroy the
///          discovery client or runtime.
///          All FindServiceHandler invocations run on one dedicated dispatcher thread, in the order in which the
///          notifications were produced. IPC callbacks only update state and enqueue notifications. After
///          StopFindService() returns, the handler of the stopped handle is neither running (unless StopFindService()
///          is called from that very handler) nor called again.
///          StopFindService() blocks until a concurrently running handler of the same handle has returned. Handlers
///          must therefore not block on resources held by, or wait for, a thread that may call StopFindService() for
///          that handle.
///          Pending notifications are coalesced: each search has at most one queued notification, which carries the
///          latest availability when it is dispatched and is dropped if that equals the last delivered availability.
///          Session loss is reported once via LogError and an empty notification to searches previously reported as
///          available. Neither offers nor searches are replayed; the user has to re-issue them.
///
/// \attention This implementation is QM and does not enforce an identifier's quality. Accepting a non-QM identifier,
///            including an ASIL-B-tagged identifier, does not provide ASIL-B safety guarantees.
///            Consumer searches support configured instance IDs only. Synchronous FindService remains unsupported
///            because the control protocol has no availability snapshot. Concurrent version variants for one
///            service/instance are also unsupported because availability notifications carry no version.
class ServiceDiscoveryClient final : public IServiceDiscoveryClient
{
  public:
    explicit ServiceDiscoveryClient(score::message_passing::IClientFactory& client_factory);
    ~ServiceDiscoveryClient() noexcept override;

    ServiceDiscoveryClient(const ServiceDiscoveryClient&) = delete;
    ServiceDiscoveryClient(ServiceDiscoveryClient&&) noexcept = delete;
    ServiceDiscoveryClient& operator=(const ServiceDiscoveryClient&) & = delete;
    ServiceDiscoveryClient& operator=(ServiceDiscoveryClient&&) & noexcept = delete;

    Result<void> OfferService(const InstanceIdentifier instance_identifier) override;
    Result<void> StopOfferService(const InstanceIdentifier instance_identifier,
                                  const IServiceDiscovery::QualityTypeSelector quality_type_selector) override;
    Result<void> StartFindService(const FindServiceHandle find_service_handle,
                                  FindServiceHandler<HandleType> handler,
                                  const EnrichedInstanceIdentifier enriched_instance_identifier) override;
    Result<void> StopFindService(const FindServiceHandle find_service_handle) override;
    Result<ServiceHandleContainer<HandleType>> FindService(
        const EnrichedInstanceIdentifier enriched_instance_identifier) override;

  private:
    struct ServiceKey
    {
        std::uint16_t service;
        std::uint16_t instance;

        friend bool operator==(const ServiceKey& lhs, const ServiceKey& rhs) noexcept
        {
            return (lhs.service == rhs.service) && (lhs.instance == rhs.instance);
        }
    };

    struct ServiceKeyHash
    {
        std::size_t operator()(const ServiceKey& key) const noexcept
        {
            return (static_cast<std::size_t>(key.service) << 16U) | static_cast<std::size_t>(key.instance);
        }
    };

    struct ServiceRequest
    {
        std::uint8_t major;
        std::uint32_t minor;
        std::size_t users;
        bool available;
        bool request_sent;
    };

    struct SearchRequest
    {
        FindServiceHandle handle;
        InstanceIdentifier instance_identifier;
        ServiceKey key;
        FindServiceHandler<HandleType> handler;
        /// Session generation at registration time; a search never survives a session loss.
        std::uint64_t generation;
        /// Set once StartFindService() succeeded; inactive searches receive no notifications.
        bool active;
        /// Availability most recently handed to the handler by the dispatcher; empty until the first callback.
        std::optional<bool> last_delivered_availability;
        /// Latest availability to deliver; only meaningful while queued is set.
        bool pending_availability;
        /// Set while this search has its (single) entry in dispatch_queue_.
        bool queued;
    };

    using SearchRequests = std::vector<std::shared_ptr<SearchRequest>>;

    // Call with worker_mutex_ held.
    bool HasSearchRequest(FindServiceHandle handle, const InstanceIdentifier& instance_identifier) const;
    bool IsCurrentSearchRequest(const std::shared_ptr<SearchRequest>& search_request) const;
    Result<void> RegisterSearchRequest(const std::shared_ptr<SearchRequest>& search_request,
                                       const score::someip_sd::ServiceDetails& details);
    void RemoveSearchRequest(const std::shared_ptr<SearchRequest>& search_request);
    void EnqueuePendingNotifications();
    void EnqueueNotification(const std::shared_ptr<SearchRequest>& search_request, bool available);
    /// Moves the purged entries into removed so that they are destroyed after worker_mutex_ is released.
    void PurgeQueuedNotifications(FindServiceHandle handle, SearchRequests& removed);

    Result<void> SendServiceRequest(const std::shared_ptr<SearchRequest>& search_request,
                                    const score::someip_sd::ServiceDetails& details);
    static void NotifySearchRequest(const SearchRequest& search_request, bool available);
    void RunDispatcher();
    void WaitForInFlightHandler(FindServiceHandle handle);
    void OnAvailability(ServiceKey key, bool available);
    void OnConnectionLost();

    /// Serializes control operations towards the SD daemon (offer start/stop, search start/stop), so that no
    /// reconnect can happen between a search's session check and its request send. Never taken by IPC callbacks and
    /// never held while waiting for the dispatcher.
    std::mutex control_mutex_;
    /// Protects all state below. Never held while calling into SdClient or user code.
    std::mutex worker_mutex_;
    std::condition_variable dispatch_condition_;
    std::condition_variable handler_done_condition_;
    SearchRequests search_requests_;
    std::unordered_map<ServiceKey, ServiceRequest, ServiceKeyHash> service_requests_;
    /// FIFO of searches with a pending notification; each search appears at most once (see SearchRequest::queued).
    std::deque<std::shared_ptr<SearchRequest>> dispatch_queue_;
    std::optional<FindServiceHandle> in_flight_handle_;
    std::uint64_t session_generation_{0U};
    bool dispatcher_stop_requested_{false};
    std::thread dispatcher_thread_;
    /// Declared last so connection callbacks stop before their captured state is destroyed.
    score::someip_sd::SdClient sd_client_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERVICE_DISCOVERY_CLIENT_H
