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
#include "score/mw/com/test/generic_proxy/unsubscribe_on_destruction/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/generic_proxy/unsubscribe_on_destruction/test_event_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/generic_proxy_unsub_on_destruction_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/generic_proxy/unsubscribe_on_destruction/TestEvents"}).value();

GenericProxyEvent& GetTestEvent(GenericProxy& generic_proxy)
{
    auto events = generic_proxy.GetEvents();
    auto event_it = events.find(kTestEventName);
    if (event_it == events.cend())
    {
        FailTest("Consumer: GenericProxy does not contain event ", kTestEventName);
    }
    return event_it->second;
}

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("GenericProxy unsubscribe_on_destruction consumer failed: Could not create ProcessSynchronizer");
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of this
    // function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    {
        ProxyContainer<GenericProxy> proxy_container{};

        // Step 1. Find service and create GenericProxy
        std::cout << "\nConsumer: Step 1" << std::endl;
        proxy_container.CreateProxy(kInstanceSpecifier, "unsubscribe_on_destruction");
        auto& test_event = GetTestEvent(proxy_container.GetProxy());

        // Step 2. Register receive handler
        std::cout << "\nConsumer: Step 2" << std::endl;
        ProxyEventReceiver proxy_event_receiver{test_event};

        // Step 3. Subscribe to event and notify provider
        std::cout << "\nConsumer: Step 3" << std::endl;
        const auto subscribe_result = test_event.Subscribe(kNumberOfSamples);
        if (!subscribe_result.has_value())
        {
            FailTest("Consumer: Subscribe failed: ", subscribe_result.error());
        }
        if (test_event.GetSubscriptionState() != SubscriptionState::kSubscribed)
        {
            FailTest("Consumer: Event is not in state kSubscribed after Subscribe");
        }
        process_synchronizer_result->Notify();

        // Step 4. Receive samples
        std::cout << "\nConsumer: Step 4" << std::endl;
        std::uint32_t expected_value{1U};
        auto check_sample = [&expected_value](SamplePtr<void> sample) {
            if (!sample)
            {
                FailTest("Consumer: Received null sample");
            }
            const auto received_value = *static_cast<const std::uint32_t*>(sample.get());
            if (received_value != expected_value)
            {
                FailTest("Consumer: Received value ", received_value, " but expected ", expected_value);
            }
            ++expected_value;
        };
        if (!proxy_event_receiver.WaitForSamples(stop_token, kNumberOfSamples, check_sample))
        {
            FailTest("Consumer: WaitForSamples was interrupted by stop_token");
        }

        // Step 5. Destroy GenericProxy without unsubscribing
        std::cout << "\nConsumer: Step 5" << std::endl;
    }

    std::cout << "Consumer: GenericProxy destroyed while subscribed" << std::endl;
}

}  // namespace score::mw::com::test
