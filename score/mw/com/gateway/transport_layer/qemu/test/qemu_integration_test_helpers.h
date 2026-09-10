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
/// via QemuHypervisorTransport::NotifyUpdate, not by polling shared CTRL memory — matching
/// ivshmem-plain's lack of an MSI-X/doorbell.

#include "score/mw/com/gateway/gateway_application/gateway_core.h"

#include "score/result/result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace
{

constexpr std::uint32_t kShmSize = 4096U;

// Payload magic values — different per direction to prove correct routing.
constexpr std::uint32_t kMagicA = 0xCAFEBABEU;  // VM-A → VM-B
constexpr std::uint32_t kMagicB = 0xDEADBEEFU;  // VM-B → VM-A

// Service specifiers for each direction.
constexpr char kServiceA[] = "service_a";  // produced by VM-A, consumed on VM-B
constexpr char kServiceB[] = "service_b";  // produced by VM-B, consumed on VM-A

// NotifyUpdate element names distinguishing the notification purposes below. Each VM
// receives at most one of each per run, so a plain string comparison is enough to route them.
constexpr char kElementNameDataReady[] = "DataReady";
constexpr char kElementNameVerified[] = "Verified";
// Sent by VM-B once it has read VM-A's Verified notification, so VM-A knows it may close.
constexpr char kElementNameDone[] = "Done";

/// Control structure placed in each service's CTRL shm (a minimal stand-in for production's
/// ServiceDataControl). Readiness is signalled to the peer over the transport
/// (kElementNameDataReady), not by polling event_count; event_count is kept only as a
/// realistic data-plane artifact.
struct ServiceControl
{
    volatile std::uint32_t event_count;  // incremented by provider after writing DATA
};

/// Polls a bool flag until it becomes true or times out — used to wait for a message
/// delivered asynchronously by the real BidirectionalTransport.
bool WaitForFlag(const std::atomic<bool>& flag, int timeout_ms = 60000)
{
    constexpr int kSleepMs = 50;
    int elapsed = 0;
    while (!flag.load(std::memory_order_acquire) && elapsed < timeout_ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepMs));
        elapsed += kSleepMs;
    }
    return flag.load(std::memory_order_acquire);
}

/// Calls `notify_update` until it succeeds, up to `attempts` times, `interval_ms` apart.
///
/// BidirectionalTransport::SendNotification (used by NotifyUpdate) is fire-and-forget — no ack,
/// no retry — so a send can fail while the link is re-establishing (e.g. after a reconnect).
/// Retrying is safe here: the peer's handler (TestGatewayCore::NotifyUpdate) just (re-)sets an
/// idempotent flag, so a duplicate that does arrive is harmless.
template <typename NotifyFn>
bool SendNotificationWithRetries(NotifyFn&& notify_update, const char* what, int attempts = 3, int interval_ms = 500)
{
    for (int attempt = 1; attempt <= attempts; ++attempt)
    {
        if (notify_update().has_value())
        {
            return true;
        }
        std::fprintf(stderr, "SendNotificationWithRetries: %s attempt %d/%d failed to send\n", what, attempt, attempts);
        if (attempt < attempts)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
    return false;
}

/// Brings up the "intervm" virtio-net NIC (vtnet1) and assigns it the given static IP.
///
/// This is the point-to-point link between the two dual_qemu VMs. QNX only auto-configures
/// vtnet0 (SSH) in the shared qnx8_qemu boot image, so vtnet1 arrives unconfigured. Each app
/// configures its own side here rather than in the shared boot script because:
///   - each app binary has a fixed, known VM role, whereas the boot script is shared by both
///     VMs and would need runtime MAC-based branching;
///   - the boot script is parsed by mkifs at image-build time with a restricted grammar (no
///     real shell; even escaped `$` substitutions broke the parser) — ordinary compiled code
///     avoids that fragility.
bool ConfigureIntervmNic(const char* local_ip)
{
    constexpr const char* kIntervmInterface = "vtnet1";

    const int up_rc = std::system((std::string{"if_up -p "} + kIntervmInterface).c_str());
    if (up_rc != 0)
    {
        std::fprintf(stderr, "ConfigureIntervmNic: if_up -p %s failed (rc=%d)\n", kIntervmInterface, up_rc);
        return false;
    }

    const std::string ifconfig_cmd =
        std::string{"ifconfig "} + kIntervmInterface + " " + local_ip + " netmask 255.255.255.0";
    const int ifconfig_rc = std::system(ifconfig_cmd.c_str());
    if (ifconfig_rc != 0)
    {
        std::fprintf(stderr, "ConfigureIntervmNic: ifconfig %s failed (rc=%d)\n", kIntervmInterface, ifconfig_rc);
        return false;
    }

    std::fprintf(stderr, "ConfigureIntervmNic: %s configured as %s\n", kIntervmInterface, local_ip);
    return true;
}

/// Test GatewayCore stub — records ProvideService calls and routes incoming NotifyUpdate
/// notifications to local flags by element name, so app code can WaitForFlag() on them.
class TestGatewayCore final : public score::mw::com::gateway::GatewayCore
{
  public:
    std::atomic<bool> provide_service_called{false};
    std::atomic<bool> data_ready_notified{false};  // set when peer's "DataReady" NotifyUpdate arrives
    std::atomic<bool> verified_notified{false};    // set when peer's "Verified" NotifyUpdate arrives
    std::atomic<bool> done_notified{false};        // set when peer's "Done" NotifyUpdate arrives

    score::Result<void> ProvideService(score::mw::com::impl::InstanceSpecifier /*s*/,
                                       std::vector<score::mw::com::impl::EventInfo> /*e*/) override
    {
        provide_service_called.store(true, std::memory_order_release);
        return {};
    }
    score::Result<void> OfferService(score::mw::com::impl::InstanceSpecifier /*s*/) override
    {
        return {};
    }
    void StopOfferService(score::mw::com::impl::InstanceSpecifier /*s*/) override {}
    score::Result<void> NotifyUpdate(score::mw::com::impl::InstanceSpecifier /*s*/,
                                     score::mw::com::impl::ServiceElementType /*t*/,
                                     std::string element_name) override
    {
        if (element_name == kElementNameDataReady)
        {
            data_ready_notified.store(true, std::memory_order_release);
        }
        else if (element_name == kElementNameVerified)
        {
            verified_notified.store(true, std::memory_order_release);
        }
        else if (element_name == kElementNameDone)
        {
            done_notified.store(true, std::memory_order_release);
        }
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

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_TEST_QEMU_INTEGRATION_TEST_HELPERS_H
