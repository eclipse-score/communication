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

#include "score/mw/com/gateway/gateway_application/gateway_core.h"
#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"

#include "score/result/result.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
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

/// Test IBidirectionalTransport stub — captures the message handler for injection.
///
/// This stub bypasses the real socket-based BidirectionalTransport on purpose: it lets the test
/// deliver a ProvideServiceRequest directly into QemuHypervisorTransport::OnMessageReceived so
/// the test focuses on verifying ivshmem shared-memory visibility across VMs (the part specific
/// to this transport), independent of the message-transport socket plumbing, which is already
/// covered by the sample transport's own tests/integration test.
class TestBidirectionalTransport final : public score::mw::com::gateway::IBidirectionalTransport
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
