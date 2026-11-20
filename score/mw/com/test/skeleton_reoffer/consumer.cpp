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
#include "score/mw/com/test/skeleton_reoffer/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/skeleton_reoffer/test_datatype.h"
#include "score/mw/com/test/skeleton_reoffer/test_parameters.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::test::skeleton_reoffer
{
namespace
{

constexpr std::string_view kTestNamePrefix{"skeleton_reoffer"};

using TestProxyEvent = decltype(std::declval<TestProxy>().test_event);

TestProxyEvent& CreateProxyAndGetEvent(ProxyContainer<TestProxy>& proxy_container)
{
    proxy_container.CreateProxy(CreateTestInstanceSpecifierOrFail(), std::string{kTestNamePrefix});
    return proxy_container.GetProxy().test_event;
}

class SubscribedProxy
{
  public:
    SubscribedProxy()
        : proxy_container_{},
          event_receiver_{CreateProxyAndGetEvent(proxy_container_)},
          state_notifier_{proxy_container_.GetProxy().test_event}
    {
        const auto subscribe_result = proxy_container_.GetProxy().test_event.Subscribe(kNumSamplesPerOffer);
        if (!subscribe_result.has_value())
        {
            FailTest("skeleton_reoffer consumer: Subscribe failed: ", subscribe_result.error());
        }
    }

    ~SubscribedProxy() = default;
    SubscribedProxy(const SubscribedProxy&) = delete;
    SubscribedProxy& operator=(const SubscribedProxy&) = delete;
    SubscribedProxy(SubscribedProxy&&) = delete;
    SubscribedProxy& operator=(SubscribedProxy&&) = delete;

    void WaitSubscribedAndReceive(const std::vector<std::uint32_t>& expected_samples,
                                  const score::cpp::stop_token& stop_token)
    {
        if (!state_notifier_.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
        {
            FailTest("skeleton_reoffer consumer: WaitForStateChange was interrupted by stop_token");
        }
        if (!event_receiver_.WaitForSamples(stop_token, expected_samples))
        {
            FailTest("skeleton_reoffer consumer: WaitForSamples was interrupted by stop_token");
        }
    }

  private:
    ProxyContainer<TestProxy> proxy_container_{};
    ProxyEventReceiver<TestProxyEvent> event_receiver_;
    ProxyEventStateChangeNotifier<TestProxyEvent> state_notifier_;
};

void run_consumer_no_connected_proxy(const score::cpp::stop_token& stop_token)
{
    auto reoffer_done_sync = ProcessSynchronizer::Create(std::string{kReofferDoneShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    // Unblock the waiting provider if FailTest exits early so the test fails fast instead of timing out.
    ExitFunctionGuard done_guard{[&second_batch_of_samples_received_sync]() {
        second_batch_of_samples_received_sync->Notify();
    }};

    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    // Wait until the provider offers, sends the first batch, stops offering, and re-offers with no proxy attached.
    WaitForNotificationOrFail(reoffer_done_sync, stop_token, "reoffer_done");

    // Connect the first proxy only after re-offer and verify it receives the second batch of samples.
    SubscribedProxy proxy{};
    proxy.WaitSubscribedAndReceive(second_offer_samples, stop_token);
    second_batch_of_samples_received_sync->Notify();
}

void run_consumer_with_connected_proxy(const score::cpp::stop_token& stop_token)
{
    auto first_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kFirstBatchOfSamplesReceivedShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    ExitFunctionGuard done_guard{[&first_batch_of_samples_received_sync, &second_batch_of_samples_received_sync]() {
        first_batch_of_samples_received_sync->Notify();
        second_batch_of_samples_received_sync->Notify();
    }};

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    // Connect a proxy for the initial offer, receive the first batch, and signal the provider to stop and re-offer.
    SubscribedProxy proxy{};
    proxy.WaitSubscribedAndReceive(first_offer_samples, stop_token);
    first_batch_of_samples_received_sync->Notify();

    // Keep the same proxy alive while the provider calls StopOfferService() and OfferService(). It auto-reconnects
    // via service discovery and receives the second batch.
    proxy.WaitSubscribedAndReceive(second_offer_samples, stop_token);
    second_batch_of_samples_received_sync->Notify();
}

void run_consumer_proxy_disconnected_before_reoffer(const score::cpp::stop_token& stop_token)
{
    auto first_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kFirstBatchOfSamplesReceivedShmPath});
    auto stop_offer_done_sync = ProcessSynchronizer::Create(std::string{kStopOfferDoneShmPath});
    auto ready_for_reoffer_sync = ProcessSynchronizer::Create(std::string{kReadyForReofferShmPath});
    auto reoffer_done_sync = ProcessSynchronizer::Create(std::string{kReofferDoneShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    ExitFunctionGuard done_guard{
        [&first_batch_of_samples_received_sync, &ready_for_reoffer_sync, &second_batch_of_samples_received_sync]() {
            first_batch_of_samples_received_sync->Notify();
            ready_for_reoffer_sync->Notify();
            second_batch_of_samples_received_sync->Notify();
        }};

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    // Keep first_proxy connected while StopOfferService() runs so the skeleton leaves shared memory in place.
    {
        SubscribedProxy first_proxy{};
        first_proxy.WaitSubscribedAndReceive(first_offer_samples, stop_token);
        first_batch_of_samples_received_sync->Notify();
        WaitForNotificationOrFail(stop_offer_done_sync, stop_token, "stop_offer_done");
    }
    // first_proxy is now destroyed. Signal the provider to call OfferService(), which recreates shared memory.
    ready_for_reoffer_sync->Notify();

    // Connect a new proxy to the re-offered instance and verify it receives the second batch of samples.
    WaitForNotificationOrFail(reoffer_done_sync, stop_token, "reoffer_done");
    SubscribedProxy second_proxy{};
    second_proxy.WaitSubscribedAndReceive(second_offer_samples, stop_token);
    second_batch_of_samples_received_sync->Notify();
}

}  // namespace

void run_consumer(const Scenario scenario, const score::cpp::stop_token& stop_token)
{
    switch (scenario)
    {
        case Scenario::kNoConnectedProxy:
            run_consumer_no_connected_proxy(stop_token);
            break;
        case Scenario::kWithConnectedProxy:
            run_consumer_with_connected_proxy(stop_token);
            break;
        case Scenario::kProxyDisconnectedBeforeReoffer:
            run_consumer_proxy_disconnected_before_reoffer(stop_token);
            break;
        default:
            FailTest("skeleton_reoffer consumer: Unknown scenario: ", static_cast<std::uint32_t>(scenario));
            break;
    }
}

}  // namespace score::mw::com::test::skeleton_reoffer
