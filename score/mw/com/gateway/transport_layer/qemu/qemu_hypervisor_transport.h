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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_HYPERVISOR_TRANSPORT_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_HYPERVISOR_TRANSPORT_H_

#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"
#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/sample_hypervisor_transport.h"
#include "score/mw/com/gateway/transport_layer/transport.h"

#include <functional>
#include <memory>

namespace score::mw::com::gateway
{

/// Resolves the inter-VM shm paths (with /intervm-shared-shmem/ prefix) for the given
/// service instance specifier so that lib-memory routes them through the InterVM provider.
ShmPaths ResolveInterVmShmPaths(const impl::InstanceSpecifier& specifier);

/// Opens the existing shm objects and returns their sizes.
#if defined(__QNXNTO__)
ShmSizes GetInterVmShmSizes(const impl::InstanceSpecifier& specifier, const score::os::qnx::MmanQnx* mman_qnx);
#else
ShmSizes GetInterVmShmSizes(const impl::InstanceSpecifier& specifier);
#endif

/// QEMU/ivshmem-based transport layer for the LoLa gateway.
///
/// Reuses the message-communication layer from ``sample`` (BidirectionalTransport over TCP
/// sockets on the QEMU intervm NIC) and implements the memory-sharing part via ivshmem:
/// both VMs map the same ivshmem BAR through the IvshmemTypedMemoryProvider, so shm objects
/// with the inter-VM prefix land on the shared physical BAR.
class QemuHypervisorTransport : public Transport
{
  public:
    /// Callable type used to resolve SHM paths for a given instance specifier.
    /// Defaults to ResolveInterVmShmPaths; injectable for testing.
    using ShmPathResolver = std::function<ShmPaths(const impl::InstanceSpecifier&)>;

    QemuHypervisorTransport(GatewayCore& gateway_app,
                            std::unique_ptr<IBidirectionalTransport> transport,
                            std::shared_ptr<qemu::ivshmem::IvshmemTypedMemoryProvider> ivshmem_provider,
                            ShmPathResolver path_resolver = ResolveInterVmShmPaths) noexcept;
    ~QemuHypervisorTransport() override;

    QemuHypervisorTransport(const QemuHypervisorTransport&) = delete;
    QemuHypervisorTransport& operator=(const QemuHypervisorTransport&) = delete;
    QemuHypervisorTransport(QemuHypervisorTransport&&) = delete;
    QemuHypervisorTransport& operator=(QemuHypervisorTransport&&) = delete;

    bool IsMemorySharingSupported() const override;

    Result<void> Setup() override;
    void Shutdown() override;

    Result<void> ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                std::vector<impl::EventInfo> service_elements) override;
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
    void HandleProvideServiceRequest(std::unique_ptr<TransportMessage> message);
    void HandleStopOfferServiceRequest(std::unique_ptr<TransportMessage> message);
    void HandleOfferServiceRequest(std::unique_ptr<TransportMessage> message);
    void HandleUpdateNotification(std::unique_ptr<TransportMessage> message);
    void HandleRegisterNotificationRequest(std::unique_ptr<TransportMessage> message);
    void HandleUnregisterNotificationRequest(std::unique_ptr<TransportMessage> message);
    void OnMessageReceived(std::unique_ptr<TransportMessage> message);

    /// Binds the inter-VM shm paths to the ivshmem BAR on the destination side.
    /// Looks up the source-side BAR offsets from the BAR-resident directory so that
    /// both VMs map the same physical sub-range regardless of local allocation order.
    void PreCreateInterVmSharedMemory(const impl::InstanceSpecifier& specifier,
                                      std::uint32_t shm_control_size,
                                      std::uint32_t shm_data_size);

    GatewayCore& gateway_app_;
    std::unique_ptr<IBidirectionalTransport> message_transport_;
    std::shared_ptr<qemu::ivshmem::IvshmemTypedMemoryProvider> ivshmem_provider_;
    ShmPathResolver path_resolver_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_HYPERVISOR_TRANSPORT_H_
