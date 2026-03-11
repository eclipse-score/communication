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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_CLIENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_CLIENT_H

#include "score/mw/com/impl/i_service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/known_instances_container.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/lola_service_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/quality_aware_container.h"
#include "score/mw/com/impl/find_service_handler.h"

#include "score/concurrency/executor.h"
#include "score/filesystem/filesystem.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"

#include <score/stop_token.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace score::mw::com::impl::lola
{

class ServiceDiscoveryClient final : public IServiceDiscoveryClient
{
  public:
    explicit ServiceDiscoveryClient(concurrency::Executor& long_running_threads) noexcept;
    ServiceDiscoveryClient(concurrency::Executor& long_running_threads,
                           std::unique_ptr<os::InotifyInstance> inotify_instance,
                           std::unique_ptr<os::Unistd> unistd,
                           filesystem::Filesystem filesystem) noexcept;

    ServiceDiscoveryClient(const ServiceDiscoveryClient&) noexcept = delete;
    ServiceDiscoveryClient& operator=(const ServiceDiscoveryClient&) noexcept = delete;
    ServiceDiscoveryClient(ServiceDiscoveryClient&&) noexcept = delete;
    ServiceDiscoveryClient& operator=(ServiceDiscoveryClient&&) noexcept = delete;

    ~ServiceDiscoveryClient() noexcept override;

    [[nodiscard]] ResultBlank OfferService(const InstanceIdentifier instance_identifier) noexcept override;

    [[nodiscard]] ResultBlank StopOfferService(
        const InstanceIdentifier instance_identifier,
        const IServiceDiscovery::QualityTypeSelector quality_type_selector) noexcept override;

    [[nodiscard]] ResultBlank StartFindService(
        const FindServiceHandle find_service_handle,
        FindServiceHandler<HandleType> handler,
        const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept override;

    [[nodiscard]] ResultBlank StopFindService(const FindServiceHandle find_service_handle) noexcept override;
    [[nodiscard]] Result<ServiceHandleContainer<HandleType>> FindService(
        const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept override;

  private:
    class SearchRequest
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_set<os::InotifyWatchDescriptor> watch_descriptors;
        // coverity[autosar_cpp14_m11_0_1_violation]
        FindServiceHandler<HandleType> find_service_handler;
        // coverity[autosar_cpp14_m11_0_1_violation]
        EnrichedInstanceIdentifier enriched_instance_identifier;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_set<HandleType> handles;
    };

    class Watch
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        EnrichedInstanceIdentifier enriched_instance_identifier;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_set<FindServiceHandle> find_service_handles;
    };

    class IdentifierWatches
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::optional<os::InotifyWatchDescriptor> watch_descriptor;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_set<os::InotifyWatchDescriptor> child_watches;
    };

    class NewSearchRequest
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        FindServiceHandle find_service_handle;
        // coverity[autosar_cpp14_m11_0_1_violation]
        EnrichedInstanceIdentifier instance_identifier;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier> watch_descriptors;
        // coverity[autosar_cpp14_m11_0_1_violation]
        FindServiceHandler<HandleType> on_service_found_callback;
        // coverity[autosar_cpp14_m11_0_1_violation]
        QualityAwareContainer<KnownInstancesContainer> known_instances;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unordered_set<HandleType> previous_handles;
    };

    using SearchRequestsContainer = std::unordered_map<FindServiceHandle, SearchRequest>;
    using WatchesContainer = std::unordered_map<os::InotifyWatchDescriptor, Watch>;
    using Disambiguator = std::uint64_t;

    void CallHandlers(const std::unordered_set<FindServiceHandle>& search_keys) noexcept;

    WatchesContainer::iterator StoreWatch(const os::InotifyWatchDescriptor& watch_descriptor,
                                          const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept;

    void EraseWatch(const WatchesContainer::iterator& watch_iterator) noexcept;

    void LinkWatchWithSearchRequest(const WatchesContainer::iterator& watch_iterator,
                                    const SearchRequestsContainer::iterator& search_iterator) noexcept;

    void UnlinkWatchWithSearchRequest(const WatchesContainer::iterator& watch_iterator,
                                      const SearchRequestsContainer::iterator& search_iterator) noexcept;

    void OnInstanceDirectoryCreated(const WatchesContainer::iterator& watch_iterator, std::string_view name) noexcept;

    void OnInstanceFlagFileCreated(const WatchesContainer::iterator& watch_iterator,
                                   const std::string_view name) noexcept;
    void OnInstanceFlagFileRemoved(const WatchesContainer::iterator& watch_iterator, std::string_view name) noexcept;

    void TransferSearchRequests() noexcept;
    SearchRequestsContainer::value_type& TransferNewSearchRequest(NewSearchRequest search_request) noexcept;
    void TransferObsoleteSearchRequests() noexcept;

    void TransferObsoleteSearchRequest(const FindServiceHandle& find_service_handle) noexcept;

    void HandleEvents(const score::cpp::expected<score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>,
                                          os::Error>& expected_events) noexcept;

    void HandleDeletionEvents(const std::vector<os::InotifyEvent>& events) noexcept;
    void HandleCreationEvents(const std::vector<os::InotifyEvent>& events) noexcept;

    std::atomic<Disambiguator> offer_disambiguator_;

    std::unique_ptr<os::InotifyInstance> i_notify_;
    std::unique_ptr<os::Unistd> unistd_;

    filesystem::Filesystem filesystem_;
    concurrency::Executor& long_running_threads_;

    /**
     * It is important to consider the synchronization scheme in this class. All below attributes except for mutexes and
     * the following containers are considered to be exclusively accessed by the worker thread:
     *  - new_search_requests_
     *  - obsolete_search_requests_
     *
     * While in theory it is possible to modify more attributes, this comes with the risk of ending up in concurrent
     * modifications of the same container. For this reason it is also prohibited to modify the above sets from within
     * a constructor or destructor (even if the lock is acquired).
     *
     * When modifying any of the above sets, worker_mutex_ must be locked.
     *
     * It is imperative that worker_mutex_ stays a recursive lock to not end up in deadlocks when the user calls
     * StartFindService() or StopFindService() from within the FindServiceHandler.
     *
     * The lock is retained when calling the handler, so that StopFindService is able to wait for ongoing invocations
     * to finish before returning. Since the mutex is recursive, StopFindService will return without blocking if called
     * from within the FindServiceHandler.
     */
    SearchRequestsContainer search_requests_;

    /// \brief Container which stores a information relating inotify watch descriptors to Service / instance data.
    ///
    /// This is used for identifying the relevant service instance that needs to be notified (i.e. via a handler) after
    /// the inotify mechanism has been triggered, indicating a change in the filesystem relating to that service
    /// instance.
    WatchesContainer watches_;

    /// \brief Container which stores the set of identifiers for which a watch currently exists
    ///
    /// This is used to not recrawl the filesystem if there exists already a watch that ensures an up to date cache of
    /// the service discovery state for a specific instance identifier.
    std::unordered_map<LolaServiceInstanceIdentifier, IdentifierWatches> watched_identifiers_;

    /// \brief Container which stores a map of Service Ids to instance Ids.
    ///
    /// This is used for generating the HandleTypes which are passed to the user handler. The HandleTypes are created
    /// based on the InstanceIdentifier corresponding to the FindServiceHandle provided by the user and the instance id
    /// either from the InstanceIdentifier or from the file system in the Find Any case.
    QualityAwareContainer<KnownInstancesContainer> known_instances_;

    std::recursive_mutex worker_mutex_;
    concurrency::TaskResult<void> worker_thread_result_;
    std::unordered_map<InstanceIdentifier, QualityAwareContainer<score::cpp::optional<FlagFile>>> flag_files_;
    std::unordered_set<FindServiceHandle> obsolete_search_requests_;
    std::mutex flag_files_mutex_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_CLIENT_H
