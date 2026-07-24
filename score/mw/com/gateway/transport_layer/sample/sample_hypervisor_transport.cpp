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
#include "score/mw/com/gateway/transport_layer/sample/sample_hypervisor_transport.h"

#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/log/logging.h"

#include <cstdint>
#include <string>

namespace score::mw::com::gateway
{

ShmPaths ResolveShmPaths(const impl::InstanceSpecifier& specifier)
{
    auto& runtime = impl::Runtime::getInstance();
    auto identifiers = runtime.resolve(specifier);
    if (identifiers.empty())
    {
        return {};
    }
    impl::InstanceIdentifierView view{identifiers.front()};
    const auto* lola_type =
        std::get_if<impl::LolaServiceTypeDeployment>(&view.GetServiceTypeDeployment().binding_info_);
    const auto* lola_instance =
        std::get_if<impl::LolaServiceInstanceDeployment>(&view.GetServiceInstanceDeployment().bindingInfo_);

    if (lola_type == nullptr || lola_instance == nullptr || !lola_instance->instance_id_.has_value())
    {
        return {};
    }
    // TODO Implement a ShmPathBuilder for your specific hypervisor shared memory technology that provides the according
    // path names.
    // LCOV_EXCL_START This code is not yet implemented so its not yet covered by tests
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        false, "Missing implementation of ShmPathBuilder for LoLa Hypervisor gateway.");
    // LCOV_EXCL_STOP
}

ShmSizes GetShmSizes(const impl::InstanceSpecifier& specifier)
{
    auto paths = ResolveShmPaths(specifier);
    if (paths.control.empty())
    {
        ::score::mw::log::LogError() << "GetShmSizes: failed to resolve SHM paths for " << specifier.ToString();
        return {0U, 0U};
    }

    // LCOV_EXCL_START This code is not yet implemented so its not yet covered by tests
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        false, "Missing specific implementation of LoLa hypervisor gateway SHM size calculation.");
    // TODO Add an implementation that is opening the SHM paths and calculates the size of those segments. This sample
    // implementation does not include this implementation, as it is highly specific to the used hypervisor shared
    // memory technology.
    // LCOV_EXCL_STOP
}

SampleHyperVisorTransport::SampleHyperVisorTransport(GatewayCore& gateway_app,
                                                     std::unique_ptr<IBidirectionalTransport> transport) noexcept
    : gateway_app_{gateway_app}, message_transport_{std::move(transport)}
{
}

SampleHyperVisorTransport::~SampleHyperVisorTransport()
{
    Shutdown();
}

bool SampleHyperVisorTransport::IsMemorySharingSupported() const
{
    return true;
}

score::ResultBlank SampleHyperVisorTransport::Setup()
{
    message_transport_->SetMessageHandler([this](std::unique_ptr<TransportMessage> message) {
        OnMessageReceived(std::move(message));
    });
    return message_transport_->Setup();
}

void SampleHyperVisorTransport::OnMessageReceived(std::unique_ptr<TransportMessage> message)
{
    if (!message)
    {
        log::LogError("LoLa") << "SampleTransport: Invalid message received: nullptr";
        return;
    }

    if (message->GetType() == MessageType::kProvideServiceRequest)
    {
        HandleProvideServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kStopOfferServiceRequest)
    {
        HandleStopOfferServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kOfferServiceRequest)
    {
        HandleOfferServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kRegisterNotificationRequest)
    {
        HandleRegisterNotificationRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kUnregisterNotificationRequest)
    {
        HandleUnregisterNotificationRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kUpdateNotification)
    {
        HandleUpdateNotification(std::move(message));
    }
    else
    {
        log::LogError("LoLa") << "SampleTransport: Unexpected TransportMessage received: "
                              << static_cast<int>(message->GetType());
    }
}

void SampleHyperVisorTransport::HandleProvideServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto& request = dynamic_cast<ProvideServiceRequest&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in ProvideServiceRequest!";
        return;
    }
    PreCreateInterVmSharedMemory(specifier_result.value(), request.GetShmControlSize(), request.GetShmDataSize());
    gateway_app_.ProvideService(specifier_result.value(), request.GetServiceElements());
}

void SampleHyperVisorTransport::HandleStopOfferServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto& request = dynamic_cast<StopOfferServiceRequest&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in StopOfferServiceRequest!";
        return;
    }
    gateway_app_.StopOfferService(specifier_result.value());
}

void SampleHyperVisorTransport::HandleOfferServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto& request = dynamic_cast<OfferServiceRequest&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in OfferServiceRequest!";
        return;
    }
    gateway_app_.OfferService(specifier_result.value());
}

void SampleHyperVisorTransport::HandleUpdateNotification(std::unique_ptr<TransportMessage> message)
{
    auto& notification = dynamic_cast<UpdateNotification&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{notification.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in UpdateNotification!";
        return;
    }
    gateway_app_.NotifyUpdate(specifier_result.value(), notification.GetElementType(), notification.GetElementName());
}

void SampleHyperVisorTransport::HandleRegisterNotificationRequest(std::unique_ptr<TransportMessage> message)
{
    auto& request = dynamic_cast<RegisterNotificationRequest&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in RegisterNotificationRequest!";
        return;
    }
    gateway_app_.RegisterUpdateNotification(
        specifier_result.value(), request.GetElementType(), request.GetElementName());
}

void SampleHyperVisorTransport::HandleUnregisterNotificationRequest(std::unique_ptr<TransportMessage> message)
{
    auto& request = dynamic_cast<UnregisterNotificationRequest&>(*message);
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "SampleTransport: Invalid instance specifier in UnregisterNotificationRequest!";
        return;
    }
    gateway_app_.UnregisterUpdateNotification(
        specifier_result.value(), request.GetElementType(), request.GetElementName());
}

void SampleHyperVisorTransport::Shutdown()
{
    message_transport_->Shutdown();
}

score::ResultBlank SampleHyperVisorTransport::ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                                             std::vector<impl::EventInfo> service_elements)
{
    const auto shm_sizes = GetShmSizes(service_instance_specifier);

    ProvideServiceRequest request{
        std::move(service_instance_specifier), std::move(service_elements), shm_sizes.control, shm_sizes.data};
    return message_transport_->SendRequest(request);
}

score::ResultBlank SampleHyperVisorTransport::OfferService(impl::InstanceSpecifier service_instance_specifier)
{
    OfferServiceRequest request{std::move(service_instance_specifier)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank SampleHyperVisorTransport::StopOfferService(impl::InstanceSpecifier service_instance_specifier)
{
    StopOfferServiceRequest request{std::move(service_instance_specifier)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank SampleHyperVisorTransport::NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                                           impl::ServiceElementType updated_element_type,
                                                           std::string updated_element_name)
{
    UpdateNotification notification{
        std::move(service_instance_specifier), updated_element_type, std::move(updated_element_name)};
    return message_transport_->SendNotification(notification);
}

score::ResultBlank SampleHyperVisorTransport::RegisterUpdateNotification(
    impl::InstanceSpecifier service_instance_specifier,
    impl::ServiceElementType element_type,
    std::string element_name)
{
    RegisterNotificationRequest request{std::move(service_instance_specifier), element_type, std::move(element_name)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank SampleHyperVisorTransport::UnregisterUpdateNotification(
    impl::InstanceSpecifier service_instance_specifier,
    impl::ServiceElementType element_type,
    std::string element_name)
{
    UnregisterNotificationRequest request{std::move(service_instance_specifier), element_type, std::move(element_name)};
    return message_transport_->SendRequest(request);
}

void SampleHyperVisorTransport::PreCreateInterVmSharedMemory(const impl::InstanceSpecifier& specifier,
                                                             std::uint32_t /* shm_control_size */,
                                                             std::uint32_t /* shm_data_size */)
{
    auto paths = ResolveShmPaths(specifier);
    if (paths.control.empty())
    {
        ::score::mw::log::LogError() << "PreCreateInterVmSharedMemory: failed to resolve SHM paths for "
                                     << specifier.ToString();
        return;
    }
    // LCOV_EXCL_START This code is not yet implemented so it is not yet covered by tests
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        false, "Missing specific implementation of hypervisor SHM memory access.");
    // TODO Add an implementation that is opening the SHM paths in a hypervisor gateway compatible way. This sample
    // implementation does not include this implementation, as it is highly specific to the used hypervisor shared
    // memory technology.
    // LCOV_EXCL_STOP
}

}  // namespace score::mw::com::gateway
