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
/// matching production LoLa's ServiceDataControl). Cross-VM signaling ("data ready", "peer verified")
/// travels over the real transport via QemuHypervisorTransport::NotifyUpdate().
///
/// Both VMs run QemuHypervisorTransport, each acting as source (own service) and destination
/// (peer's service) simultaneously.

#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"
#include "score/mw/com/gateway/transport_layer/qemu/qemu_hypervisor_transport.h"
#include "score/mw/com/gateway/transport_layer/qemu/test/qemu_integration_test_helpers.h"
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/hypervisor_socket_configuration.h"

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/result/result.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>

using score::mw::com::InstanceSpecifier;
using score::mw::com::gateway::BidirectionalTransport;
using score::mw::com::gateway::HyperVisorSocketConfiguration;
using score::mw::com::gateway::QemuHypervisorTransport;
using score::mw::com::gateway::ResolveInterVmShmPaths;
using score::mw::com::gateway::qemu::ivshmem::DiscoverIvshmemBar;
using score::mw::com::gateway::qemu::ivshmem::IvshmemTypedMemoryProvider;

namespace
{

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
    std::cerr << "=== app1 (VM-A): bidirectional gateway transport test ===\n";

    // --- Setup: discover BAR, create provider, create transport ---
    std::uint64_t paddr = 0U;
    std::uint64_t size = 0U;
    if (!DiscoverIvshmemBar(paddr, size))
    {
        std::cerr << "app1: failed to discover ivshmem BAR\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: BAR paddr=0x" << std::hex << paddr << " size=0x" << size << std::dec << '\n';

    // The entire BAR is usable for shm allocations (no reserved handshake page needed).
    auto provider = std::make_shared<IvshmemTypedMemoryProvider>(paddr, size);
    score::memory::shared::SharedMemoryFactory::SetInterVMMemoryProvider(provider);

    TestGatewayCore gateway_core;
    auto message_transport = std::make_unique<BidirectionalTransport>(CreateConfiguration());
    QemuHypervisorTransport qemu_transport{gateway_core, std::move(message_transport), provider};
    std::cerr << "app1: starting transport setup\n";
    if (!qemu_transport.Setup().has_value())
    {
        std::cerr << "app1: QemuHypervisorTransport::Setup failed\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: transport setup complete\n";

    // ========================================================================
    // SOURCE SIDE: Create CTRL + DATA shm for service_a, matching the LoLa skeleton pattern.
    // ========================================================================
    auto spec_a = InstanceSpecifier::Create(std::string{kServiceA});
    if (!spec_a.has_value())
    {
        std::cerr << "app1: failed to create InstanceSpecifier for service_a\n";
        return EXIT_FAILURE;
    }
    const auto paths_a = ResolveInterVmShmPaths(spec_a.value());

    // Create CTRL shm for service_a — holds the ServiceControl signaling structure.
    std::cerr << "app1: creating service_a CTRL shm\n";
    auto ctrl_a = score::memory::shared::SharedMemoryFactory::Create(
        paths_a.control,
        [](std::shared_ptr<score::memory::shared::ISharedMemoryResource> /*res*/) {},
        sizeof(ServiceControl),
        score::memory::shared::SharedMemoryFactory::WorldWritable{});
    if (ctrl_a == nullptr)
    {
        std::cerr << "app1: SharedMemoryFactory::Create for service_a CTRL failed\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: service_a CTRL shm created\n";

    // Create DATA shm for service_a — holds the actual payload.
    auto data_a = score::memory::shared::SharedMemoryFactory::Create(
        paths_a.data,
        [](std::shared_ptr<score::memory::shared::ISharedMemoryResource> /*res*/) {},
        kShmSize,
        score::memory::shared::SharedMemoryFactory::WorldWritable{});
    if (data_a == nullptr)
    {
        std::cerr << "app1: SharedMemoryFactory::Create for service_a DATA failed\n";
        return EXIT_FAILURE;
    }

    // Initialize CTRL to zero (no events yet).
    auto* ctrl_a_ptr = static_cast<ServiceControl*>(ctrl_a->getUsableBaseAddress());
    ctrl_a_ptr->event_count.store(0U, std::memory_order_relaxed);

    // Write payload into DATA shm.
    auto* write_data = static_cast<ServicePayload*>(data_a->getUsableBaseAddress());
    write_data->magic = kMagicA;
    write_data->value_one = kValueOneFromVmA;
    write_data->value_two = kValueTwoFromVmA;

    // Release-store publishes the payload writes above to whoever acquires event_count.
    ctrl_a_ptr->event_count.store(kDataReadyEventCount, std::memory_order_release);
    std::cerr << "app1: wrote service_a [magic=0x" << std::hex << std::setw(8) << std::setfill('0') << kMagicA
              << std::dec << ", " << kValueOneFromVmA << ", " << kValueTwoFromVmA << "] and signalled via CTRL\n";

    // Notify VM-B over the real transport that service_a is available. Waits for the ACK
    // (BidirectionalTransport retries internally), not for VM-B to have processed it.
    const auto provide_result = qemu_transport.ProvideService(spec_a.value(), std::vector<score::mw::com::EventInfo>{});
    if (!provide_result.has_value())
    {
        std::cerr << "app1: ProvideService for service_a failed to send\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: sent ProvideServiceRequest for service_a to VM-B\n";

    // Notify VM-B (over the transport, not shared memory) that service_a's DATA is ready.
    // Both messages share one TCP connection with FIFO dispatch on the receiver, so this is
    // guaranteed to be handled after the ProvideServiceRequest above (shm already bound).
    const auto notify_result =
        qemu_transport.NotifyUpdate(spec_a.value(), ServiceElementType::EVENT, kElementNameDataReady);
    if (!notify_result.has_value())
    {
        std::cerr << "app1: DataReady notification for service_a failed to send\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: sent DataReady notification for service_a to VM-B\n";

    // ========================================================================
    // DESTINATION SIDE: Wait for service_b's data-ready notification, then read.
    // The transport must first make service_b's CTRL+DATA visible on this VM.
    // ========================================================================
    auto spec_b = InstanceSpecifier::Create(std::string{kServiceB});
    if (!spec_b.has_value())
    {
        std::cerr << "app1: failed to create InstanceSpecifier for service_b\n";
        return EXIT_FAILURE;
    }
    const auto paths_b = ResolveInterVmShmPaths(spec_b.value());

    // Wait for VM-B's ProvideServiceRequest for service_b. On arrival, OnMessageReceived binds
    // service_b's CTRL+DATA to this VM's shm before calling GatewayCore::ProvideService.
    std::cerr << "app1: waiting for ProvideServiceRequest for service_b from VM-B...\n";
    if (!WaitForFlag(gateway_core.provide_service_b_called))
    {
        std::cerr << "app1: transport did not call GatewayCore::ProvideService for service_b\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: transport made service_b CTRL+DATA visible on this VM\n";

    // Wait for VM-B's "DataReady" notification instead of polling service_b's CTRL shm.
    std::cerr << "app1: waiting for DataReady notification for service_b from VM-B...\n";
    if (!WaitForFlag(gateway_core.data_ready_service_b_notified))
    {
        std::cerr << "app1: timed out waiting for DataReady notification for service_b\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: DataReady notification for service_b received\n";

    // Sanity-check event_count (FIFO ordering guarantees it's already set by this point) —
    // the notification above is the actual synchronization signal, not this check.
    auto ctrl_b = score::memory::shared::SharedMemoryFactory::Open(paths_b.control, /*is_read_write=*/true);
    if (ctrl_b == nullptr)
    {
        std::cerr << "app1: Open for service_b CTRL failed\n";
        return EXIT_FAILURE;
    }
    const auto* ctrl_b_ptr = static_cast<const ServiceControl*>(ctrl_b->getUsableBaseAddress());
    const auto event_count = ctrl_b_ptr->event_count.load(std::memory_order_acquire);
    if (event_count < kDataReadyEventCount)
    {
        std::cerr << "app1: service_b event_count not signalled despite DataReady notification (value=" << event_count
                  << ")\n";
        return EXIT_FAILURE;
    }

    // Open service_b's DATA shm and verify the payload.
    auto data_b = score::memory::shared::SharedMemoryFactory::Open(paths_b.data, /*is_read_write=*/false);
    if (data_b == nullptr)
    {
        std::cerr << "app1: Open for service_b DATA failed\n";
        return EXIT_FAILURE;
    }

    const auto* read_data = static_cast<const ServicePayload*>(data_b->getUsableBaseAddress());
    if (read_data->magic != kMagicB || read_data->value_one != kValueOneFromVmB ||
        read_data->value_two != kValueTwoFromVmB)
    {
        std::cerr << "app1: service_b verification FAILED (magic=0x" << std::hex << std::setw(8) << std::setfill('0')
                  << read_data->magic << std::dec << " value_one=" << read_data->value_one
                  << " value_two=" << read_data->value_two << ")\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: service_b verified [magic=0x" << std::hex << std::setw(8) << std::setfill('0') << kMagicB
              << std::dec << ", " << kValueOneFromVmB << ", " << kValueTwoFromVmB << "] — read from VM-B OK\n";

    // Confirm back to VM-B over the transport that we verified its data.
    const auto verified_result =
        qemu_transport.NotifyUpdate(spec_b.value(), ServiceElementType::EVENT, kElementNameVerified);
    if (!verified_result.has_value())
    {
        std::cerr << "app1: Verified notification for service_b failed to send\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: sent Verified notification for service_b to VM-B\n";

    // Wait for VM-B to confirm it verified our data.
    std::cerr << "app1: waiting for VM-B to verify service_a...\n";
    if (!WaitForFlag(gateway_core.verified_service_a_notified))
    {
        std::cerr << "app1: timed out waiting for VM-B's Verified notification for service_a\n";
        return EXIT_FAILURE;
    }
    std::cerr << "app1: VM-B verified service_a successfully!\n";

    std::cerr << "app1: both directions verified successfully!\n";
    std::cout << "verified\n";
    return EXIT_SUCCESS;
}
