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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_HYPERVISOR_TRANSPORT_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_HYPERVISOR_TRANSPORT_H_

#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/transport.h"

#include <memory>

namespace score::mw::com::gateway
{

struct ShmPaths
{
    std::string control;
    std::string data;
};

struct ShmSizes
{
    std::uint32_t control;
    std::uint32_t data;
};

ShmPaths ResolveShmPaths(const impl::InstanceSpecifier& specifier);

ShmSizes GetShmSizes(const impl::InstanceSpecifier& specifier);

class SampleHyperVisorTransport : public Transport
{
  public:
    SampleHyperVisorTransport(GatewayCore& gateway_app, std::unique_ptr<IBidirectionalTransport> transport) noexcept;
    ~SampleHyperVisorTransport() override;

    SampleHyperVisorTransport(const SampleHyperVisorTransport&) = delete;
    SampleHyperVisorTransport& operator=(const SampleHyperVisorTransport&) = delete;
    SampleHyperVisorTransport(SampleHyperVisorTransport&&) = delete;
    SampleHyperVisorTransport& operator=(SampleHyperVisorTransport&&) = delete;

    bool IsMemorySharingSupported() const override;

    Result<void> Setup() override;
    void Shutdown() override;

    // Methods to create the related TransportMessage and call SendMessage of BidirectionalTransport

    Result<void> ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                std::vector<ServiceElementConfiguration> service_elements) override;
    Result<void> OfferService(impl::InstanceSpecifier service_instance_specifier) override;
    Result<void> StopOfferService(impl::InstanceSpecifier service_instance_specifier) override;

    Result<void> NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                              impl::ServiceElementType updated_element_type,
                              std::string updated_element_name) override;
    Result<void> RegisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                            impl::ServiceElementType element_type,
                                            std::string element_name) override;
    Result<void> UnregisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                              impl::ServiceElementType element_type,
                                              std::string element_name) override;

  private:
    /// \brief Internal callback for incoming messages from the transport.
    /// \details inspects the received message and dispatches it to related API calls of the gateway application
    /// (e.g. ProvideService, NotifyUpdate, etc.). In some cases it does a message specific processing before calling
    /// the gateway application API (e.g. for ProvideServiceRequest, it creates the shared memory segments for the
    /// service instance before calling ProvideService).
    /// @param message received message from the BidirectionalTransport (transport_ member variable)
    void OnMessageReceived(std::unique_ptr<TransportMessage> message);
    /// \brief Creates the shared memory region based on the given arguments, prior to the gateway application accessing
    /// the shared memory region, i.e. before ProvideService() has been called.
    void PreCreateInterVmSharedMemory(const impl::InstanceSpecifier& specifier,
                                      std::uint32_t shm_control_size,
                                      std::uint32_t shm_data_size);

    GatewayCore& gateway_app_;
    std::unique_ptr<IBidirectionalTransport> transport_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_HYPERVISOR_TRANSPORT_H_
