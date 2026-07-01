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
 *******************************************************************************/

#include "score/mw/com/gateway/gateway_application/gateway_application.h"

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"

#include "score/mw/com/gateway/gateway_application/gateway_error.h"
#include "score/mw/com/gateway/transport_layer/transport_factory.h"
#include "score/mw/log/logging.h"
#include <score/assert.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::gateway
{

namespace
{
// Subscribe with 1 sample — the gateway just needs notification that data changed,
// actual data is read directly from shared memory by the consumer.
constexpr std::size_t kGatewaySubscribeSamples = 1U;

}  // namespace

GatewayApplication::~GatewayApplication()
{
    // Shutdown the transport BEFORE expiring the scope. Scope::Expire() blocks until all
    // in-flight scoped callbacks complete. A subscription-change callback may be executing
    // SendRequest (waiting for an ACK). Shutting down the transport first sets
    // is_connected_ = false, which unblocks any pending SendRequest immediately.
    // Without this ordering, Expire() deadlocks: it waits for the callback, but the
    // callback waits for Shutdown() (which would set is_connected_ = false) that is
    // sequenced after Expire().
    if (transport_layer_)
    {
        transport_layer_->Shutdown();
    }
    scope_.Expire();
    find_handles_.clear();
    for (auto& [specifier, skeleton] : skeletons_)
    {
        skeleton.StopOfferService();
    }
    skeletons_.clear();
    proxies_.clear();
}

score::Result<void> GatewayApplication::Setup()
{
    if (!transport_layer_)
    {
        transport_layer_ = TransportFactory::Create(
            *this, app_configuration_.GetTransportLayerId(), app_configuration_.GetTransportConfigPath());
    }
    return transport_layer_->Setup();
}

score::Result<void> GatewayApplication::Start()
{
    return StartServiceDiscovery();
}

bool GatewayApplication::IsServiceInstanceAccepted(std::string_view service_instance_str) const
{
    const auto received_services = app_configuration_.GetReceivedServices();
    return std::any_of(received_services.cbegin(),
                       received_services.cend(),
                       [service_instance_str](const std::string& expected_service) {
                           return expected_service == service_instance_str;
                       });
}

score::Result<void> GatewayApplication::StartServiceDiscovery()
{
    const auto& forwarded_services = app_configuration_.GetForwardedServices();

    for (const auto& service_str : forwarded_services)
    {
        const auto specifier_result = impl::InstanceSpecifier::Create(std::string{service_str});
        if (!specifier_result.has_value())
        {
            score::mw::log::LogError() << "GatewayApplication: Invalid instance specifier: " << service_str;
            continue;
        }

        auto scoped_find_callback = std::make_shared<FindCallbackScopedCb>(
            scope_,
            [this, specifier = std::string(service_str)](impl::ServiceHandleContainer<impl::HandleType> handles,
                                                         impl::FindServiceHandle /*find_handle*/) {
                OnServiceAvailabilityChanged(specifier, std::move(handles));
            });

        auto find_result = impl::GenericProxy::StartFindService(
            [scoped_find_callback](impl::ServiceHandleContainer<impl::HandleType> handles,
                                   impl::FindServiceHandle find_handle) {
                (*scoped_find_callback)(std::move(handles), std::move(find_handle));
            },
            std::move(specifier_result).value());

        if (!find_result.has_value())
        {
            score::mw::log::LogError() << "GatewayApplication: Failed to start service discovery for " << service_str;
            continue;
        }

        find_handles_.push_back(std::move(find_result).value());
        score::mw::log::LogInfo() << "GatewayApplication: Started service discovery for " << service_str;
    }

    return {};
}

void GatewayApplication::OnServiceAvailabilityChanged(const std::string& specifier_str,
                                                      std::vector<impl::HandleType> handles)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (handles.empty())
    {
        score::mw::log::LogInfo() << "GatewayApplication: Service disappeared: " << specifier_str;
        OnServiceUnavailable(specifier_str);
        return;
    }

    if (proxies_.count(specifier_str) > 0U)
    {
        // Proxy kept alive from a previous cycle (see OnServiceUnavailable). Re-propagate so
        // the remote gateway recreates its skeleton.
        score::mw::log::LogInfo() << "GatewayApplication: Service re-found, re-propagating: " << specifier_str;
        PropagateService(specifier_str);
        return;
    }

    score::mw::log::LogInfo() << "GatewayApplication: Service found: " << specifier_str;

    auto proxy_result = impl::GenericProxy::Create(std::move(handles.front()));
    if (!proxy_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Failed to create proxy for " << specifier_str;
        return;
    }

    proxies_.emplace(specifier_str, std::move(proxy_result).value());
    PropagateService(specifier_str);
}

void GatewayApplication::OnServiceUnavailable(const std::string& specifier_str)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto it = proxies_.find(specifier_str);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != proxies_.cend(),
                                                "GatewayApplication: Service unavailable but proxy not found");

    score::mw::log::LogInfo() << "GatewayApplication: Service no longer available: " << specifier_str;

    auto specifier_result = impl::InstanceSpecifier::Create(std::string{specifier_str});
    if (specifier_result.has_value())
    {
        auto result = transport_layer_->StopOfferService(std::move(specifier_result).value());
        if (!result.has_value())
        {
            score::mw::log::LogWarn() << "GatewayApplication: Failed to forward StopOfferService for " << specifier_str;
        }
    }

    // Note: Do NOT erase the proxy. For MemorySharing gateways the proxy instance holds the
    //  service_usage_marker_file lock, which prevents SHM object unlinking while remote
    //  proxies on the destination domain may still be accessing the shared memory.
}

void GatewayApplication::PropagateService(const std::string& specifier_str)
{
    const auto proxy_it = proxies_.find(specifier_str);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_it != proxies_.cend(),
                                                "GatewayApplication: Proxy not found in PropagateService");

    auto specifier_result = impl::InstanceSpecifier::Create(std::string{specifier_str});
    if (!specifier_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Invalid instance specifier: " << specifier_str;
        return;
    }

    std::vector<impl::EventInfo> elements{};
    const auto& event_map = proxy_it->second.GetEvents();
    for (auto it = event_map.cbegin(); it != event_map.cend(); ++it)
    {
        const auto sample_size = it->second.GetSampleSize();
        elements.push_back(impl::EventInfo{it->first, impl::DataTypeMetaInfo{sample_size, 0U}});
    }

    auto provide_result = transport_layer_->ProvideService(std::move(specifier_result).value(), std::move(elements));
    if (!provide_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: ProvideService failed for " << specifier_str
                                   << ", will retry on next discovery";
        // Next time the service instance gets found again, proxy will get recreated and PropagateService
        // will be retried.
        proxies_.erase(specifier_str);
        return;
    }

    score::mw::log::LogInfo() << "GatewayApplication: Propagated service " << specifier_str;
}

void GatewayApplication::RegisterEventReceiveHandlerCallback(impl::GenericSkeleton& skeleton,
                                                             const std::string& specifier_str,
                                                             const std::string& event_name)
{
    // TODO: GenericSkeletonEvent::SetReceiveHandlerNotificationCallback is not yet available in the OSS impl.
    // Once available, remove the (void) suppressions below and uncomment the implementation block.
    (void)skeleton;
    (void)specifier_str;
    (void)event_name;

    // using NotificationCallback = safecpp::MoveOnlyScopedFunction<void(bool)>;
    // auto scoped_callback = std::make_shared<NotificationCallback>(
    //     scope_, [this, specifier = specifier_str, event = event_name](bool has_subscribers) {
    //         OnSubscriptionStateChanged(specifier, event, has_subscribers);
    //     });
    //
    // auto result = skeleton.SetReceiveHandlerNotificationCallback(
    //     event_name, [scoped_callback](bool has_subscribers) { (*scoped_callback)(has_subscribers); });
    //
    // if (!result.has_value())
    // {
    //     score::mw::log::LogError() << "GatewayApplication: Failed to set subscription callback for "
    //                                << event_name << " of " << specifier_str;
    // }
}

void GatewayApplication::OnSubscriptionStateChanged(const std::string& specifier_str,
                                                    const std::string& event_name,
                                                    bool has_subscribers)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto specifier_result = impl::InstanceSpecifier::Create(std::string{specifier_str});
    if (!specifier_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Invalid instance specifier in subscription callback: "
                                   << specifier_str;
        return;
    }

    auto& active_events = active_event_subscriptions_[specifier_str];

    if (has_subscribers)
    {
        if (!active_events.insert(event_name).second)
        {
            // Already registered — LoLa fires this callback more than once during the
            // PrepareOffer/PrepareStopOffer/PrepareOffer SHM resize cycle. Deduplicate.
            score::mw::log::LogDebug() << "GatewayApplication: Duplicate subscription callback for " << event_name
                                       << " of " << specifier_str << ", skipping";
            return;
        }

        score::mw::log::LogInfo() << "GatewayApplication: Consumer subscribed to " << event_name << " of "
                                  << specifier_str << ", registering for updates at source gateway";

        auto result = transport_layer_->RegisterUpdateNotification(
            std::move(specifier_result).value(), impl::ServiceElementType::EVENT, std::string{event_name});
        if (!result.has_value())
        {
            score::mw::log::LogError() << "GatewayApplication: Failed to register for updates: " << event_name << " of "
                                       << specifier_str;
        }
    }
    else
    {
        if (active_events.erase(event_name) == 0U)
        {
            return;
        }

        score::mw::log::LogInfo() << "GatewayApplication: All consumers unsubscribed from " << event_name << " of "
                                  << specifier_str << ", unregistering from updates at source gateway";

        auto result = transport_layer_->UnregisterUpdateNotification(
            std::move(specifier_result).value(), impl::ServiceElementType::EVENT, std::string{event_name});
        if (!result.has_value())
        {
            score::mw::log::LogError() << "GatewayApplication: Failed to unregister from updates: " << event_name
                                       << " of " << specifier_str;
        }
    }
}

void GatewayApplication::ReRegisterActiveEventSubscriptions(const std::string& specifier_str)
{
    const auto sub_it = active_event_subscriptions_.find(specifier_str);
    if (sub_it == active_event_subscriptions_.end() || sub_it->second.empty())
    {
        score::mw::log::LogInfo() << "GatewayApplication: No active event subscriptions to re-register for "
                                  << specifier_str;
        return;
    }

    for (const auto& event_name : sub_it->second)
    {
        auto specifier_result = impl::InstanceSpecifier::Create(std::string{specifier_str});
        if (!specifier_result.has_value())
        {
            continue;
        }

        score::mw::log::LogInfo() << "GatewayApplication: Re-registering subscription for " << event_name << " of "
                                  << specifier_str;

        auto result = transport_layer_->RegisterUpdateNotification(
            std::move(specifier_result).value(), impl::ServiceElementType::EVENT, std::string{event_name});
        if (!result.has_value())
        {
            score::mw::log::LogError() << "GatewayApplication: Failed to re-register for updates: " << event_name
                                       << " of " << specifier_str;
        }
    }
}

score::Result<void> GatewayApplication::ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                                       std::vector<impl::EventInfo> service_elements)
{
    if (!IsServiceInstanceAccepted(service_instance_specifier.ToString()))
    {
        score::mw::log::LogWarn()
            << "GatewayApplication: Received request to provide a service instance locally, which has not "
               "been whitelisted in the configuration under <expected-received-services>: "
            << service_instance_specifier;
        return MakeUnexpected(GatewayErrorc::kNonWhitelistedService);
    }
    // transparent hashing/key-equal is only there in C++20 for std::unordered_map::find ... so we have to create
    // a std::string from the specifiers string_view 1st ... but it is just a PoC right now ;)
    auto service_instance_specifier_str = std::string(service_instance_specifier.ToString());
    auto existing_it = skeletons_.find(service_instance_specifier_str);
    if (existing_it != skeletons_.end())
    {
        // Reuse the existing skeleton. Creating a new one would do placement-new on the SHM
        // EventSubscriptionControl, zeroing it while the local consumer proxy still holds an
        // active subscription — causing a fatal Unsubscribe assertion crash at shutdown.
        // Re-offer (if stopped by HandleStopOfferServiceRequest) and re-register event
        // subscriptions so the (re-started) source gateway knows which events have active consumers.
        score::mw::log::LogInfo() << "GatewayApplication: Reusing existing skeleton for " << service_instance_specifier;

        auto offer_result = existing_it->second.OfferService();
        if (!offer_result.has_value())
        {
            score::mw::log::LogWarn() << "GatewayApplication: OfferService on reused skeleton returned error for "
                                      << service_instance_specifier << " (may already be offered)";
        }

        ReRegisterActiveEventSubscriptions(service_instance_specifier_str);
        return {};
    }

    impl::GenericSkeletonServiceElementInfo skeleton_info;
    skeleton_info.events = service_elements;
    auto skeleton_result = impl::GenericSkeleton::Create(service_instance_specifier, skeleton_info);
    if (!skeleton_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Failed to create skeleton for "
                                   << service_instance_specifier;
        return MakeUnexpected(GatewayErrorc::kSkeletonCreationFailed);
    }

    skeletons_.emplace(service_instance_specifier_str, std::move(skeleton_result).value());

    // Register per-event receive handler registration callbacks. When a local consumer/proxy registers an
    // event-receive-handler for an event at the skeleton, the callback we register here, forwards a
    // RegisterUpdateNotification to the source gateway, which then registers an event-receive-handler at the actual
    // proxy event and begins forwarding update notifications back to the destination gateway.
    auto& skeleton = skeletons_.at(service_instance_specifier_str);
    for (const auto& element : service_elements)
    {
        RegisterEventReceiveHandlerCallback(skeleton, service_instance_specifier_str, std::string{element.name});
    }

    auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Failed to offer service for " << service_instance_specifier;
        skeletons_.erase(service_instance_specifier_str);
        return MakeUnexpected(GatewayErrorc::kSkeletonCreationFailed);
    }

    score::mw::log::LogInfo() << "GatewayApplication: Created and offered skeleton for "
                              << service_instance_specifier_str;
    return {};
}

void GatewayApplication::StopOfferService(impl::InstanceSpecifier service_instance_specifier)
{
    auto service_instance_specifier_str = std::string(service_instance_specifier.ToString());
    score::mw::log::LogInfo() << "GatewayApplication: StopOfferService for " << service_instance_specifier;

    auto it = skeletons_.find(service_instance_specifier_str);
    if (it == skeletons_.end())
    {
        score::mw::log::LogWarn() << "GatewayApplication: No skeleton found for " << service_instance_specifier;
        return;
    }

    // Stop offering but keep the skeleton. The source side may decide to re-offer anytime, then we need our skeleton
    // anyhow.
    it->second.StopOfferService();
    score::mw::log::LogInfo() << "GatewayApplication: Stopped offering " << service_instance_specifier
                              << " (skeleton kept for reuse)";
}

score::Result<void> GatewayApplication::OfferService(impl::InstanceSpecifier service_instance_specifier)
{
    auto specifier_str = std::string(service_instance_specifier.ToString());
    score::mw::log::LogInfo() << "GatewayApplication: HandleOfferServiceRequest for " << service_instance_specifier;

    auto it = skeletons_.find(specifier_str);
    if (it == skeletons_.end())
    {
        score::mw::log::LogError() << "GatewayApplication: No skeleton found for " << service_instance_specifier;
        return MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed);
    }

    auto offer_result = it->second.OfferService();
    if (!offer_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Failed to offer service for " << service_instance_specifier;
        return MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed);
    }

    score::mw::log::LogInfo() << "GatewayApplication: Offered skeleton service for " << service_instance_specifier;
    return {};
}

score::Result<void> GatewayApplication::RegisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                                   impl::ServiceElementType element_type,
                                                                   std::string element_name)
{
    auto specifier_str = std::string(service_instance_specifier.ToString());
    score::mw::log::LogInfo() << "GatewayApplication: HandleRegisterNotificationRequest for "
                              << service_instance_specifier << " element " << element_name;

    auto it = proxies_.find(specifier_str);
    if (it == proxies_.end())
    {
        score::mw::log::LogError() << "GatewayApplication: No proxy found for " << specifier_str;
        return MakeUnexpected(GatewayErrorc::kUnknownServiceInstance);
    }

    auto event_map = it->second.GetEvents();
    auto event_it = event_map.find(element_name);
    if (event_it == event_map.cend())
    {
        score::mw::log::LogError() << "GatewayApplication: Event " << element_name << " not found in proxy for "
                                   << specifier_str;
        return MakeUnexpected(GatewayErrorc::kUnknownServiceElement);
    }

    auto& proxy_event = event_it->second;
    if (!proxy_event.Subscribe(kGatewaySubscribeSamples).has_value())
    {
        score::mw::log::LogWarn() << "GatewayApplication: Subscribe failed for event " << element_name << " of "
                                  << specifier_str << " (may already be subscribed)";
    }

    using ReceiveCallback = safecpp::MoveOnlyScopedFunction<void()>;
    auto scoped_handler = std::make_shared<ReceiveCallback>(
        scope_, [this, spec = specifier_str, elem_name = std::string(element_name), elem_type = element_type]() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            auto specifier_result = impl::InstanceSpecifier::Create(std::string{spec});
            if (specifier_result.has_value())
            {
                transport_layer_->NotifyUpdate(std::move(specifier_result).value(), elem_type, std::string{elem_name});
            }
        });

    auto handler_result = proxy_event.SetReceiveHandler([scoped_handler]() noexcept {
        (*scoped_handler)();
    });

    if (!handler_result.has_value())
    {
        score::mw::log::LogError() << "GatewayApplication: Failed to set receive handler for " << element_name;
        return MakeUnexpected(GatewayErrorc::kReceiveHandlerRegistrationFailed);
    }
    return {};
}

score::Result<void> GatewayApplication::UnregisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                                     impl::ServiceElementType element_type,
                                                                     std::string element_name)
{
    auto specifier_str = std::string(service_instance_specifier.ToString());

    score::mw::log::LogInfo() << "GatewayApplication: HandleUnregisterNotificationRequest for " << specifier_str
                              << " element " << element_name;

    auto it = proxies_.find(specifier_str);
    if (it == proxies_.end())
    {
        return MakeUnexpected(GatewayErrorc::kUnknownServiceInstance);
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        element_type == impl::ServiceElementType::EVENT,
        "GatewayApplication: UnregisterUpdateNotification only supports events!");

    auto event_map = it->second.GetEvents();
    auto event_it = event_map.find(element_name);
    if (event_it == event_map.cend())
    {
        return MakeUnexpected(GatewayErrorc::kUnknownServiceElement);
    }

    if (!event_it->second.UnsetReceiveHandler().has_value())
    {
        score::mw::log::LogWarn() << "GatewayApplication: Failed to unset receive handler for event " << element_name
                                  << " of " << specifier_str;
    }
    event_it->second.Unsubscribe();
    return {};
}

score::Result<void> GatewayApplication::NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                                     impl::ServiceElementType updated_element_type,
                                                     std::string updated_element_name)
{
    auto specifier_str = std::string(service_instance_specifier.ToString());

    auto it = skeletons_.find(specifier_str);
    if (it == skeletons_.end())
    {
        score::mw::log::LogWarn() << "GatewayApplication: No skeleton found for update notification " << specifier_str;
        return MakeUnexpected(GatewayErrorc::kUnknownServiceInstance);
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(updated_element_type == impl::ServiceElementType::EVENT,
                                                "GatewayApplication: NotifyUpdate only supports events!");

    auto event_map = it->second.GetEvents();
    auto event_it = event_map.find(updated_element_name);
    if (event_it == event_map.cend())
    {
        score::mw::log::LogWarn() << "GatewayApplication: Event " << updated_element_name
                                  << " not found in skeleton for update notification";
        return MakeUnexpected(GatewayErrorc::kUnknownServiceElement);
    }

    event_it->second.Notify();
    return {};
}

}  // namespace score::mw::com::gateway
