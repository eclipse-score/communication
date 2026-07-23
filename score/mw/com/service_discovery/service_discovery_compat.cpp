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
#include "score/mw/com/service_discovery/service_discovery_compat.h"

#include <algorithm>
#include <chrono>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kClientStartTimeout = std::chrono::milliseconds{1500};

auto ContainsHandle(const std::vector<FindServiceHandle>& handles, const FindServiceHandle& handle) noexcept -> bool
{
    return std::find(handles.cbegin(), handles.cend(), handle) != handles.cend();
}

}  // namespace

ServiceDiscoveryCompat::ServiceDiscoveryCompat(IRuntime& runtime) noexcept : runtime_{runtime}, client_{} {}

ServiceDiscoveryCompat::ServiceDiscoveryCompat(
    IRuntime& runtime,
    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingClient::Config config) noexcept
    : runtime_{runtime}, client_{std::move(config)}
{
}

ServiceDiscoveryCompat::~ServiceDiscoveryCompat() noexcept
{
    std::vector<FindServiceHandle> handles{};
    {
        const std::lock_guard lock{mutex_};
        handles.reserve(searches_.size());
        for (const auto& entry : searches_)
        {
            handles.push_back(entry.first);
        }
    }

    for (const auto& handle : handles)
    {
        score::cpp::ignore = StopFindService(handle);
    }
}

auto ServiceDiscoveryCompat::ServiceKeyHash::operator()(
    const score::mw::com::service_discovery::ServiceKey& key) const noexcept -> std::size_t
{
    const auto service_hash = std::hash<std::uint64_t>{}(key.service_id);
    const auto instance_hash = std::hash<std::uint32_t>{}(key.instance_id);
    return service_hash ^ (instance_hash << 1U);
}

auto ServiceDiscoveryCompat::ToIntegrityLevel(const QualityType quality_type) noexcept
    -> score::mw::com::service_discovery::IntegrityLevel
{
    return quality_type == QualityType::kASIL_B ? score::mw::com::service_discovery::IntegrityLevel::kAsilB
                                                : score::mw::com::service_discovery::IntegrityLevel::kAsilQm;
}

auto ServiceDiscoveryCompat::EnsureClientStarted() noexcept -> bool
{
    const std::lock_guard lock{mutex_};
    if (client_started_)
    {
        return true;
    }

    client_started_ = client_.Start(kClientStartTimeout);
    return client_started_;
}

auto ServiceDiscoveryCompat::ToTrackedIdentifier(
    const EnrichedInstanceIdentifier& enriched_instance_identifier) const noexcept -> std::optional<TrackedIdentifier>
{
    const auto service_id = enriched_instance_identifier.GetBindingSpecificServiceId<LolaServiceTypeDeployment>();
    const auto instance_id = enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>();
    if (!service_id.has_value())
    {
        return std::nullopt;
    }

    TrackedIdentifier tracked_identifier{enriched_instance_identifier.GetInstanceIdentifier(), {}};
    tracked_identifier.key.service_id = static_cast<std::uint64_t>(service_id.value());
    tracked_identifier.key.instance_id = instance_id.has_value() ? static_cast<std::uint32_t>(instance_id.value())
                                                                 : score::mw::com::service_discovery::kAnyInstanceId;
    return tracked_identifier;
}

auto ServiceDiscoveryCompat::ToRegistration(const EnrichedInstanceIdentifier& enriched_instance_identifier)
    const noexcept -> std::optional<score::mw::com::service_discovery::Registration>
{
    const auto tracked_identifier = ToTrackedIdentifier(enriched_instance_identifier);
    if (!tracked_identifier.has_value())
    {
        return std::nullopt;
    }

    score::mw::com::service_discovery::Registration registration{};
    registration.key = tracked_identifier->key;
    if (score::mw::com::service_discovery::IsAnyInstanceKey(registration.key))
    {
        return std::nullopt;
    }
    registration.offered_integrity = ToIntegrityLevel(enriched_instance_identifier.GetQualityType());
    registration.provider_integrity = registration.offered_integrity;
    registration.endpoint.address = "compat";
    return registration;
}

auto ServiceDiscoveryCompat::BuildHandleContainer(const SearchEntry& search_entry) const noexcept
    -> ServiceHandleContainer<HandleType>
{
    ServiceHandleContainer<HandleType> handles{};

    for (const auto& tracked_identifier : search_entry.tracked_identifiers)
    {
        const auto snapshot_it = snapshots_.find(tracked_identifier.key);
        if (snapshot_it == snapshots_.cend())
        {
            continue;
        }

        const auto& snapshot = snapshot_it->second;
        for (std::size_t index = 0U; index < snapshot.size; ++index)
        {
            const auto& registration = snapshot.values[index];
            const auto instance_id = InstanceIdentifierView{tracked_identifier.identifier}.GetServiceInstanceId();
            if (instance_id.has_value())
            {
                handles.push_back(make_HandleType(tracked_identifier.identifier));
            }
            else
            {
                handles.push_back(make_HandleType(
                    tracked_identifier.identifier,
                    ServiceInstanceId{LolaServiceInstanceId{
                        static_cast<LolaServiceInstanceId::InstanceId>(registration.key.instance_id)}}));
            }
        }
    }

    return handles;
}

auto ServiceDiscoveryCompat::FindSearch(const FindServiceHandle& handle) noexcept -> SearchEntry*
{
    const auto iterator = searches_.find(handle);
    if (iterator == searches_.end())
    {
        return nullptr;
    }

    return &iterator->second;
}

auto ServiceDiscoveryCompat::OnNotification(
    const score::mw::com::service_discovery::ProtocolNotification& notification) noexcept -> void
{
    const std::lock_guard lock{mutex_};
    snapshots_[notification.key] = notification.response.registrations;

    const auto handles_it = key_to_handles_.find(notification.key);
    if (handles_it == key_to_handles_.end())
    {
        return;
    }

    // Copy handles: the callback may call StopFindService, which modifies key_to_handles_
    // (potentially erasing handles_it->second or key_to_handles_ itself), invalidating iterators.
    const std::vector<FindServiceHandle> handles{handles_it->second};

    for (const auto& handle : handles)
    {
        auto* search_entry = FindSearch(handle);
        if (search_entry == nullptr)
        {
            continue;
        }
        // Extend handler lifetime beyond the callback invocation: the callback may call
        // StopFindService, which erases searches_[handle] and destroys the SearchEntry,
        // dropping the shared_ptr ref count to zero and freeing the handler while executing.
        const auto handler = search_entry->handler;
        if (handler)
        {
            (*handler)(BuildHandleContainer(*search_entry), handle);
        }
    }
}

auto ServiceDiscoveryCompat::OfferService(InstanceIdentifier instance_identifier) noexcept -> Result<void>
{
    if (!EnsureClientStarted())
    {
        return MakeUnexpected(ComErrc::kCommunicationStackError);
    }

    const EnrichedInstanceIdentifier enriched_instance_identifier{std::move(instance_identifier)};
    const auto registration = ToRegistration(enriched_instance_identifier);
    if (!registration.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation);
    }

    score::mw::com::service_discovery::ProtocolRequest request{};
    request.operation = score::mw::com::service_discovery::ProtocolOperation::kRegister;
    request.registration = *registration;

    const auto response = client_.Request(request);
    if (!response.has_value() || response->status_code != 0U)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return {};
}

auto ServiceDiscoveryCompat::StopOfferService(InstanceIdentifier instance_identifier) noexcept -> Result<void>
{
    return StopOfferService(std::move(instance_identifier), QualityTypeSelector::kBoth);
}

auto ServiceDiscoveryCompat::StopOfferService(InstanceIdentifier instance_identifier,
                                              QualityTypeSelector /*quality_type*/) noexcept -> Result<void>
{
    if (!EnsureClientStarted())
    {
        return MakeUnexpected(ComErrc::kCommunicationStackError);
    }

    const EnrichedInstanceIdentifier enriched_instance_identifier{std::move(instance_identifier)};
    const auto registration = ToRegistration(enriched_instance_identifier);
    if (!registration.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation);
    }

    score::mw::com::service_discovery::ProtocolRequest request{};
    request.operation = score::mw::com::service_discovery::ProtocolOperation::kUnregister;
    request.registration = *registration;

    const auto response = client_.Request(request);
    if (!response.has_value() || response->status_code != 0U)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return {};
}

auto ServiceDiscoveryCompat::StartFindService(FindServiceHandler<HandleType> handler,
                                              const InstanceSpecifier instance_specifier) noexcept
    -> Result<FindServiceHandle>
{
    const auto instance_identifiers = runtime_.resolve(instance_specifier);
    if (instance_identifiers.empty())
    {
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }

    std::vector<TrackedIdentifier> tracked_identifiers{};
    tracked_identifiers.reserve(instance_identifiers.size());
    for (const auto& instance_identifier : instance_identifiers)
    {
        const auto tracked_identifier = ToTrackedIdentifier(EnrichedInstanceIdentifier{instance_identifier});
        if (!tracked_identifier.has_value())
        {
            return MakeUnexpected(ComErrc::kInvalidBindingInformation);
        }
        tracked_identifiers.push_back(*tracked_identifier);
    }

    const auto handle = make_FindServiceHandle(next_find_service_uid_++);
    return StartFindServiceImpl(handle, std::move(handler), tracked_identifiers);
}

auto ServiceDiscoveryCompat::StartFindService(FindServiceHandler<HandleType> handler,
                                              InstanceIdentifier instance_identifier) noexcept
    -> Result<FindServiceHandle>
{
    const auto tracked_identifier = ToTrackedIdentifier(EnrichedInstanceIdentifier{instance_identifier});
    if (!tracked_identifier.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation);
    }

    const auto handle = make_FindServiceHandle(next_find_service_uid_++);
    return StartFindServiceImpl(handle, std::move(handler), std::vector<TrackedIdentifier>{*tracked_identifier});
}

auto ServiceDiscoveryCompat::StartFindService(FindServiceHandler<HandleType> handler,
                                              const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept
    -> Result<FindServiceHandle>
{
    const auto tracked_identifier = ToTrackedIdentifier(enriched_instance_identifier);
    if (!tracked_identifier.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation);
    }

    const auto handle = make_FindServiceHandle(next_find_service_uid_++);
    return StartFindServiceImpl(handle, std::move(handler), std::vector<TrackedIdentifier>{*tracked_identifier});
}

auto ServiceDiscoveryCompat::StartFindServiceImpl(FindServiceHandle handle,
                                                  FindServiceHandler<HandleType> handler,
                                                  const std::vector<TrackedIdentifier>& tracked_identifiers) noexcept
    -> Result<FindServiceHandle>
{
    if (!EnsureClientStarted())
    {
        return MakeUnexpected(ComErrc::kCommunicationStackError);
    }

    {
        const std::lock_guard lock{mutex_};
        SearchEntry search_entry{};
        search_entry.handler = std::make_shared<FindServiceHandler<HandleType>>(std::move(handler));
        search_entry.tracked_identifiers = tracked_identifiers;
        searches_.emplace(handle, std::move(search_entry));

        for (const auto& tracked_identifier : tracked_identifiers)
        {
            auto& handles = key_to_handles_[tracked_identifier.key];
            if (!ContainsHandle(handles, handle))
            {
                handles.push_back(handle);
            }
        }
    }

    for (const auto& tracked_identifier : tracked_identifiers)
    {
        bool first_subscription{false};
        {
            const std::lock_guard lock{mutex_};
            const auto& handles = key_to_handles_[tracked_identifier.key];
            first_subscription = (handles.size() == 1U);
        }

        if (first_subscription)
        {
            const auto response = client_.StartFindService(
                tracked_identifier.key,
                [this](const score::mw::com::service_discovery::ProtocolNotification& notification) {
                    OnNotification(notification);
                });
            if (!response.has_value())
            {
                score::cpp::ignore = StopFindService(handle);
                return MakeUnexpected(ComErrc::kBindingFailure);
            }

            const std::lock_guard lock{mutex_};
            key_to_daemon_find_handle_[tracked_identifier.key] = response->search_handle;
            snapshots_[tracked_identifier.key] = response->registrations;
        }
    }

    const std::lock_guard lock{mutex_};
    auto* search_entry = FindSearch(handle);
    if (search_entry != nullptr)
    {
        // Copy shared_ptr to extend lifetime: callback may call StopFindService,
        // erasing searches_[handle] and dropping ref count to zero, freeing the
        // handler while it is still executing.
        const auto initial_handler = search_entry->handler;
        if (initial_handler)
        {
            const auto handles = BuildHandleContainer(*search_entry);
            if (!handles.empty())
            {
                (*initial_handler)(handles, handle);
            }
        }
    }

    return handle;
}

auto ServiceDiscoveryCompat::StopFindService(const FindServiceHandle handle) noexcept -> Result<void>
{
    std::vector<std::uint64_t> daemon_handles_to_stop{};
    {
        const std::lock_guard lock{mutex_};
        const auto search_it = searches_.find(handle);
        if (search_it == searches_.end())
        {
            return MakeUnexpected(ComErrc::kInvalidHandle);
        }

        for (const auto& tracked_identifier : search_it->second.tracked_identifiers)
        {
            auto handles_it = key_to_handles_.find(tracked_identifier.key);
            if (handles_it != key_to_handles_.end())
            {
                auto& handles = handles_it->second;
                handles.erase(std::remove(handles.begin(), handles.end(), handle), handles.end());
                if (handles.empty())
                {
                    key_to_handles_.erase(handles_it);
                    snapshots_.erase(tracked_identifier.key);
                    const auto daemon_handle_it = key_to_daemon_find_handle_.find(tracked_identifier.key);
                    if (daemon_handle_it != key_to_daemon_find_handle_.end())
                    {
                        daemon_handles_to_stop.push_back(daemon_handle_it->second);
                        key_to_daemon_find_handle_.erase(daemon_handle_it);
                    }
                }
            }
        }

        searches_.erase(search_it);
    }

    for (const auto daemon_handle : daemon_handles_to_stop)
    {
        if (!client_.StopFindService(daemon_handle))
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }

    return {};
}

auto ServiceDiscoveryCompat::FindService(InstanceIdentifier instance_identifier) noexcept
    -> Result<ServiceHandleContainer<HandleType>>
{
    if (!EnsureClientStarted())
    {
        return MakeUnexpected(ComErrc::kCommunicationStackError);
    }

    const auto tracked_identifier = ToTrackedIdentifier(EnrichedInstanceIdentifier{instance_identifier});
    if (!tracked_identifier.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation);
    }

    score::mw::com::service_discovery::ProtocolRequest request{};
    request.operation = score::mw::com::service_discovery::ProtocolOperation::kResolve;
    request.registration.key = tracked_identifier->key;
    const auto response = client_.Request(request);
    if (!response.has_value() || response->status_code != 0U)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    SearchEntry search_entry{};
    search_entry.tracked_identifiers.push_back(*tracked_identifier);
    {
        const std::lock_guard lock{mutex_};
        snapshots_[tracked_identifier->key] = response->registrations;
        return BuildHandleContainer(search_entry);
    }
}

auto ServiceDiscoveryCompat::FindService(InstanceSpecifier instance_specifier) noexcept
    -> Result<ServiceHandleContainer<HandleType>>
{
    const auto instance_identifiers = runtime_.resolve(instance_specifier);
    if (instance_identifiers.empty())
    {
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }

    ServiceHandleContainer<HandleType> handles{};
    bool any_success{false};
    for (auto& instance_identifier : instance_identifiers)
    {
        auto result = FindService(std::move(instance_identifier));
        if (result.has_value())
        {
            any_success = true;
            for (const auto& handle : result.value())
            {
                handles.push_back(handle);
            }
        }
    }

    if (!any_success)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return handles;
}

}  // namespace score::mw::com::impl
