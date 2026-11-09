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
    proxy_container.CreateProxy(GetTestInstanceSpecifier(), std::string{kTestNamePrefix});
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

    void WaitForState(const SubscriptionState expected_state, const score::cpp::stop_token& stop_token)
    {
        if (!state_notifier_.WaitForStateChange(stop_token, expected_state))
        {
            FailTest("skeleton_reoffer consumer: WaitForStateChange was interrupted by stop_token");
        }
    }

    void WaitSubscribedAndReceive(const std::vector<std::uint32_t>& expected_samples,
                                  const score::cpp::stop_token& stop_token)
    {
        WaitForState(SubscriptionState::kSubscribed, stop_token);
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

}  // namespace

void run_consumer(const Scenario scenario, const score::cpp::stop_token& stop_token)
{
    auto first_batch_received_sync = CreateSynchronizer(kFirstBatchReceivedShmPath);
    auto stop_offer_done_sync = CreateSynchronizer(kStopOfferDoneShmPath);
    auto ready_for_reoffer_sync = CreateSynchronizer(kReadyForReofferShmPath);
    auto reoffer_done_sync = CreateSynchronizer(kReofferDoneShmPath);
    auto consumer_done_sync = CreateSynchronizer(kConsumerDoneShmPath);

    ExitFunctionGuard done_guard{[&first_batch_received_sync, &ready_for_reoffer_sync, &consumer_done_sync]() {
        first_batch_received_sync->Notify();
        ready_for_reoffer_sync->Notify();
        consumer_done_sync->Notify();
    }};

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    if (scenario == Scenario::kWithConnectedProxy)
    {
        SubscribedProxy proxy{};
        proxy.WaitSubscribedAndReceive(first_offer_samples, stop_token);
        first_batch_received_sync->Notify();
        WaitForNotification(stop_offer_done_sync, stop_token, "stop_offer_done");
        proxy.WaitForState(SubscriptionState::kSubscriptionPending, stop_token);
        ready_for_reoffer_sync->Notify();
        proxy.WaitSubscribedAndReceive(second_offer_samples, stop_token);
    }
    else
    {
        if (scenario == Scenario::kProxyDisconnectedBeforeReoffer)
        {
            {
                SubscribedProxy first_proxy{};
                first_proxy.WaitSubscribedAndReceive(first_offer_samples, stop_token);
                first_batch_received_sync->Notify();
                WaitForNotification(stop_offer_done_sync, stop_token, "stop_offer_done");
            }
            ready_for_reoffer_sync->Notify();
        }
        WaitForNotification(reoffer_done_sync, stop_token, "reoffer_done");
        SubscribedProxy proxy{};
        proxy.WaitSubscribedAndReceive(second_offer_samples, stop_token);
    }
}

}  // namespace score::mw::com::test::skeleton_reoffer
