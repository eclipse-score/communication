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
#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_CONSUMER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_CONSUMER_H

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

namespace score::mw::com::test
{

inline auto MakeSampleSequenceCallback(std::optional<std::uint32_t>& latest_value, const char* failure_message_prefix)
{
    return [&latest_value, failure_message_prefix](SamplePtr<std::uint32_t> sample) {
        if (sample == nullptr)
        {
            FailTest(failure_message_prefix, " received null sample");
        }
        // After a reset latest_value is empty, so the next sample is accepted as the new baseline.
        const std::uint32_t expected_value = latest_value.has_value() ? latest_value.value() + 1U : *sample;
        if (*sample != expected_value)
        {
            FailTest(
                failure_message_prefix, " received value ", *sample, " does not match expected value ", expected_value);
        }
        latest_value = *sample;
    };
}

/// \brief Waits until subscribed, receives the expected number of samples, and notifies the provider once - repeated
/// for the given number of iterations.
template <typename ProxyEventReceiverType, typename ProxyStateChangeNotifierType, typename ProcessSynchronizerType>
void ReceiveAndNotify(ProxyEventReceiverType& proxy_event_receiver,
                      ProxyStateChangeNotifierType& proxy_state_change_notifier,
                      ProcessSynchronizerType& process_synchronizer,
                      const std::size_t num_samples_to_receive,
                      const std::size_t num_iterations,
                      const score::cpp::stop_token& stop_token)
{
    for (std::size_t iteration = 0U; iteration < num_iterations; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << num_iterations << std::endl;
        const bool subscribed =
            proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed);
        if (!subscribed)
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
        }

        const bool received = proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive);
        if (!received)
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
        }
        process_synchronizer.Notify();
    }
}

/// \brief Coordinates a proxy re-subscription across a provider re-offer. Waits until the provider has withdrawn its
/// offer (kSubscriptionPending), unsubscribes and subscribes again while the service is withdrawn, then notifies the
/// provider so that it re-offers. This ordering guarantees that the samples received afterwards come from the fresh
/// offer instead of being stale buffered samples from the previous offer.
template <typename ProxyEventType, typename ProxyStateChangeNotifierType, typename ProcessSynchronizerType>
void ResubscribeAcrossReoffer(ProxyEventType& proxy_event,
                              ProxyStateChangeNotifierType& proxy_state_change_notifier,
                              ProcessSynchronizerType& process_synchronizer,
                              const std::size_t num_samples_to_receive,
                              const score::cpp::stop_token& stop_token)
{
    std::cout << "\nConsumer: Waiting for provider to withdraw its offer" << std::endl;
    const bool withdrawn =
        proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscriptionPending);
    if (!withdrawn)
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }

    std::cout << "Consumer: Unsubscribe and subscribe again" << std::endl;
    proxy_event.Unsubscribe();
    const auto subscribe_result = proxy_event.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Re-subscribe failed: ", subscribe_result.error());
    }

    // Tell the provider we have re-subscribed so that it can re-offer the service.
    process_synchronizer.Notify();
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_CONSUMER_H
