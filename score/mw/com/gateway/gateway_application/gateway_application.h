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
#ifndef SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_H
#define SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_H

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/language/safecpp/scoped_function/scope.h"
#include "score/mw/com/gateway/gateway_application/configuration/gateway_configuration.h"
#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/transport.h"
#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace score::mw::com::gateway
{

class GatewayApplication : public GatewayCore
{
  public:
    explicit GatewayApplication(GatewayConfiguration app_configuration) noexcept
        : app_configuration_{std::move(app_configuration)}
    {
    }

    /// \brief Constructor for testing — injects a pre-built transport, skipping TransportFactory::Create in Setup().
    explicit GatewayApplication(GatewayConfiguration app_configuration, std::unique_ptr<Transport> transport) noexcept
        : app_configuration_{std::move(app_configuration)}, transport_layer_{std::move(transport)}
    {
    }

    ~GatewayApplication() override;

    score::Result<void> Setup();

    score::Result<void> Start();

    score::Result<void> ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                       std::vector<impl::EventInfo> service_elements) override;
    void StopOfferService(impl::InstanceSpecifier service_instance_specifier) override;
    score::Result<void> OfferService(impl::InstanceSpecifier service_instance_specifier) override;
    score::Result<void> RegisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                   impl::ServiceElementType element_type,
                                                   std::string element_name) override;
    score::Result<void> UnregisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                     impl::ServiceElementType element_type,
                                                     std::string element_name) override;
    score::Result<void> NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                     impl::ServiceElementType updated_element_type,
                                     std::string updated_element_name) override;

  private:
    using FindCallbackScopedCb =
        safecpp::MoveOnlyScopedFunction<void(impl::ServiceHandleContainer<impl::HandleType>, impl::FindServiceHandle)>;

    GatewayConfiguration app_configuration_;
    std::unique_ptr<Transport> transport_layer_;

    std::recursive_mutex mutex_;
    std::unordered_map<std::string, impl::GenericProxy> proxies_;
    std::unordered_map<std::string, impl::GenericSkeleton> skeletons_;
    std::vector<impl::FindServiceHandle> find_handles_;
    std::unordered_map<std::string, std::unordered_set<std::string>> active_event_subscriptions_;

    /// \brief Starts asynchronous service discovery for all forwarded services.
    /// \details Uses StartFindService to continuously monitor for services.
    /// When a service appears, automatically creates a GenericProxy and propagates it via transport.
    score::Result<void> StartServiceDiscovery();

    /// \brief Checks if the service instance of the given specifier is accepted to be propagated by this gateway
    /// application based on the configuration.
    /// \return true if the service instance is accepted, false otherwise.
    bool IsServiceInstanceAccepted(std::string_view service_instance_str) const;

    void OnServiceAvailabilityChanged(const std::string& specifier_str, std::vector<impl::HandleType> handles);
    void OnServiceUnavailable(const std::string& specifier_str);
    void PropagateService(const std::string& specifier_str);

    void RegisterEventReceiveHandlerCallback(impl::GenericSkeleton& skeleton,
                                             const std::string& specifier_str,
                                             const std::string& event_name);
    void OnSubscriptionStateChanged(const std::string& specifier_str,
                                    const std::string& event_name,
                                    bool has_subscribers);
    void ReRegisterActiveEventSubscriptions(const std::string& specifier_str);

    // Intentionally putting the scope as the last member, so that it gets destroyed first and thus all callbacks get
    // invalidated before any other member gets destroyed.
    safecpp::Scope<> scope_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_H
