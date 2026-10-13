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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_TEST_QEMU_INTEGRATION_TEST_HELPERS_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_TEST_QEMU_INTEGRATION_TEST_HELPERS_H

/// Shared fixtures for the bidirectional QEMU/ivshmem gateway integration test
/// (app1_main.cpp = VM-A, app2_main.cpp = VM-B). Both apps run the identical protocol in
/// mirrored roles, so the constants and test doubles below are defined once here.
///
/// Both message transport (real BidirectionalTransport over the intervm socket NIC) and shared
/// memory (ivshmem BAR) are exercised end-to-end; only GatewayCore is stubbed, since driving a
/// full GenericSkeleton is out of scope for this transport-layer test.
///
/// Cross-VM synchronization (data-ready / verified signaling) travels over the real transport
/// via QemuHypervisorTransport::NotifyUpdate.

#include "score/mw/com/gateway/gateway_application/gateway_core.h"

#include "score/result/result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace
{

// GatewayCore's own interface still exposes impl::ServiceElementType and mw::com publishes no
// public alias for it, so this is the single deliberate impl reference in the test apps.
using ServiceElementType = score::mw::com::impl::ServiceElementType;

constexpr std::uint32_t kShmSize = 4096U;

// Payload magic values — different per direction to prove correct routing.
constexpr std::uint32_t kMagicA = 0xCAFEBABEU;  // VM-A → VM-B
constexpr std::uint32_t kMagicB = 0xDEADBEEFU;  // VM-B → VM-A

// Payload values each VM writes; the peer verifies it reads back exactly these.
constexpr std::uint32_t kValueOneFromVmA = 100U;
constexpr std::uint32_t kValueTwoFromVmA = 200U;
constexpr std::uint32_t kValueOneFromVmB = 300U;
constexpr std::uint32_t kValueTwoFromVmB = 400U;

// event_count a provider publishes once its DATA payload is fully written.
constexpr std::uint32_t kDataReadyEventCount = 1U;

// Bounds for WaitForFlag; generous because the peer VM boots independently.
constexpr std::chrono::milliseconds kFlagWaitTimeout{60000};
constexpr std::chrono::milliseconds kFlagPollInterval{50};

// Static IPs on the "intervm" NIC (vtnet1), assigned by the dual_qemu fixture.
constexpr char kIntervmIpVmA[] = "10.0.3.1";
constexpr char kIntervmIpVmB[] = "10.0.3.2";
constexpr std::uint16_t kTransportPortVmA = 46001U;
constexpr std::uint16_t kTransportPortVmB = 46002U;

// Service specifiers for each direction.
constexpr char kServiceA[] = "service_a";  // produced by VM-A, consumed on VM-B
constexpr char kServiceB[] = "service_b";  // produced by VM-B, consumed on VM-A

// NotifyUpdate element names distinguishing the two notification purposes below.
constexpr char kElementNameDataReady[] = "DataReady";
constexpr char kElementNameVerified[] = "Verified";

/// Control structure placed in each service's CTRL shm (a minimal stand-in for production's
/// ServiceDataControl). Readiness is signalled to the peer over the transport
/// (kElementNameDataReady), not by polling event_count; event_count is kept only as a
/// realistic data-plane artifact.
struct ServiceControl
{
    // Atomic rather than volatile, matching production's ServiceDataControl: volatile alone
    // gives neither atomicity nor the cross-core ordering this hands off to the reader with.
    std::atomic<std::uint32_t> event_count;
};

// A lock-based atomic would keep its lock in this process, not in the shm the peer VM maps.
static_assert(std::atomic<std::uint32_t>::is_always_lock_free, "event_count must be lock-free to cross VMs");

/// Payload laid out at the start of each service's DATA shm.
struct ServicePayload
{
    std::uint32_t magic;
    std::uint32_t value_one;
    std::uint32_t value_two;
};

/// Polls a bool flag until it becomes true or times out — used to wait for a message
/// delivered asynchronously by the real BidirectionalTransport.
bool WaitForFlag(const std::atomic<bool>& flag, std::chrono::milliseconds timeout = kFlagWaitTimeout)
{
    std::chrono::milliseconds elapsed{0};
    while (!flag.load(std::memory_order_acquire) && elapsed < timeout)
    {
        std::this_thread::sleep_for(kFlagPollInterval);
        elapsed += kFlagPollInterval;
    }
    return flag.load(std::memory_order_acquire);
}

/// Test GatewayCore stub — records service-specific ProvideService calls and routes incoming
/// notifications to service-specific flags, so one service cannot satisfy another service's wait.
class TestGatewayCore final : public score::mw::com::gateway::GatewayCore
{
  public:
    std::atomic<bool> provide_service_a_called{false};
    std::atomic<bool> provide_service_b_called{false};
    std::atomic<bool> data_ready_service_a_notified{false};
    std::atomic<bool> data_ready_service_b_notified{false};
    std::atomic<bool> verified_service_a_notified{false};
    std::atomic<bool> verified_service_b_notified{false};

    score::Result<void> ProvideService(score::mw::com::InstanceSpecifier s,
                                       std::vector<score::mw::com::EventInfo> /*e*/) override
    {
        if (s.ToString() == kServiceA)
        {
            provide_service_a_called.store(true, std::memory_order_release);
        }
        else if (s.ToString() == kServiceB)
        {
            provide_service_b_called.store(true, std::memory_order_release);
        }
        return {};
    }
    score::Result<void> OfferService(score::mw::com::InstanceSpecifier /*s*/) override
    {
        return {};
    }
    void StopOfferService(score::mw::com::InstanceSpecifier /*s*/) override {}
    score::Result<void> NotifyUpdate(score::mw::com::InstanceSpecifier s,
                                     ServiceElementType /*t*/,
                                     std::string element_name) override
    {
        const auto service = s.ToString();
        if (service == kServiceA && element_name == kElementNameDataReady)
        {
            data_ready_service_a_notified.store(true, std::memory_order_release);
        }
        else if (service == kServiceB && element_name == kElementNameDataReady)
        {
            data_ready_service_b_notified.store(true, std::memory_order_release);
        }
        else if (service == kServiceA && element_name == kElementNameVerified)
        {
            verified_service_a_notified.store(true, std::memory_order_release);
        }
        else if (service == kServiceB && element_name == kElementNameVerified)
        {
            verified_service_b_notified.store(true, std::memory_order_release);
        }
        return {};
    }
    score::Result<void> RegisterUpdateNotification(score::mw::com::InstanceSpecifier /*s*/,
                                                   ServiceElementType /*t*/,
                                                   std::string /*n*/) override
    {
        return {};
    }
    score::Result<void> UnregisterUpdateNotification(score::mw::com::InstanceSpecifier /*s*/,
                                                     ServiceElementType /*t*/,
                                                     std::string /*n*/) override
    {
        return {};
    }
};

}  // namespace

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_TEST_QEMU_INTEGRATION_TEST_HELPERS_H
