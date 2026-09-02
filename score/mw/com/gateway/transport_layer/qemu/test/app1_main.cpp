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
/// Gateway transport layer integration test — VM-A (bidirectional).
///
/// VM-A creates CTRL+DATA shm for "service_a" and VM-B creates it for "service_b"; each VM
/// makes the peer's shm visible via the transport, then reads and verifies it.
///
/// Shared memory (the ivshmem BAR) carries the data plane only (DATA + CTRL's event_count,
/// matching production LoLa's ServiceDataControl). ivshmem-plain has no MSI-X/doorbell, so
/// cross-VM signaling ("data ready", "peer verified") travels over the real transport via
/// QemuHypervisorTransport::NotifyUpdate() instead of polling shared memory.
///
/// Both VMs run QemuHypervisorTransport, each acting as source (own service) and destination
/// (peer's service) simultaneously.

#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"
#include "score/mw/com/gateway/transport_layer/qemu/qemu_hypervisor_transport.h"
#include "score/mw/com/gateway/transport_layer/qemu/test/qemu_integration_test_helpers.h"
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/hypervisor_socket_configuration.h"
#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/result/result.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

using score::mw::com::gateway::BidirectionalTransport;
using score::mw::com::gateway::HyperVisorSocketConfiguration;
using score::mw::com::gateway::QemuHypervisorTransport;
using score::mw::com::gateway::ResolveInterVmShmPaths;
using score::mw::com::gateway::qemu::ivshmem::DiscoverIvshmemBar;
using score::mw::com::gateway::qemu::ivshmem::IvshmemTypedMemoryProvider;
using score::mw::com::impl::InstanceSpecifier;

namespace
{

// Static IPs on the "intervm" NIC (vtnet1) bridging the two VMs; see ConfigureIntervmNic().
constexpr char kIntervmIpVmA[] = "10.0.3.1";
constexpr char kIntervmIpVmB[] = "10.0.3.2";  // this VM's transport peer
constexpr std::uint16_t kTransportPortVmA = 46001U;
constexpr std::uint16_t kTransportPortVmB = 46002U;

HyperVisorSocketConfiguration CreateConfiguration()
{
    HyperVisorSocketConfiguration config{};
    config.remote_ip_ = score::os::Ipv4Address{kIntervmIpVmB};
    config.local_port_ = kTransportPortVmA;
    config.remote_port_ = kTransportPortVmB;
    return config;
}

}  // namespace

int main()
{
    std::fprintf(stderr, "=== app1 (VM-A): bidirectional gateway transport test ===\n");

    // Bring up the intervm NIC before BidirectionalTransport needs it for listen()/connect().
    if (!ConfigureIntervmNic(kIntervmIpVmA))
    {
        std::fprintf(stderr, "app1: failed to configure intervm NIC\n");
        return 1;
    }

    // --- Setup: discover BAR, create provider, create transport ---
    std::uint64_t paddr = 0U;
    std::uint64_t size = 0U;
    if (!DiscoverIvshmemBar(paddr, size))
    {
        std::fprintf(stderr, "app1: failed to discover ivshmem BAR\n");
        return 1;
    }
    std::fprintf(stderr,
                 "app1: BAR paddr=0x%llx size=0x%llx\n",
                 static_cast<unsigned long long>(paddr),
                 static_cast<unsigned long long>(size));

    // The entire BAR is usable for shm allocations (no reserved handshake page needed).
    auto provider = std::make_shared<IvshmemTypedMemoryProvider>(paddr, size);
    score::memory::shared::SharedMemoryFactory::SetInterVMMemoryProvider(provider);

    TestGatewayCore gateway_core;
    auto message_transport = std::make_unique<BidirectionalTransport>(CreateConfiguration());
    QemuHypervisorTransport qemu_transport{gateway_core, std::move(message_transport), provider};
    if (!qemu_transport.Setup().has_value())
    {
        std::fprintf(stderr, "app1: QemuHypervisorTransport::Setup failed\n");
        return 1;
    }

    // ========================================================================
    // SOURCE SIDE: Create CTRL + DATA shm for service_a, matching the LoLa skeleton pattern.
    // ========================================================================
    auto spec_a = InstanceSpecifier::Create(std::string{kServiceA});
    if (!spec_a.has_value())
    {
        std::fprintf(stderr, "app1: failed to create InstanceSpecifier for service_a\n");
        return 1;
    }
    const auto paths_a = ResolveInterVmShmPaths(spec_a.value());

    // Create CTRL shm for service_a — holds the ServiceControl signaling structure.
    auto ctrl_a = score::memory::shared::SharedMemoryFactory::Create(
        paths_a.control,
        [](std::shared_ptr<score::memory::shared::ISharedMemoryResource> /*res*/) {},
        sizeof(ServiceControl),
        score::memory::shared::SharedMemoryFactory::WorldWritable{});
    if (ctrl_a == nullptr)
    {
        std::fprintf(stderr, "app1: SharedMemoryFactory::Create for service_a CTRL failed\n");
        return 1;
    }

    // Create DATA shm for service_a — holds the actual payload.
    auto data_a = score::memory::shared::SharedMemoryFactory::Create(
        paths_a.data,
        [](std::shared_ptr<score::memory::shared::ISharedMemoryResource> /*res*/) {},
        kShmSize,
        score::memory::shared::SharedMemoryFactory::WorldWritable{});
    if (data_a == nullptr)
    {
        std::fprintf(stderr, "app1: SharedMemoryFactory::Create for service_a DATA failed\n");
        return 1;
    }

    // Initialize CTRL to zero (no events yet).
    auto* ctrl_a_ptr = static_cast<ServiceControl*>(ctrl_a->getUsableBaseAddress());
    ctrl_a_ptr->event_count = 0U;

    // Write payload into DATA shm.
    auto* write_data = static_cast<std::uint32_t*>(data_a->getUsableBaseAddress());
    write_data[0] = kMagicA;
    write_data[1] = 100U;
    write_data[2] = 200U;

    // Signal readiness via CTRL shm — like the skeleton updating EventDataControl.
    std::atomic_thread_fence(std::memory_order_release);
    ctrl_a_ptr->event_count = 1U;
    std::fprintf(stderr, "app1: wrote service_a [magic=0x%08x, 100, 200] and signalled via CTRL\n", kMagicA);

    // Notify VM-B over the real transport that service_a is available. Waits for the ACK
    // (BidirectionalTransport retries internally), not for VM-B to have processed it.
    const auto provide_result =
        qemu_transport.ProvideService(spec_a.value(), std::vector<score::mw::com::impl::EventInfo>{});
    if (!provide_result.has_value())
    {
        std::fprintf(stderr, "app1: ProvideService for service_a failed to send\n");
        return 1;
    }
    std::fprintf(stderr, "app1: sent ProvideServiceRequest for service_a to VM-B\n");

    // Notify VM-B (over the transport, not shared memory) that service_a's DATA is ready.
    // Both messages share one TCP connection with FIFO dispatch on the receiver, so this is
    // guaranteed to be handled after the ProvideServiceRequest above (shm already bound).
    const auto notify_result = qemu_transport.NotifyUpdate(
        spec_a.value(), score::mw::com::impl::ServiceElementType::EVENT, kElementNameDataReady);
    if (!notify_result.has_value())
    {
        std::fprintf(stderr, "app1: DataReady notification for service_a failed to send\n");
        return 1;
    }
    std::fprintf(stderr, "app1: sent DataReady notification for service_a to VM-B\n");

    // ========================================================================
    // DESTINATION SIDE: Wait for service_b's data-ready notification, then read.
    // The transport must first make service_b's CTRL+DATA visible on this VM.
    // ========================================================================
    auto spec_b = InstanceSpecifier::Create(std::string{kServiceB});
    if (!spec_b.has_value())
    {
        std::fprintf(stderr, "app1: failed to create InstanceSpecifier for service_b\n");
        return 1;
    }
    const auto paths_b = ResolveInterVmShmPaths(spec_b.value());

    // Wait for VM-B's ProvideServiceRequest for service_b. On arrival, OnMessageReceived binds
    // service_b's CTRL+DATA to this VM's shm before calling GatewayCore::ProvideService.
    std::fprintf(stderr, "app1: waiting for ProvideServiceRequest for service_b from VM-B...\n");
    if (!WaitForFlag(gateway_core.provide_service_called))
    {
        std::fprintf(stderr, "app1: transport did not call GatewayCore::ProvideService for service_b\n");
        return 1;
    }
    std::fprintf(stderr, "app1: transport made service_b CTRL+DATA visible on this VM\n");

    // Wait for VM-B's "DataReady" notification instead of polling service_b's CTRL shm.
    std::fprintf(stderr, "app1: waiting for DataReady notification for service_b from VM-B...\n");
    if (!WaitForFlag(gateway_core.data_ready_notified))
    {
        std::fprintf(stderr, "app1: timed out waiting for DataReady notification for service_b\n");
        return 1;
    }
    std::fprintf(stderr, "app1: DataReady notification for service_b received\n");

    // Sanity-check event_count (FIFO ordering guarantees it's already set by this point) —
    // the notification above is the actual synchronization signal, not this check.
    auto ctrl_b = score::memory::shared::SharedMemoryFactory::Open(paths_b.control, /*is_read_write=*/true);
    if (ctrl_b == nullptr)
    {
        std::fprintf(stderr, "app1: Open for service_b CTRL failed\n");
        return 1;
    }
    auto* ctrl_b_ptr = static_cast<ServiceControl*>(ctrl_b->getUsableBaseAddress());
    std::atomic_thread_fence(std::memory_order_acquire);
    if (ctrl_b_ptr->event_count < 1U)
    {
        std::fprintf(stderr,
                     "app1: service_b event_count not signalled despite DataReady notification (value=%u)\n",
                     ctrl_b_ptr->event_count);
        return 1;
    }

    // Open service_b's DATA shm and verify the payload.
    auto data_b = score::memory::shared::SharedMemoryFactory::Open(paths_b.data, /*is_read_write=*/false);
    if (data_b == nullptr)
    {
        std::fprintf(stderr, "app1: Open for service_b DATA failed\n");
        return 1;
    }

    const auto* read_data = static_cast<const std::uint32_t*>(data_b->getUsableBaseAddress());
    if (read_data[0] != kMagicB || read_data[1] != 300U || read_data[2] != 400U)
    {
        std::fprintf(stderr,
                     "app1: service_b verification FAILED (magic=0x%08x d[1]=%u d[2]=%u)\n",
                     read_data[0],
                     read_data[1],
                     read_data[2]);
        return 1;
    }
    std::fprintf(stderr, "app1: service_b verified [magic=0x%08x, 300, 400] — read from VM-B OK\n", kMagicB);

    // Confirm back to VM-B over the transport that we verified its data.
    const auto verified_result = qemu_transport.NotifyUpdate(
        spec_b.value(), score::mw::com::impl::ServiceElementType::EVENT, kElementNameVerified);
    if (!verified_result.has_value())
    {
        std::fprintf(stderr, "app1: Verified notification for service_b failed to send\n");
        return 1;
    }
    std::fprintf(stderr, "app1: sent Verified notification for service_b to VM-B\n");

    // Wait for VM-B to confirm it verified our data.
    std::fprintf(stderr, "app1: waiting for VM-B to verify service_a...\n");
    if (!WaitForFlag(gateway_core.verified_notified))
    {
        std::fprintf(stderr, "app1: timed out waiting for VM-B's Verified notification for service_a\n");
        return 1;
    }
    std::fprintf(stderr, "app1: VM-B verified service_a successfully!\n");

    std::fprintf(stderr, "app1: both directions verified successfully!\n");
    std::printf("verified\n");
    return 0;
}
