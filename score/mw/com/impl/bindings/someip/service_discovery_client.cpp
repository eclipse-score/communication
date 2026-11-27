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
#include "score/mw/com/impl/bindings/someip/service_discovery_client.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_version_type.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"

#include "score/mw/log/logging.h"
#include <score/utility.hpp>

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::someip
{
namespace
{

std::optional<score::someip_sd::ServiceDetails> GetOfferDetails(const InstanceIdentifier& instance_identifier)
{
    const InstanceIdentifierView instance_identifier_view{instance_identifier};
    const auto& service_instance_deployment = instance_identifier_view.GetServiceInstanceDeployment();
    const auto* const instance_deployment =
        std::get_if<SomeIpServiceInstanceDeployment>(&service_instance_deployment.bindingInfo_);
    if ((instance_deployment == nullptr) || !instance_deployment->instance_id_.has_value())
    {
        return std::nullopt;
    }

    const auto& service_type_deployment = instance_identifier_view.GetServiceTypeDeployment();
    const auto* const someip_service_type =
        std::get_if<SomeIpServiceTypeDeployment>(&service_type_deployment.binding_info_);
    if (someip_service_type == nullptr)
    {
        return std::nullopt;
    }

    const auto service_version = ServiceIdentifierTypeView{service_instance_deployment.service_}.GetVersion();
    const ServiceVersionTypeView version{service_version};
    if (version.getMajor() > std::numeric_limits<std::uint8_t>::max())
    {
        return std::nullopt;
    }

    return score::someip_sd::ServiceDetails{someip_service_type->service_id_,
                                            instance_deployment->instance_id_->GetId(),
                                            static_cast<std::uint8_t>(version.getMajor()),
                                            version.getMinor()};
}

constexpr auto kConnectTimeout = std::chrono::seconds{1};

}  // namespace

ServiceDiscoveryClient::ServiceDiscoveryClient(score::message_passing::IClientFactory& client_factory)
    : sd_client_{client_factory,
                 [this](std::uint16_t service, std::uint16_t instance, bool available) {
                     OnAvailability(ServiceKey{service, instance}, available);
                 },
                 {},
                 [this]() {
                     OnConnectionLost();
                 }}
{
    dispatcher_thread_ = std::thread{[this]() {
        RunDispatcher();
    }};
}

ServiceDiscoveryClient::~ServiceDiscoveryClient() noexcept
{
    // Removed entries own user handlers; they are destroyed only after worker_mutex_ was released.
    std::deque<std::shared_ptr<SearchRequest>> dropped_notifications{};
    {
        const std::lock_guard lock{worker_mutex_};
        dispatcher_stop_requested_ = true;
        dropped_notifications.swap(dispatch_queue_);
    }
    dispatch_condition_.notify_all();
    if (dispatcher_thread_.joinable())
    {
        dispatcher_thread_.join();
    }
    SearchRequests removed{};
    const std::lock_guard lock{worker_mutex_};
    removed.swap(search_requests_);
    service_requests_.clear();
}

Result<void> ServiceDiscoveryClient::OfferService(const InstanceIdentifier instance_identifier)
{
    const auto details = GetOfferDetails(instance_identifier);
    if (!details.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                              "SOME/IP offer requires a configured instance ID and an 8-bit major version");
    }
    // A reconnect must not happen while a search is between its session check and its request send.
    const std::lock_guard control_lock{control_mutex_};
    if (!sd_client_.Connect(kConnectTimeout).has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not connect to the SOME/IP SD daemon");
    }
    if (!sd_client_.OfferService(details->service, details->instance, details->major, details->minor))
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not send SOME/IP offer to the SD daemon");
    }
    return {};
}

Result<void> ServiceDiscoveryClient::StopOfferService(
    const InstanceIdentifier instance_identifier,
    const IServiceDiscovery::QualityTypeSelector quality_type_selector)
{
    if (quality_type_selector != IServiceDiscovery::QualityTypeSelector::kBoth)
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                              "The SOME/IP SD control protocol cannot stop an offer by quality type");
    }
    const auto details = GetOfferDetails(instance_identifier);
    if (!details.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                              "SOME/IP offer requires a configured instance ID and an 8-bit major version");
    }
    const std::lock_guard control_lock{control_mutex_};
    if (!sd_client_.Connect(kConnectTimeout).has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not connect to the SOME/IP SD daemon");
    }
    if (!sd_client_.StopOfferService(details->service, details->instance))
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not stop SOME/IP offer at the SD daemon");
    }
    return {};
}

Result<void> ServiceDiscoveryClient::StartFindService(const FindServiceHandle find_service_handle,
                                                      FindServiceHandler<HandleType> handler,
                                                      const EnrichedInstanceIdentifier enriched_instance_identifier)
{
    const auto details = GetOfferDetails(enriched_instance_identifier.GetInstanceIdentifier());
    if (!details.has_value())
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                              "SOME/IP service discovery requires a configured instance ID and an 8-bit major version");
    }

    const std::lock_guard control_lock{control_mutex_};
    std::shared_ptr<SearchRequest> search_request{};
    {
        const std::lock_guard lock{worker_mutex_};
        if (HasSearchRequest(find_service_handle, enriched_instance_identifier.GetInstanceIdentifier()))
        {
            return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP search is already active");
        }

        search_request =
            std::make_shared<SearchRequest>(SearchRequest{find_service_handle,
                                                          enriched_instance_identifier.GetInstanceIdentifier(),
                                                          ServiceKey{details->service, details->instance},
                                                          std::move(handler),
                                                          session_generation_,
                                                          false,
                                                          {},
                                                          false,
                                                          false});
        const auto registered = RegisterSearchRequest(search_request, *details);
        if (!registered.has_value())
        {
            return Unexpected{registered.error()};
        }
    }

    if (!sd_client_.Connect(kConnectTimeout).has_value())
    {
        const std::lock_guard lock{worker_mutex_};
        RemoveSearchRequest(search_request);
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not connect to the SOME/IP SD daemon");
    }
    return SendServiceRequest(search_request, *details);
}

Result<void> ServiceDiscoveryClient::SendServiceRequest(const std::shared_ptr<SearchRequest>& search_request,
                                                        const score::someip_sd::ServiceDetails& details)
{
    bool send_required{false};
    {
        const std::lock_guard lock{worker_mutex_};
        if (!IsCurrentSearchRequest(search_request))
        {
            RemoveSearchRequest(search_request);
            return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP SD session was lost while starting the search");
        }
        const auto service_request = service_requests_.find(search_request->key);
        send_required = (service_request != service_requests_.cend()) && !service_request->second.request_sent;
    }

    if (send_required && !sd_client_.RequestService(details))
    {
        const std::lock_guard lock{worker_mutex_};
        RemoveSearchRequest(search_request);
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not send SOME/IP request to the SD daemon");
    }

    {
        const std::lock_guard lock{worker_mutex_};
        if (!IsCurrentSearchRequest(search_request))
        {
            RemoveSearchRequest(search_request);
            return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP SD session was lost while starting the search");
        }
        auto service_request = service_requests_.find(search_request->key);
        if (service_request != service_requests_.end())
        {
            service_request->second.request_sent = true;
        }
        search_request->active = true;
        EnqueuePendingNotifications();
    }
    dispatch_condition_.notify_all();
    return {};
}

bool ServiceDiscoveryClient::HasSearchRequest(const FindServiceHandle handle,
                                              const InstanceIdentifier& instance_identifier) const
{
    return std::any_of(search_requests_.cbegin(),
                       search_requests_.cend(),
                       [&handle, &instance_identifier](const auto& search_request) {
                           return (search_request->handle == handle) &&
                                  (search_request->instance_identifier == instance_identifier);
                       });
}

bool ServiceDiscoveryClient::IsCurrentSearchRequest(const std::shared_ptr<SearchRequest>& search_request) const
{
    // Only equality is evaluated, so a (theoretical) wrap-around of the generation counter is harmless.
    return (search_request->generation == session_generation_) &&
           (std::find(search_requests_.cbegin(), search_requests_.cend(), search_request) != search_requests_.cend()) &&
           (service_requests_.find(search_request->key) != service_requests_.cend());
}

Result<void> ServiceDiscoveryClient::RegisterSearchRequest(const std::shared_ptr<SearchRequest>& search_request,
                                                           const score::someip_sd::ServiceDetails& details)
{
    const auto key = search_request->key;
    auto request = service_requests_.find(key);
    const bool is_first_user = request == service_requests_.end();
    if (!is_first_user && ((request->second.major != details.major) || (request->second.minor != details.minor)))
    {
        return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                              "Concurrent SOME/IP searches for one instance with different versions are unsupported");
    }

    search_requests_.push_back(search_request);
    if (is_first_user)
    {
        score::cpp::ignore =
            service_requests_.emplace(key, ServiceRequest{details.major, details.minor, 1U, false, false});
    }
    else
    {
        ++request->second.users;
    }

    return {};
}

void ServiceDiscoveryClient::RemoveSearchRequest(const std::shared_ptr<SearchRequest>& search_request)
{
    const auto position = std::find(search_requests_.begin(), search_requests_.end(), search_request);
    if (position == search_requests_.end())
    {
        return;
    }
    search_requests_.erase(position);
    auto service_request = service_requests_.find(search_request->key);
    if (service_request != service_requests_.end())
    {
        --service_request->second.users;
        if (service_request->second.users == 0U)
        {
            service_requests_.erase(service_request);
        }
    }
    dispatch_queue_.erase(std::remove(dispatch_queue_.begin(), dispatch_queue_.end(), search_request),
                          dispatch_queue_.end());
    search_request->queued = false;
}

void ServiceDiscoveryClient::PurgeQueuedNotifications(const FindServiceHandle handle, SearchRequests& removed)
{
    const auto first_purged = std::stable_partition(
        dispatch_queue_.begin(), dispatch_queue_.end(), [&handle](const std::shared_ptr<SearchRequest>& queued) {
            return !(queued->handle == handle);
        });
    for (auto purged = first_purged; purged != dispatch_queue_.end(); ++purged)
    {
        (*purged)->queued = false;
        removed.push_back(std::move(*purged));
    }
    dispatch_queue_.erase(first_purged, dispatch_queue_.end());
}

void ServiceDiscoveryClient::NotifySearchRequest(const SearchRequest& search_request, const bool available)
{
    ServiceHandleContainer<HandleType> handles{};
    if (available)
    {
        handles.push_back(make_HandleType(search_request.instance_identifier));
    }
    search_request.handler(std::move(handles), search_request.handle);
}

Result<void> ServiceDiscoveryClient::StopFindService(const FindServiceHandle find_service_handle)
{
    bool release_failed{false};
    // Erased searches own user handlers; they are destroyed after worker_mutex_ was released.
    SearchRequests removed{};
    {
        const std::lock_guard control_lock{control_mutex_};
        std::vector<ServiceKey> keys_to_release{};
        {
            const std::lock_guard lock{worker_mutex_};
            for (auto search_request = search_requests_.begin(); search_request != search_requests_.end();)
            {
                if (!((*search_request)->handle == find_service_handle))
                {
                    ++search_request;
                    continue;
                }
                auto request = service_requests_.find((*search_request)->key);
                if (request != service_requests_.end())
                {
                    --request->second.users;
                    if (request->second.users == 0U)
                    {
                        if (request->second.request_sent)
                        {
                            keys_to_release.push_back(request->first);
                        }
                        service_requests_.erase(request);
                    }
                }
                removed.push_back(std::move(*search_request));
                search_request = search_requests_.erase(search_request);
            }
            PurgeQueuedNotifications(find_service_handle, removed);
        }
        // The local bookkeeping is removed even if the release fails: the shared ServiceDiscovery has already
        // forgotten the handle, so retained state would be unreachable.
        for (const auto& key : keys_to_release)
        {
            if (!sd_client_.ReleaseService(key.service, key.instance))
            {
                release_failed = true;
            }
        }
    }

    WaitForInFlightHandler(find_service_handle);

    if (release_failed)
    {
        ::score::mw::log::LogError("someip") << "Could not release SOME/IP request from the SD daemon";
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not release SOME/IP request from the SD daemon");
    }
    return {};
}

Result<ServiceHandleContainer<HandleType>> ServiceDiscoveryClient::FindService(
    const EnrichedInstanceIdentifier enriched_instance_identifier)
{
    score::cpp::ignore = enriched_instance_identifier;
    return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                          "SOME/IP FindService requires an SD availability snapshot that the current protocol lacks");
}

void ServiceDiscoveryClient::OnAvailability(const ServiceKey key, const bool available)
{
    {
        const std::lock_guard lock{worker_mutex_};
        auto request = service_requests_.find(key);
        if ((request == service_requests_.end()) || (request->second.available == available))
        {
            return;
        }
        request->second.available = available;
        EnqueuePendingNotifications();
    }
    dispatch_condition_.notify_all();
}

void ServiceDiscoveryClient::EnqueuePendingNotifications()
{
    for (const auto& search_request : search_requests_)
    {
        if (!search_request->active)
        {
            continue;
        }
        const auto request = service_requests_.find(search_request->key);
        if (request == service_requests_.end())
        {
            continue;
        }
        const bool available = request->second.available;
        if (!search_request->queued)
        {
            if (search_request->last_delivered_availability == available)
            {
                continue;
            }
            if (!search_request->last_delivered_availability.has_value() && !available)
            {
                continue;
            }
        }
        // The state and the queue are updated under the same lock, so the delivery order equals the state order.
        EnqueueNotification(search_request, available);
    }
}

void ServiceDiscoveryClient::EnqueueNotification(const std::shared_ptr<SearchRequest>& search_request,
                                                 const bool available)
{
    // Coalescing bounds the queue by the number of searches; the dispatcher delivers the latest state only.
    search_request->pending_availability = available;
    if (!search_request->queued)
    {
        search_request->queued = true;
        dispatch_queue_.push_back(search_request);
    }
}

void ServiceDiscoveryClient::RunDispatcher()
{
    std::unique_lock lock{worker_mutex_};
    while (!dispatcher_stop_requested_)
    {
        if (dispatch_queue_.empty())
        {
            dispatch_condition_.wait(lock);
            continue;
        }
        std::shared_ptr<SearchRequest> search_request = std::move(dispatch_queue_.front());
        dispatch_queue_.pop_front();
        search_request->queued = false;
        const bool available = search_request->pending_availability;
        const auto& last_delivered = search_request->last_delivered_availability;
        // Skip coalesced no-op changes and never send an empty first callback.
        const bool deliver = !(last_delivered == available) && (last_delivered.has_value() || available);
        if (deliver)
        {
            search_request->last_delivered_availability = available;
            in_flight_handle_ = search_request->handle;
        }
        lock.unlock();
        if (deliver)
        {
            NotifySearchRequest(*search_request, available);
        }
        // May be the last owner of the handler, so it is released without worker_mutex_.
        search_request.reset();
        lock.lock();
        if (deliver)
        {
            in_flight_handle_.reset();
            handler_done_condition_.notify_all();
        }
    }
}

void ServiceDiscoveryClient::WaitForInFlightHandler(const FindServiceHandle handle)
{
    // A handler stopping its own (or another) search runs on the dispatcher thread and must not wait for itself.
    if (std::this_thread::get_id() == dispatcher_thread_.get_id())
    {
        return;
    }
    std::unique_lock lock{worker_mutex_};
    handler_done_condition_.wait(lock, [this, &handle]() {
        return !in_flight_handle_.has_value() || !(in_flight_handle_.value() == handle);
    });
}

void ServiceDiscoveryClient::OnConnectionLost()
{
    // Invalidated searches own user handlers; those not referenced by the queue are destroyed after unlocking.
    SearchRequests invalidated{};
    {
        const std::lock_guard lock{worker_mutex_};
        ++session_generation_;
        for (const auto& search_request : search_requests_)
        {
            // A queued entry may still carry a not yet delivered availability; the dispatcher drops the loss
            // notification if the search was never reported available.
            if (search_request->active &&
                (search_request->queued || search_request->last_delivered_availability.value_or(false)))
            {
                EnqueueNotification(search_request, false);
            }
        }
        invalidated.swap(search_requests_);
        service_requests_.clear();
    }
    dispatch_condition_.notify_all();
    ::score::mw::log::LogError("someip")
        << "SOME/IP SD connection lost; offers and active searches were invalidated and must be re-issued";
}

}  // namespace score::mw::com::impl::someip
