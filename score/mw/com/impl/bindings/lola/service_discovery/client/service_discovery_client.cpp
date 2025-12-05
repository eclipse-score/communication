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
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file_crawler.h"

#include "score/filesystem/filesystem.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/expected.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <exception>
#include <mutex>
#include <utility>

namespace score::mw::com::impl::lola
{

namespace
{
using underlying_type_readmask = std::underlying_type<os::InotifyEvent::ReadMask>::type;

constexpr std::uint8_t kMaxNumberOfCrawlAndWatchRetries{3U};

auto ReadMaskSet(const os::InotifyEvent& event, const os::InotifyEvent::ReadMask mask) noexcept -> bool
{
    return static_cast<underlying_type_readmask>(event.GetMask() & mask) != 0U;
}

std::vector<HandleType> GetKnownHandles(EnrichedInstanceIdentifier enriched_instance_identifier,
                                        QualityAwareContainer<KnownInstancesContainer> known_instances) noexcept
{
    std::vector<HandleType> known_handles{};
    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the kInvalid case as we use fallthrough.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (enriched_instance_identifier.GetQualityType())
    {
        case QualityType::kASIL_B:
        {
            known_handles = known_instances.asil_b.GetKnownHandles(enriched_instance_identifier);
            break;
        }
        case QualityType::kASIL_QM:
        {
            known_handles = known_instances.asil_qm.GetKnownHandles(enriched_instance_identifier);
            break;
        }
        case QualityType::kInvalid:
            [[fallthrough]];
        // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause.
        default:
        {
            score::mw::log::LogFatal("lola") << "Quality level not set for instance identifier. Terminating.";
            std::terminate();
        }
    }

    return known_handles;
}

}  // namespace

ServiceDiscoveryClient::ServiceDiscoveryClient(concurrency::Executor& long_running_threads) noexcept
    : ServiceDiscoveryClient(long_running_threads,
                             std::make_unique<os::InotifyInstanceImpl>(),
                             os::Unistd::Default(),
                             filesystem::FilesystemFactory{}.CreateInstance())
{
}

ServiceDiscoveryClient::ServiceDiscoveryClient(concurrency::Executor& long_running_threads,
                                               std::unique_ptr<os::InotifyInstance> inotify_instance,
                                               std::unique_ptr<os::Unistd> unistd,
                                               filesystem::Filesystem filesystem) noexcept
    : IServiceDiscoveryClient{},
      offer_disambiguator_{static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())},
      i_notify_{std::move(inotify_instance)},
      unistd_{std::move(unistd)},
      filesystem_{std::move(filesystem)},
      long_running_threads_{long_running_threads},
      search_requests_{},
      watches_{},
      watched_identifiers_{},
      known_instances_{},
      worker_mutex_{},
      worker_thread_result_{},
      flag_files_{},
      obsolete_search_requests_{},
      flag_files_mutex_{}
{
    // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
    // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
    // By design, if `long_running_threads_.Submit()` ever fails, we expect program termination.
    // coverity[autosar_cpp14_a15_4_2_violation]
    worker_thread_result_ = long_running_threads_.Submit([this](const auto stop_token) noexcept {
        // Suppress "AUTOSAR C++14 M0-1-3" and "AUTOSAR C++14 M0-1-9" rule violations. The rule states
        // "A project shall not contain unused variables." and "There shall be no dead code.", respectively.
        // Tolerated, this is a stop callback.
        // score::cpp::stop_callback is an RAII class which registers a callback with stop_token on construction
        // and deregisters it on destruction. Therefore, it is used again when it goes out of scope.
        // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
        // coverity[autosar_cpp14_m0_1_3_violation : FALSE]
        score::cpp::stop_callback i_notify_close_guard{stop_token, [this]() noexcept {
                                                    i_notify_->Close();
                                                }};
        while (!stop_token.stop_requested())
        {
            // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function
            // shall have automatic storage duration".
            // This is a false positive, the returned vector here is a static compile time vector.
            // coverity[autosar_cpp14_a18_5_8_violation : FALSE]
            const auto expected_events = i_notify_->Read();
            HandleEvents(expected_events);
        }
    });
}

ServiceDiscoveryClient::~ServiceDiscoveryClient() noexcept
{
    // Shut down worker thread correctly to avoid concurrency issues during destruction
    worker_thread_result_.Abort();
    score::cpp::ignore = worker_thread_result_.Wait();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ServiceDiscoveryClient::OfferService(const InstanceIdentifier instance_identifier) noexcept -> ResultBlank
{
    const EnrichedInstanceIdentifier enriched_instance_identifier{instance_identifier};
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value(),
        "Instance identifier must have instance id for service offer");
    // A wrap-around of the disambiguator is not problematic here as unsigned integer overflow
    // is well-defined (wrap-around behavior) and still produces a valid disambiguator value.
    auto offer_disambiguator = offer_disambiguator_.fetch_add(1);

    {
        // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
        // initialization.
        // This is a false positive, we don't use auto here.
        // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
        std::lock_guard lock{flag_files_mutex_};
        if (flag_files_.find(instance_identifier) != flag_files_.cend())
        {
            return MakeUnexpected(ComErrc::kBindingFailure, "Service is already offered");
        }
    }

    QualityAwareContainer<score::cpp::optional<FlagFile>> flag_files{};
    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at each case as we use fallthrough and return.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (enriched_instance_identifier.GetQualityType())
    {
        case QualityType::kASIL_B:
        {
            EnrichedInstanceIdentifier asil_b_enriched_instance_identifier{enriched_instance_identifier,
                                                                           QualityType::kASIL_B};
            auto asil_b_flag_file =
                FlagFile::Make(asil_b_enriched_instance_identifier, offer_disambiguator, filesystem_);
            if (!asil_b_flag_file.has_value())
            {
                return score::MakeUnexpected(ComErrc::kServiceNotOffered, "Failed to create flag file for ASIL-B");
            }
            score::cpp::ignore = flag_files.asil_b.emplace(std::move(asil_b_flag_file).value());
        }
            // As a service provider if we support offering a service with ASIL_B quality level that means that
            // this is the highest quality level we support, so we also support the lower quality levels that's why
            // we fall through QM level.
            [[fallthrough]];
        case QualityType::kASIL_QM:
        {
            EnrichedInstanceIdentifier asil_qm_enriched_instance_identifier{enriched_instance_identifier,
                                                                            QualityType::kASIL_QM};
            auto asil_qm_flag_file =
                FlagFile::Make(asil_qm_enriched_instance_identifier, offer_disambiguator, filesystem_);
            if (!asil_qm_flag_file.has_value())
            {
                return score::MakeUnexpected(ComErrc::kServiceNotOffered, "Failed to create flag file for ASIL-QM");
            }
            score::cpp::ignore = flag_files.asil_qm.emplace(std::move(asil_qm_flag_file).value());
            break;
        }
        case QualityType::kInvalid:
            [[fallthrough]];
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        default:
            return score::MakeUnexpected(ComErrc::kBindingFailure, "Unknown quality type of service");
    }

    {
        // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
        // initialization.
        // This is a false positive, we don't use auto here.
        // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
        std::lock_guard lock{flag_files_mutex_};
        score::cpp::ignore = flag_files_.emplace(enriched_instance_identifier.GetInstanceIdentifier(), std::move(flag_files));
    }

    return {};
}

auto ServiceDiscoveryClient::StopOfferService(
    const InstanceIdentifier instance_identifier,
    const IServiceDiscovery::QualityTypeSelector quality_type_selector) noexcept -> ResultBlank
{
    const EnrichedInstanceIdentifier enriched_instance_identifier{instance_identifier};
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value(),
        "Instance identifier must have instance id for service offer stop");

    {
        // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
        // initialization.
        // This is a false positive, we don't use auto here.
        // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
        std::lock_guard lock{flag_files_mutex_};
        const auto flag_file_iterator = flag_files_.find(enriched_instance_identifier.GetInstanceIdentifier());
        if (flag_file_iterator == flag_files_.cend())
        {
            return score::MakeUnexpected(ComErrc::kBindingFailure, "Never offered or offer already stopped");
        }

        // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
        // a well-formed switch statement".
        // We don't need a break statement at the end of default case as we use return.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (quality_type_selector)
        {
            case IServiceDiscovery::QualityTypeSelector::kBoth:
                score::cpp::ignore = flag_files_.erase(flag_file_iterator);
                break;
            case IServiceDiscovery::QualityTypeSelector::kAsilQm:
                flag_file_iterator->second.asil_qm.reset();
                break;
                // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
            default:
                return score::MakeUnexpected(ComErrc::kBindingFailure, "Unknown quality type of service");
        }
    }

    return {};
}

auto ServiceDiscoveryClient::StopFindService(const FindServiceHandle find_service_handle) noexcept -> ResultBlank
{
    {
        // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced
        // initialization.
        // This is a false positive, we don't use auto here.
        // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
        std::lock_guard lock{worker_mutex_};
        score::cpp::ignore = obsolete_search_requests_.emplace(find_service_handle);
    }

    mw::log::LogDebug("lola") << "LoLa SD: Stopped service discovery for FindServiceHandle"
                              << FindServiceHandleView{find_service_handle}.getUid();

    return {};
}

auto ServiceDiscoveryClient::TransferSearchRequests() noexcept -> void
{
    TransferObsoleteSearchRequests();
}

auto ServiceDiscoveryClient::TransferNewSearchRequest(NewSearchRequest search_request) noexcept
    -> SearchRequestsContainer::value_type&
{
    auto& [find_service_handle,
           instance_identifier,
           watch_descriptors,
           on_service_found_callback,
           known_instances,
           previous_handles] = search_request;

    std::unordered_set<os::InotifyWatchDescriptor> watch_descriptor_placeholder{};
    watch_descriptor_placeholder.reserve(watch_descriptors.size());

    const auto added_search_request = search_requests_.emplace(find_service_handle,
                                                               SearchRequest{std::move(watch_descriptor_placeholder),
                                                                             std::move(on_service_found_callback),
                                                                             instance_identifier,
                                                                             previous_handles});
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(added_search_request.second,
                                 "The FindServiceHandle should be unique for every call to StartFindService");

    for (const auto& watch_descriptor : watch_descriptors)
    {
        const auto watch_iterator = StoreWatch(watch_descriptor.first, watch_descriptor.second);
        LinkWatchWithSearchRequest(watch_iterator, added_search_request.first);
    }

    known_instances_.asil_b.Merge(std::move(known_instances.asil_b));
    known_instances_.asil_qm.Merge(std::move(known_instances.asil_qm));

    return *(added_search_request.first);
}

auto ServiceDiscoveryClient::TransferObsoleteSearchRequests() noexcept -> void
{
    for (const auto& obsolete_search_request : obsolete_search_requests_)
    {
        TransferObsoleteSearchRequest(obsolete_search_request);
    }
    obsolete_search_requests_.clear();
}

auto ServiceDiscoveryClient::TransferObsoleteSearchRequest(const FindServiceHandle& find_service_handle) noexcept
    -> void
{
    const auto search_iterator = search_requests_.find(find_service_handle);
    if (search_iterator == search_requests_.end())
    {
        mw::log::LogWarn("lola") << "Could not find search request for:"
                                 << FindServiceHandleView{find_service_handle}.getUid();
        return;
    }

    // Intentional copy since it allows us to iterate over the watches while we modify the original set in
    // UnlinkWatchWithSearchRequest(). This could be optimised, but would make the algorithm even more complex.
    const auto watches = search_iterator->second.watch_descriptors;
    for (const auto& watch : watches)
    {
        const auto watch_to_remove_it = watches_.find(watch);
        // LCOV_EXCL_START (Defensive programming: Watches are always added and removed alongside the search request
        // under lock. Therefore, if a search request corresponding to find_service_handle was found, then a
        // corresponding watch should also be found.)
        if (watch_to_remove_it == watches_.end())
        {
            mw::log::LogError("lola") << "Could not find watch for:"
                                      << FindServiceHandleView{find_service_handle}.getUid();
            continue;
        }
        // LCOV_EXCL_STOP

        UnlinkWatchWithSearchRequest(watch_to_remove_it, search_iterator);

        auto& [watch_descriptor_to_remove, watch_to_remove] = *watch_to_remove_it;
        if (watch_to_remove.find_service_handles.empty())
        {
            known_instances_.asil_b.Remove(watch_to_remove.enriched_instance_identifier);
            known_instances_.asil_qm.Remove(watch_to_remove.enriched_instance_identifier);
            score::cpp::ignore = i_notify_->RemoveWatch(watch_descriptor_to_remove);
            EraseWatch(watch_to_remove_it);
        }
    }

    score::cpp::ignore = search_requests_.erase(find_service_handle);
}

auto ServiceDiscoveryClient::HandleEvents(
    const score::cpp::expected<score::cpp::static_vector<os::InotifyEvent, os::InotifyInstance::max_events>, os::Error>&
        expected_events) noexcept -> void
{
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here.
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    std::lock_guard lock{worker_mutex_};
    TransferSearchRequests();

    if (!(expected_events.has_value()))
    {
        if (expected_events.error() != os::Error::Code::kOperationWasInterruptedBySignal)
        {
            mw::log::LogError("lola") << "Inotify Read() failed with:" << expected_events.error().ToString();
        }
        return;
    }

    const auto& events = expected_events.value();

    std::vector<os::InotifyEvent> deletion_events{};
    std::vector<os::InotifyEvent> creation_events{};
    for (const auto& event : events)
    {

        const bool inotify_queue_overflowed = ReadMaskSet(event, os::InotifyEvent::ReadMask::kInQOverflow);
        const bool search_directory_was_removed = ReadMaskSet(event, os::InotifyEvent::ReadMask::kInIgnored);
        const bool flag_file_was_removed = ReadMaskSet(event, os::InotifyEvent::ReadMask::kInDelete);
        const bool inode_was_removed = search_directory_was_removed || flag_file_was_removed;
        const bool inode_was_created = ReadMaskSet(event, os::InotifyEvent::ReadMask::kInCreate);

        if (inotify_queue_overflowed)
        {
            mw::log::LogError("lola")
                << "Service discovery lost at least one event and is compromised now. Bailing out!";
            // Potential optimization: Resync the full service discovery with the file system and update all ongoing
            // searches with potential changes.
            std::terminate();
        }

        if (inode_was_removed)
        {
            deletion_events.push_back(event);
        }
        else if (inode_was_created)
        {
            creation_events.push_back(event);
        }
        else
        {
            const auto watch_iterator = watches_.find(event.GetWatchDescriptor());
            if (watch_iterator == watches_.end())
            {
                mw::log::LogWarn("lola") << "Received unexpected event on unknown watch"
                                         << event.GetWatchDescriptor().GetUnderlying() << "with mask"
                                         << static_cast<underlying_type_readmask>(event.GetMask());
            }
            else
            {
                const auto enriched_instance_identifier = watch_iterator->second.enriched_instance_identifier;
                // TODO: In the next line remove .data() once filesystem supports concatenation of string views
                // (Ticket-207000)
                const auto file_path =
                    GetSearchPathForIdentifier(enriched_instance_identifier) / event.GetName().data();
                mw::log::LogWarn("lola") << "Received unexpected event on" << file_path << "with mask"
                                         << static_cast<underlying_type_readmask>(event.GetMask());
            }
        }
    }

    HandleDeletionEvents(deletion_events);

    HandleCreationEvents(creation_events);
}

auto ServiceDiscoveryClient::HandleDeletionEvents(const std::vector<os::InotifyEvent>& events) noexcept -> void
{
    std::unordered_set<FindServiceHandle> impacted_searches{};
    for (const auto& event : events)
    {
        const auto watch_descriptor = event.GetWatchDescriptor();
        const auto watch_iterator = watches_.find(watch_descriptor);
        if (watch_iterator == watches_.end())
        {
            continue;
        }

        const bool file_was_deleted = ReadMaskSet(event, os::InotifyEvent::ReadMask::kInDelete);

        const auto& [enriched_instance_identifier, search_keys] = watch_iterator->second;
        if (file_was_deleted)
        {
            if (enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value())
            {
                const auto event_name = event.GetName();
                OnInstanceFlagFileRemoved(watch_iterator, event_name);
                impacted_searches.insert(search_keys.cbegin(), search_keys.cend());
            }
            else
            {
                mw::log::LogFatal("lola")
                    << "Directory" << GetSearchPathForIdentifier(enriched_instance_identifier) << "/" << event.GetName()
                    << "was deleted. Outside tampering with service discovery. Aborting!";
                std::terminate();
            }
        }
    }

    CallHandlers(impacted_searches);
}

auto ServiceDiscoveryClient::HandleCreationEvents(const std::vector<os::InotifyEvent>& events) noexcept -> void
{
    std::unordered_set<FindServiceHandle> impacted_searches{};
    for (const auto& event : events)
    {
        const auto watch_descriptor = event.GetWatchDescriptor();
        const auto watch_iterator = watches_.find(watch_descriptor);
        if (watch_iterator == watches_.end())
        {
            continue;
        }

        const auto& [enriched_instance_identifier, search_keys] = watch_iterator->second;
        const auto event_name = event.GetName();
        if (enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value())
        {
            OnInstanceFlagFileCreated(watch_iterator, event_name);
        }
        else
        {
            OnInstanceDirectoryCreated(watch_iterator, event_name);
        }

        impacted_searches.insert(search_keys.cbegin(), search_keys.cend());
    }

    CallHandlers(impacted_searches);
}

auto ServiceDiscoveryClient::CallHandlers(const std::unordered_set<FindServiceHandle>& search_keys) noexcept -> void
{
    for (const auto& search_key : search_keys)
    {
        const auto search_iterator = search_requests_.find(search_key);

        // LCOV_EXCL_BR_START (Defensive programming: Search keys can only be removed from search_requests_ by first
        // calling StopFindService and then the worker thread calling TransferSearchRequests(). CallHandlers() is always
        // called in the worker thread after getting inotify events and after calling TransferSearchRequests(). If an
        // event doesn't correspond to a watch descriptor that we're interested in, then CallHandlers will never be
        // called. Therefore, there's no way to reach this point in which we have a search key associated with a watch
        // descriptor but not within search_requests_.
        if (search_iterator == search_requests_.cend())
        {
            continue;
        }
        // LCOV_EXCL_BR_STOP

        const auto obsolete_search_iterator = obsolete_search_requests_.find(search_key);
        if (obsolete_search_iterator != obsolete_search_requests_.cend())
        {
            continue;
        }

        const auto& enriched_instance_identifier = search_iterator->second.enriched_instance_identifier;
        std::vector<HandleType> known_handles{};
        // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
        // a well-formed switch statement".
        // We don't need a break statement at the kInvalid case as we use fallthrough.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (enriched_instance_identifier.GetQualityType())
        {
            case QualityType::kASIL_B:
            {
                known_handles = known_instances_.asil_b.GetKnownHandles(enriched_instance_identifier);
                break;
            }
            case QualityType::kASIL_QM:
            {
                known_handles = known_instances_.asil_qm.GetKnownHandles(enriched_instance_identifier);
                break;
            }
                // LCOV_EXCL_START (Defensive programming: This function can only be called if a watch has been added
                // via StartFindService. All code paths in StartFindService already check the quality type, therefore,
                // it's impossible for the quality type to be invalid or unexpected here.)
            case QualityType::kInvalid:
                [[fallthrough]];
            // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause.
            default:
            {
                score::mw::log::LogFatal("lola") << "Quality level not set for instance identifier. Terminating.";
                std::terminate();
            }
                // LCOV_EXCL_STOP
        }

        std::unordered_set<HandleType> new_handles{known_handles.cbegin(), known_handles.cend()};
        auto& previous_handles = search_iterator->second.handles;

        if (previous_handles == new_handles)
        {
            continue;
        }

        mw::log::LogDebug("lola") << "LoLa SD: Starting asynchronous call to handler for FindServiceHandle"
                                  << FindServiceHandleView{search_key}.getUid() << "with" << known_handles.size()
                                  << "handles";

        previous_handles = new_handles;

        const auto& handler = search_iterator->second.find_service_handler;
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        handler(std::move(known_handles), search_key);

        mw::log::LogDebug("lola") << "LoLa SD: Asynchronous call to handler for FindServiceHandle"
                                  << FindServiceHandleView{search_key}.getUid() << "finished";
    }
}

auto ServiceDiscoveryClient::StoreWatch(const os::InotifyWatchDescriptor& watch_descriptor,
                                        const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> WatchesContainer::iterator
{
    const auto watch_result = watches_.emplace(
        watch_descriptor, Watch{enriched_instance_identifier, std::unordered_set<FindServiceHandle>{}});
    const auto identifier = LolaServiceInstanceIdentifier{enriched_instance_identifier};

    score::cpp::ignore = watched_identifiers_.emplace(
        identifier, IdentifierWatches{watch_descriptor, std::unordered_set<os::InotifyWatchDescriptor>{}});
    if (identifier.GetInstanceId().has_value())
    {
        auto any_identifier = LolaServiceInstanceIdentifier{identifier.GetServiceId()};
        auto emplace_result = watched_identifiers_.emplace(
            any_identifier,
            IdentifierWatches{std::nullopt, std::unordered_set<os::InotifyWatchDescriptor>{watch_descriptor}});
        if (!emplace_result.second)
        {
            auto& child_watches = emplace_result.first->second.child_watches;
            score::cpp::ignore = child_watches.emplace(watch_descriptor);
        }
    }

    return watch_result.first;
}

auto ServiceDiscoveryClient::EraseWatch(const WatchesContainer::iterator& watch_iterator) noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(watch_iterator->second.find_service_handles.empty(),
                                 "Watch must not be associated to any searches");
    const auto identifier = LolaServiceInstanceIdentifier(watch_iterator->second.enriched_instance_identifier);
    if (identifier.GetInstanceId().has_value())
    {
        score::cpp::ignore = watched_identifiers_.erase(identifier);
        auto any_identifier = LolaServiceInstanceIdentifier{identifier.GetServiceId()};
        auto watched_any_identifier = watched_identifiers_.find(any_identifier);

        // LCOV_EXCL_BR_START (Defensive programming: This branch will always be entered, but we add a check just in
        // case as it's not an unrecoverable error in case the watch has already been removed. When we add a watch for
        // an InstanceIdentifier with an instance ID, then we will always add a watch on the instance and service
        // directories. We check above that we're in the case in which InstanceIdentifier has an instance ID. Since
        // there's no other code path that removes watches, we will always have a watch on the service directory to
        // remove, which will be done below. EraseWatch is always called with an iterator in WatchesContainer, which
        // means that it's always called with a valid watch.
        if (watched_any_identifier != watched_identifiers_.end())
        {
            auto& child_watches = watched_any_identifier->second.child_watches;
            score::cpp::ignore = child_watches.erase(watch_iterator->first);
        }
        // LCOV_EXCL_BR_STOP
    }
    else
    {
        auto watched_identifier = watched_identifiers_.find(identifier);

        // LCOV_EXCL_BR_START (Defensive programming: This branch will always be entered, but we add a check just in
        // case as it's not an unrecoverable error in case the watch has already been removed. When we add a watch for
        // an InstanceIdentifier without an instance ID, then we will always add a watch on the service directory. We
        // check above that we're in the case in which InstanceIdentifier has no instance ID. Since there's no other
        // code path that removes watches, we will always have a watch on the service directory to remove, which will be
        // done below. EraseWatch is always called with an iterator in WatchesContainer, which means that it's always
        // called with a valid watch.
        if (watched_identifier != watched_identifiers_.end())
        {
            auto& main_watch = watched_identifier->second.watch_descriptor;
            main_watch = std::nullopt;
        }
        // LCOV_EXCL_BR_STOP
    }
    score::cpp::ignore = watches_.erase(watch_iterator);
}

auto ServiceDiscoveryClient::LinkWatchWithSearchRequest(
    const WatchesContainer::iterator& watch_iterator,
    const SearchRequestsContainer::iterator& search_iterator) noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(watch_iterator != watches_.end() && search_iterator != search_requests_.end(),
                                 "LinkWatchWithSearchRequest requires valid iterators");

    auto& search_key_set = watch_iterator->second.find_service_handles;
    score::cpp::ignore = search_key_set.insert(search_iterator->first);
    auto& watch_key_set = search_iterator->second.watch_descriptors;
    score::cpp::ignore = watch_key_set.insert(watch_iterator->first);
}

auto ServiceDiscoveryClient::UnlinkWatchWithSearchRequest(
    const WatchesContainer::iterator& watch_iterator,
    const SearchRequestsContainer::iterator& search_iterator) noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(watch_iterator != watches_.end() && search_iterator != search_requests_.end(),
                                 "UnlinkWatchWithSearchRequest requires valid iterators");

    auto& search_key_set = watch_iterator->second.find_service_handles;
    const auto number_of_search_keys_erased = search_key_set.erase(search_iterator->first);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(number_of_search_keys_erased == 1U,
                           "UnlinkWatchWithSearchRequest did not erase search key correctly");
    auto& watch_key_set = search_iterator->second.watch_descriptors;
    const auto number_of_watch_keys_erased = watch_key_set.erase(watch_iterator->first);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(number_of_watch_keys_erased == 1U,
                           "UnlinkWatchWithSearchRequest did not erase watch key correctly");
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank ServiceDiscoveryClient::StartFindService(
    const FindServiceHandle find_service_handle,
    FindServiceHandler<HandleType> handler,
    const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept
{
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    const std::lock_guard worker_lock{worker_mutex_};

    mw::log::LogDebug("lola") << "LoLa SD: Starting service discovery for"
                              << GetSearchPathForIdentifier(enriched_instance_identifier) << "with FindServiceHandle"
                              << FindServiceHandleView{find_service_handle}.getUid();

    std::vector<HandleType> known_handles{};
    std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier> watch_descriptors{};
    QualityAwareContainer<KnownInstancesContainer> known_instances{};
    // Check if the exact same search is already in progress. If it is, we can just duplicate the search request and
    // reuse cache data.
    const auto identifier = LolaServiceInstanceIdentifier(enriched_instance_identifier);
    const auto watched_identifier = watched_identifiers_.find(identifier);
    if (watched_identifier != watched_identifiers_.end() && watched_identifier->second.watch_descriptor.has_value())
    {
        known_handles = GetKnownHandles(enriched_instance_identifier, known_instances_);

        auto add_watch = [this, &watch_descriptors](const os::InotifyWatchDescriptor& watch_descriptor) {
            const auto matching_watch = watches_.find(watch_descriptor);
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(matching_watch != watches_.cend(), "Did not find matching watch");
            score::cpp::ignore =
                watch_descriptors.emplace(watch_descriptor, matching_watch->second.enriched_instance_identifier);
        };

        add_watch(watched_identifier->second.watch_descriptor.value());
        score::cpp::ignore = std::for_each(watched_identifier->second.child_watches.cbegin(),
                                    watched_identifier->second.child_watches.cend(),
                                    add_watch);
    }
    else
    {
        auto crawler_result = FlagFileCrawler{*i_notify_}.CrawlAndWatchWithRetry(enriched_instance_identifier,
                                                                                 kMaxNumberOfCrawlAndWatchRetries);
        if (!crawler_result.has_value())
        {
            return score::MakeUnexpected(ComErrc::kBindingFailure, "Failed to crawl filesystem");
        }

        auto& [found_watch_descriptors, found_known_instances] = crawler_result.value();
        known_handles = GetKnownHandles(enriched_instance_identifier, found_known_instances);
        watch_descriptors = std::move(found_watch_descriptors);
        known_instances = std::move(found_known_instances);
    }

    const auto& stored_search_request =
        TransferNewSearchRequest({find_service_handle,
                                  enriched_instance_identifier,
                                  std::move(watch_descriptors),
                                  std::move(handler),
                                  std::move(known_instances),
                                  std::unordered_set<HandleType>{known_handles.cbegin(), known_handles.cend()}});

    if (!(known_handles.empty()))
    {
        mw::log::LogDebug("lola") << "LoLa SD: Synchronously calling handler for FindServiceHandle"
                                  << FindServiceHandleView{find_service_handle}.getUid();
        const auto& stored_handler = stored_search_request.second.find_service_handler;
        stored_handler(known_handles, find_service_handle);
        mw::log::LogDebug("lola") << "LoLa SD: Synchronous call to handler for FindServiceHandle"
                                  << FindServiceHandleView{find_service_handle}.getUid() << "finished";
    }

    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto ServiceDiscoveryClient::OnInstanceDirectoryCreated(const WatchesContainer::iterator& watch_iterator,
                                                        std::string_view name) noexcept -> void
{
    const auto& [enriched_instance_identifier, search_keys] = watch_iterator->second;

    const auto expected_instance_id = FlagFileCrawler::ConvertFromStringToInstanceId(name);
    if (!(expected_instance_id.has_value()))
    {
        mw::log::LogError("lola") << "Outside tampering. Could not determine instance id from" << name << ". Skipping!";
        return;
    }
    const auto& instance_id = expected_instance_id.value();

    const EnrichedInstanceIdentifier enriched_instance_identifier_with_instance_id_from_string{
        enriched_instance_identifier.GetInstanceIdentifier(), ServiceInstanceId{instance_id}};

    auto crawler_result = FlagFileCrawler{*i_notify_}.CrawlAndWatchWithRetry(
        enriched_instance_identifier_with_instance_id_from_string, kMaxNumberOfCrawlAndWatchRetries);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(crawler_result.has_value(), "Filesystem crawling failed");

    auto& [watch_descriptors, known_instances] = crawler_result.value();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(watch_descriptors.size() == 1U, "Outside tampering. Must contain one watch descriptor.");

    auto watch = StoreWatch(watch_descriptors.begin()->first, watch_descriptors.begin()->second);
    for (const auto& search_key : search_keys)
    {
        const auto search_iterator = search_requests_.find(search_key);
        LinkWatchWithSearchRequest(watch, search_iterator);
    }

    known_instances_.asil_b.Merge(std::move(known_instances.asil_b));
    known_instances_.asil_qm.Merge(std::move(known_instances.asil_qm));
}

void ServiceDiscoveryClient::OnInstanceFlagFileCreated(const WatchesContainer::iterator& watch_iterator,
                                                       const std::string_view name) noexcept
{
    const auto& enriched_instance_identifier = watch_iterator->second.enriched_instance_identifier;

    const auto event_quality_type = FlagFileCrawler::ParseQualityTypeFromString(name);

    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the kInvalid case as we use fallthrough.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (event_quality_type)
    {
        case QualityType::kASIL_B:
        {
            score::cpp::ignore = known_instances_.asil_b.Insert(enriched_instance_identifier);
            mw::log::LogDebug("lola") << "LoLa SD: Added" << GetSearchPathForIdentifier(enriched_instance_identifier)
                                      << "(ASIL-B)";
            break;
        }
        case QualityType::kASIL_QM:
        {
            score::cpp::ignore = known_instances_.asil_qm.Insert(enriched_instance_identifier);
            mw::log::LogDebug("lola") << "LoLa SD: Added" << GetSearchPathForIdentifier(enriched_instance_identifier)
                                      << "(ASIL-QM)";
            break;
        }
            // LCOV_EXCL_START (Defensive programming: This function can only be called if a watch is found
            // corresponding to the watch descriptor of the inotify event triggered by the creation of the instance flag
            // file. The watch is created during a call to StartFindService. All code paths in StartFindService already
            // check the quality type, therefore, it's impossible for the quality type to be invalid or unexpected
            // here.)
        case QualityType::kInvalid:
            [[fallthrough]];
        default:
        {
            score::mw::log::LogError("lola")
                << "Received creation event for watch path" << GetSearchPathForIdentifier(enriched_instance_identifier)
                << "and file" << name << ", that does not follow convention. Ignoring event.";
            break;
        }
            // LCOV_EXCL_STOP
    }
}

void ServiceDiscoveryClient::OnInstanceFlagFileRemoved(const WatchesContainer::iterator& watch_iterator,
                                                       std::string_view name) noexcept
{
    const auto& enriched_instance_identifier = watch_iterator->second.enriched_instance_identifier;

    const auto event_quality_type = FlagFileCrawler::ParseQualityTypeFromString(name);
    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the kInvalid case as we use fallthrough.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (event_quality_type)
    {
        case QualityType::kASIL_B:
        {
            known_instances_.asil_b.Remove(enriched_instance_identifier);
            mw::log::LogDebug("lola") << "LoLa SD: Removed" << GetSearchPathForIdentifier(enriched_instance_identifier)
                                      << "(ASIL-B)";
            break;
        }
        case QualityType::kASIL_QM:
        {
            known_instances_.asil_qm.Remove(enriched_instance_identifier);
            mw::log::LogDebug("lola") << "LoLa SD: Removed" << GetSearchPathForIdentifier(enriched_instance_identifier)
                                      << "(ASIL-QM)";
            break;
        }
            // LCOV_EXCL_START (Defensive programming: This function can only be called if a watch is found
            // corresponding to the watch descriptor of the inotify event triggered by the deletion of the instance flag
            // file. The watch is created during a call to StartFindService. All code paths in StartFindService already
            // check the quality type, therefore, it's impossible for the quality type to be invalid or unexpected
            // here.)
        case QualityType::kInvalid:
            [[fallthrough]];
        default:
        {
            score::mw::log::LogError("lola")
                << "Received creation event for watch path" << GetSearchPathForIdentifier(enriched_instance_identifier)
                << "and file" << name << ", that does not follow convention. Ignoring event.";
            break;
        }
            // LCOV_EXCL_STOP
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<ServiceHandleContainer<HandleType>> ServiceDiscoveryClient::FindService(
    const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept
{
    // Suppress Autosar C++14 A8-5-3 states that auto variables shall not be initialized using braced initialization.
    // This is a false positive, we don't use auto here
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    std::lock_guard worker_lock{worker_mutex_};

    mw::log::LogDebug("lola") << "LoLa SD: find service for"
                              << GetSearchPathForIdentifier(enriched_instance_identifier);

    auto crawler_result = FlagFileCrawler{*i_notify_}.Crawl(enriched_instance_identifier);
    if (!crawler_result.has_value())
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "Instance identifier does not have quality type set");
    }

    auto& known_instances = crawler_result.value();
    return GetKnownHandles(enriched_instance_identifier, known_instances);
}
}  // namespace score::mw::com::impl::lola
