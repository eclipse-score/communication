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
/// Tests the full bidirectional gateway use case over ivshmem, using per-service
/// CTRL + DATA shared memory — matching the production LoLa pattern.
///
///   - VM-A creates CTRL+DATA shm for service "service_a", writes payload, signals via CTRL
///   - VM-B creates CTRL+DATA shm for service "service_b", writes payload, signals via CTRL
///   - Each VM uses the transport to make peer's shm visible, then reads and verifies
///
/// Synchronization uses each service's CTRL shm (like production LoLa's ServiceDataControl)
/// instead of a separate raw handshake page:
///   - Provider writes DATA, then sets ctrl->event_count to signal readiness
///   - Consumer polls peer's CTRL shm event_count until it becomes non-zero
///
/// Both VMs run QemuHypervisorTransport. Each VM is simultaneously a source gateway
/// (for its own service) and a destination gateway (for the peer's service).

#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"
#include "score/mw/com/gateway/transport_layer/qemu/qemu_hypervisor_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"
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

using score::mw::com::gateway::GatewayCore;
using score::mw::com::gateway::IBidirectionalTransport;
using score::mw::com::gateway::QemuHypervisorTransport;
using score::mw::com::gateway::ResolveInterVmShmPaths;
using score::mw::com::gateway::qemu::ivshmem::DiscoverIvshmemBar;
using score::mw::com::gateway::qemu::ivshmem::IvshmemTypedMemoryProvider;
using score::mw::com::impl::InstanceSpecifier;

namespace
{
constexpr std::uint32_t kShmSize = 4096U;

// Payload magic values — different per direction to prove correct routing.
constexpr std::uint32_t kMagicA = 0xCAFEBABEU;  // VM-A → VM-B
constexpr std::uint32_t kMagicB = 0xDEADBEEFU;  // VM-B → VM-A

// Service specifiers for each direction.
constexpr char kServiceA[] = "service_a";  // produced by VM-A, consumed on VM-B
constexpr char kServiceB[] = "service_b";  // produced by VM-B, consumed on VM-A

/// Simplified control structure placed in each service's CTRL shm.
/// In production, this would be ServiceDataControl with EventControl/TransactionLogSet.
/// Here we use a minimal version: the skeleton writes data, then increments event_count.
/// The proxy polls event_count to know when data is ready.
struct ServiceControl
{
    volatile std::uint32_t event_count;  // incremented by provider after writing DATA
    volatile std::uint32_t verified;     // set by consumer after successful read
};

/// Polls a volatile uint32 until it reaches the expected value or times out.
bool WaitForCtrl(const volatile std::uint32_t& field, std::uint32_t expected, int timeout_ms = 60000)
{
    constexpr int kSleepMs = 50;
    int elapsed = 0;
    while (field < expected && elapsed < timeout_ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepMs));
        elapsed += kSleepMs;
        if (elapsed % 5000 == 0)
        {
            std::fprintf(stderr,
                         "  [polling] elapsed %d/%d ms, current value=%u, expected=%u\n",
                         elapsed,
                         timeout_ms,
                         field,
                         expected);
            std::fflush(stderr);
        }
    }
    return field >= expected;
}

/// Test IBidirectionalTransport stub — captures the message handler for injection.
class TestBidirectionalTransport final : public IBidirectionalTransport
{
  public:
    score::Result<void> Setup() override
    {
        return {};
    }
    void Shutdown() override {}
    bool IsConnected() const override
    {
        return true;
    }
    score::Result<void> SendRequest(score::mw::com::gateway::TransportMessage& /*msg*/) override
    {
        return {};
    }
    score::Result<void> SendNotification(score::mw::com::gateway::TransportMessage& /*msg*/) override
    {
        return {};
    }
    void SetMessageHandler(MessageHandler handler) override
    {
        handler_ = std::move(handler);
    }

    void DeliverMessage(std::unique_ptr<score::mw::com::gateway::TransportMessage> msg)
    {
        handler_(std::move(msg));
    }

  private:
    MessageHandler handler_;
};

/// Test GatewayCore stub — records ProvideService calls.
class TestGatewayCore final : public GatewayCore
{
  public:
    bool provide_service_called{false};

    score::Result<void> ProvideService(score::mw::com::impl::InstanceSpecifier /*s*/,
                                       std::vector<score::mw::com::impl::EventInfo> /*e*/) override
    {
        provide_service_called = true;
        return {};
    }
    score::Result<void> OfferService(score::mw::com::impl::InstanceSpecifier /*s*/) override
    {
        return {};
    }
    void StopOfferService(score::mw::com::impl::InstanceSpecifier /*s*/) override {}
    score::Result<void> NotifyUpdate(score::mw::com::impl::InstanceSpecifier /*s*/,
                                     score::mw::com::impl::ServiceElementType /*t*/,
                                     std::string /*n*/) override
    {
        return {};
    }
    score::Result<void> RegisterUpdateNotification(score::mw::com::impl::InstanceSpecifier /*s*/,
                                                   score::mw::com::impl::ServiceElementType /*t*/,
                                                   std::string /*n*/) override
    {
        return {};
    }
    score::Result<void> UnregisterUpdateNotification(score::mw::com::impl::InstanceSpecifier /*s*/,
                                                     score::mw::com::impl::ServiceElementType /*t*/,
                                                     std::string /*n*/) override
    {
        return {};
    }
};

}  // namespace

int main()
{
    std::fprintf(stderr, "=== app1 (VM-A): bidirectional gateway transport test ===\n");

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
    auto test_transport = std::make_unique<TestBidirectionalTransport>();
    auto* test_transport_ptr = test_transport.get();
    QemuHypervisorTransport qemu_transport{gateway_core, std::move(test_transport), provider};
    if (!qemu_transport.Setup().has_value())
    {
        std::fprintf(stderr, "app1: QemuHypervisorTransport::Setup failed\n");
        return 1;
    }

    // ========================================================================
    // SOURCE SIDE: Create CTRL + DATA shm for service_a via SharedMemoryFactory::Create().
    // This is exactly what the LoLa skeleton does in production — the factory routes
    // through our InterVM provider which allocates in the BAR directory.
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
    ctrl_a_ptr->verified = 0U;

    // Write payload into DATA shm.
    auto* write_data = static_cast<std::uint32_t*>(data_a->getUsableBaseAddress());
    write_data[0] = kMagicA;
    write_data[1] = 100U;
    write_data[2] = 200U;

    // Signal readiness via CTRL shm — like the skeleton updating EventDataControl.
    std::atomic_thread_fence(std::memory_order_release);
    ctrl_a_ptr->event_count = 1U;
    std::fprintf(stderr, "app1: wrote service_a [magic=0x%08x, 100, 200] and signalled via CTRL\n", kMagicA);

    // ========================================================================
    // DESTINATION SIDE: Wait for service_b's CTRL to signal data is ready, then read.
    // The transport must first make service_b's CTRL+DATA visible on this VM.
    // ========================================================================
    auto spec_b = InstanceSpecifier::Create(std::string{kServiceB});
    if (!spec_b.has_value())
    {
        std::fprintf(stderr, "app1: failed to create InstanceSpecifier for service_b\n");
        return 1;
    }
    const auto paths_b = ResolveInterVmShmPaths(spec_b.value());

    // Poll: wait until service_b's CTRL shm appears in the BAR directory.
    // This means VM-B has created it. In production, the gateway message (ProvideServiceRequest)
    // arrives to trigger this, but here we simulate waiting for VM-B's allocation to appear.
    std::fprintf(stderr, "app1: waiting for service_b CTRL to appear in BAR directory...\n");
    {
        constexpr int kSleepMs = 50;
        constexpr int kTimeoutMs = 60000;
        int elapsed = 0;
        while (!provider->LookupOffsetInDirectory(paths_b.control).has_value() && elapsed < kTimeoutMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kSleepMs));
            elapsed += kSleepMs;
            if (elapsed % 5000 == 0)
            {
                std::fprintf(stderr, "app1: still waiting for service_b CTRL... (%d/%d ms)\n", elapsed, kTimeoutMs);
                std::fflush(stderr);
            }
        }
        if (!provider->LookupOffsetInDirectory(paths_b.control).has_value())
        {
            std::fprintf(stderr, "app1: timed out waiting for service_b CTRL in directory after %d ms\n", elapsed);
            return 1;
        }
    }
    std::fprintf(stderr, "app1: service_b CTRL found in BAR directory\n");

    // Simulate receiving ProvideServiceRequest from VM-B's gateway.
    // The transport binds service_b's CTRL+DATA to local shm names at the correct BAR offsets.
    auto request_b = std::make_unique<score::mw::com::gateway::ProvideServiceRequest>(
        spec_b.value(), std::vector<score::mw::com::impl::EventInfo>{}, sizeof(ServiceControl), kShmSize);
    test_transport_ptr->DeliverMessage(std::move(request_b));

    if (!gateway_core.provide_service_called)
    {
        std::fprintf(stderr, "app1: transport did not call GatewayCore::ProvideService for service_b\n");
        return 1;
    }
    std::fprintf(stderr, "app1: transport made service_b CTRL+DATA visible on this VM\n");

    // Open service_b's CTRL shm and poll for readiness — like the proxy reading EventDataControl.
    auto ctrl_b = score::memory::shared::SharedMemoryFactory::Open(paths_b.control, /*is_read_write=*/true);
    if (ctrl_b == nullptr)
    {
        std::fprintf(stderr, "app1: Open for service_b CTRL failed\n");
        return 1;
    }
    auto* ctrl_b_ptr = static_cast<ServiceControl*>(ctrl_b->getUsableBaseAddress());

    std::fprintf(stderr, "app1: polling service_b CTRL for event_count >= 1...\n");
    if (!WaitForCtrl(ctrl_b_ptr->event_count, 1U))
    {
        std::fprintf(stderr,
                     "app1: timed out waiting for service_b CTRL event_count (final value=%u)\n",
                     ctrl_b_ptr->event_count);
        return 1;
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    std::fprintf(stderr, "app1: service_b event_count signal received\n");

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

    // Signal back to VM-B that we verified its data (write to service_b's CTRL verified field).
    ctrl_b_ptr->verified = 1U;

    // Wait for VM-B to verify our data (polls our CTRL verified field).
    std::fprintf(stderr, "app1: waiting for VM-B to verify service_a...\n");
    if (!WaitForCtrl(ctrl_a_ptr->verified, 1U))
    {
        std::fprintf(
            stderr, "app1: timed out waiting for VM-B to verify service_a (final verified=%u)\n", ctrl_a_ptr->verified);
        return 1;
    }
    std::fprintf(stderr, "app1: VM-B verified service_a successfully!\n");

    std::fprintf(stderr, "app1: both directions verified successfully!\n");
    std::printf("verified\n");
    return 0;
}
