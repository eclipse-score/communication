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
#include "score/mw/com/test/move_semantics/proxy_field/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/move_semantics/proxy_field/test_field_datatype.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_field_move_semantics_interprocess_notification"};

/// \brief Waits for exactly one new sample and verifies it matches the expected value.
template <typename ReceiverType>
void CallGetNewSampleAndCheckValue(ReceiverType& receiver,
                                   const score::cpp::stop_token& stop_token,
                                   const std::int32_t expected_value)
{
    const std::vector<std::int32_t> expected_samples{expected_value};
    if (!receiver.WaitForSamples(stop_token, expected_samples))
    {
        FailTest("Consumer: Did not receive expected sample with value ", expected_value);
    }
}

template <typename ProxyFieldType>
void CallSetAndCheckReturnValue(ProxyFieldType& proxy_field,
                                const std::int32_t set_request_value,
                                const std::int32_t expected_accepted_value)
{
    const auto set_result = proxy_field.Set(set_request_value);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set() failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());
    if (accepted_value != expected_accepted_value)
    {
        FailTest("Consumer: Set() returned accepted value ", accepted_value, " but expected ", expected_accepted_value);
    }
    std::cout << "\nConsumer: Set() returned expected accepted value " << accepted_value << std::endl;
}

template <typename ProxyFieldType>
void CallGetAndCheckValue(ProxyFieldType& proxy_field, const std::int32_t expected_value)
{
    const auto get_result = proxy_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() failed: ", get_result.error());
    }
    if (*(get_result.value()) != expected_value)
    {
        FailTest("Consumer: Get() returned ", *(get_result.value()), " but expected ", expected_value);
    }
    std::cout << "\nConsumer: Get() returned expected value " << expected_value << std::endl;
}

template <typename ProxyFieldType>
void SubscribeField(ProxyFieldType& proxy_field, const score::cpp::stop_token& stop_token)
{
    ProxyEventStateChangeNotifier subscription_notifier{proxy_field};
    const auto subscribe_result = proxy_field.Subscribe(2U);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed: ", subscribe_result.error());
    }
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed");
    }
}

void RunConsumerMoveConstructAfterCreate(const score::cpp::stop_token& stop_token,
                                         const std::string& failure_message_prefix)
{
    // Step 1. Find service and create proxy A
    std::cout << "\nConsumer: Step 1 - Find service and create proxy A" << std::endl;
    ProxyContainer<ProxyFieldMoveSemanticsProxy> proxy_a_container{};
    proxy_a_container.CreateProxy(kInstanceSpecifierMovedTo, failure_message_prefix);
    auto proxy_a = proxy_a_container.Extract();

    // Step 2. Move construct proxy B from proxy A
    std::cout << "\nConsumer: Step 2 - Move construct proxy B from proxy A" << std::endl;
    auto proxy_b = std::move(proxy_a);

    // Step 3. Subscribe
    std::cout << "\nConsumer: Step 3 - Subscribe" << std::endl;
    SubscribeField(proxy_b.moved_field_, stop_token);

    // Step 4. GetNewSample, Set, Get
    std::cout << "\nConsumer: Step 4 - GetNewSample, Set, Get" << std::endl;
    const auto expected_accepted_value = DoubleAndIncrement(kSetRequestValue);
    {
        ProxyEventReceiver field_receiver{proxy_b.moved_field_};
        CallGetNewSampleAndCheckValue(field_receiver, stop_token, kInitialValueMovedTo);
        CallSetAndCheckReturnValue(proxy_b.moved_field_, kSetRequestValue, expected_accepted_value);
        CallGetAndCheckValue(proxy_b.moved_field_, expected_accepted_value);
    }

    // Step 5. Unsubscribe
    std::cout << "\nConsumer: Step 5 - Unsubscribe" << std::endl;
    proxy_b.moved_field_.Unsubscribe();

    // Step 6. Subscribe again
    std::cout << "\nConsumer: Step 6 - Subscribe again" << std::endl;
    SubscribeField(proxy_b.moved_field_, stop_token);

    // Step 7. GetNewSample
    std::cout << "\nConsumer: Step 7 - GetNewSample" << std::endl;
    {
        ProxyEventReceiver field_receiver{proxy_b.moved_field_};
        // Re-subscribing delivers the field's current (last accepted) value as the new initial sample, proving that
        // the re-subscribed proxy still reaches the correct channel.
        CallGetNewSampleAndCheckValue(field_receiver, stop_token, expected_accepted_value);
    }

    proxy_b.moved_field_.Unsubscribe();
}

void RunConsumerMoveAssignAfterCreate(const score::cpp::stop_token& stop_token,
                                      const std::string& failure_message_prefix)
{
    // Step 1. Find service and create proxy A (connected to the MovedFrom instance)
    std::cout << "\nConsumer: Step 1 - Find service and create proxy A" << std::endl;
    ProxyContainer<ProxyFieldMoveSemanticsProxy> proxy_a_container{};
    proxy_a_container.CreateProxy(kInstanceSpecifierMovedFrom, failure_message_prefix);
    auto proxy_a = proxy_a_container.Extract();

    // Step 2. Find service and create proxy B (connected to the MovedTo instance)
    std::cout << "\nConsumer: Step 2 - Find service and create proxy B" << std::endl;
    ProxyContainer<ProxyFieldMoveSemanticsProxy> proxy_b_container{};
    proxy_b_container.CreateProxy(kInstanceSpecifierMovedTo, failure_message_prefix);
    auto proxy_b = proxy_b_container.Extract();

    // Step 3. Move assign proxy A into proxy B. Proxy B now holds proxy A's channel (MovedFrom).
    std::cout << "\nConsumer: Step 3 - Move assign proxy A into proxy B" << std::endl;
    proxy_b = std::move(proxy_a);

    // Step 4. Subscribe
    std::cout << "\nConsumer: Step 4 - Subscribe" << std::endl;
    SubscribeField(proxy_b.moved_field_, stop_token);

    // Step 5. GetNewSample, Set, Get
    std::cout << "\nConsumer: Step 5 - GetNewSample, Set, Get" << std::endl;
    const auto expected_accepted_value = AddOneHundred(kSetRequestValue);
    {
        ProxyEventReceiver field_receiver{proxy_b.moved_field_};
        CallGetNewSampleAndCheckValue(field_receiver, stop_token, kInitialValueMovedFrom);
        CallSetAndCheckReturnValue(proxy_b.moved_field_, kSetRequestValue, expected_accepted_value);
        CallGetAndCheckValue(proxy_b.moved_field_, expected_accepted_value);
    }

    // Step 6. Unsubscribe
    std::cout << "\nConsumer: Step 6 - Unsubscribe" << std::endl;
    proxy_b.moved_field_.Unsubscribe();

    // Step 7. Subscribe again
    std::cout << "\nConsumer: Step 7 - Subscribe again" << std::endl;
    SubscribeField(proxy_b.moved_field_, stop_token);

    // Step 8. GetNewSample
    std::cout << "\nConsumer: Step 8 - GetNewSample" << std::endl;
    {
        ProxyEventReceiver field_receiver{proxy_b.moved_field_};
        // Re-subscribing delivers the field's current (last accepted) value as the new initial sample, proving that
        // the re-subscribed proxy still reaches the MovedFrom channel.
        CallGetNewSampleAndCheckValue(field_receiver, stop_token, expected_accepted_value);
    }

    proxy_b.moved_field_.Unsubscribe();
}

}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    const std::string failure_message_prefix{"proxy_field_move_semantics"};

    auto consumer_done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!consumer_done_synchronizer_result.has_value())
    {
        FailTest("proxy_field_move_semantics consumer failed: could not create consumer done synchronizer");
    }

    // Notify the provider when the consumer is done (or fails) so that it does not wait indefinitely.
    ExitFunctionGuard done_guard{[&consumer_done_synchronizer_result]() {
        consumer_done_synchronizer_result->Notify();
    }};

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructAfterCreate:
        {
            RunConsumerMoveConstructAfterCreate(stop_token, failure_message_prefix);
            break;
        }
        case ProxyMoveScenario::kMoveAssignAfterCreate:
        {
            RunConsumerMoveAssignAfterCreate(stop_token, failure_message_prefix);
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("proxy_field_move_semantics consumer failed: unknown scenario");
    }

    std::cout << "Consumer: Done with all field operations, exiting" << std::endl;
}

}  // namespace score::mw::com::test
