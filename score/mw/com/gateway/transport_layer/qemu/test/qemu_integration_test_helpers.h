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
/// mirrored roles, so the constants and test doubles below are defined once here instead of
/// being duplicated verbatim in both translation units.
///
/// Both message transport (real BidirectionalTransport over the intervm socket NIC) and shared
/// memory (ivshmem BAR) are exercised end-to-end; only GatewayCore is stubbed, since driving a
/// full GenericSkeleton is out of scope for this transport-layer test.

#include "score/mw/com/gateway/gateway_application/gateway_core.h"

#include "score/result/result.h"

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

/// Polls a bool flag until it becomes true or times out. Used to wait for the real
/// BidirectionalTransport to deliver a ProvideServiceRequest asynchronously over the
/// intervm socket, since (unlike message injection) arrival time is no longer deterministic.
bool WaitForFlag(const bool& flag, int timeout_ms = 60000)
{
    constexpr int kSleepMs = 50;
    int elapsed = 0;
    while (!flag && elapsed < timeout_ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepMs));
        elapsed += kSleepMs;
    }
    return flag;
}

/// Brings up the "intervm" virtio-net NIC (vtnet1) and assigns it the given static IP.
///
/// This NIC is the point-to-point link between the two dual_qemu VMs (see
/// dual_qemu_transport_config.json's intervm_network block); QNX only auto-configures the
/// first NIC (vtnet0, used for SSH) in the shared qnx8_qemu boot image, so vtnet1 arrives
/// unconfigured. Each app configures its own side directly via if_up/ifconfig at startup
/// instead of doing this in the shared boot script, because:
///   - each app binary already has a fixed, known VM role (app1 = VM-A, app2 = VM-B), whereas
///     the boot script is one image shared by both VMs and would need runtime MAC-based
///     branching to tell them apart;
///   - the boot script is parsed by mkifs at image-build time using a restricted, largely
///     undocumented buildfile grammar (no real shell, and even backslash-escaped `$`
///     substitutions caused parser errors in inline file bodies) — ordinary compiled code
///     avoids that fragility entirely and is easy to reason about/test.
///
/// Returns true if both if_up and ifconfig succeeded.
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

/// Test GatewayCore stub — records ProvideService calls.
class TestGatewayCore final : public score::mw::com::gateway::GatewayCore
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

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_TEST_QEMU_INTEGRATION_TEST_HELPERS_H
