/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/fields/set_and_get_and_notifier/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/getter_only_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_get_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/test_constants.h"
#include "score/mw/com/test/fields/test_resources/getter_and_setter_checkers.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

namespace score::mw::com::test
{
namespace
{

void run_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&done_synchronizer_result]() {
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<NotifierOnlyEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "notifier");
    auto& proxy = proxy_container.GetProxy();

    ProxyEventReceiver field_receiver{proxy.notifier_only_enabled_field};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.notifier_only_enabled_field};

    // Step 2. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 2 - Subscribe to field" << std::endl;
    const auto subscribe_result = proxy.notifier_only_enabled_field.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed in notifier scenario: ", subscribe_result.error());
    }

    // Step 3. Wait for subscription
    std::cout << "\nConsumer: Step 3 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 4. Wait for all expected samples
    std::cout << "\nConsumer: Step 4 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }
}

void run_set_and_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    auto set_done_synchronizer_result = ProcessSynchronizer::Create(kSetDoneShmPath);
    if (!set_done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create set-done ProcessSynchronizer");
    }
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result, &set_done_synchronizer_result]() {
        process_synchronizer_result->Notify();
        set_done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetAndNotifierEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    ProxyEventReceiver field_receiver{proxy.set_and_notifier_enabled_field};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_and_notifier_enabled_field};

    // Step 2. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 2 - Subscribe to field" << std::endl;
    const auto subscribe_result = proxy.set_and_notifier_enabled_field.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed in set_and_notifier scenario: ", subscribe_result.error());
    }

    // Step 3. Wait for subscription
    std::cout << "\nConsumer: Step 3 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in set scenario");
    }

    // Step 4. Wait for all expected samples
    std::cout << "\nConsumer: Step 4 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> first_values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, first_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    // Step 5. Set new field value and verify accepted value matches expected transformed value
    std::cout << "\nConsumer: Step 5 - Set field value and verify accepted value" << std::endl;
    const auto expected_accepted_set_value = (kSetRequestValue * 2) + 1;
    CallSetAndCheckReturnValue(proxy.set_and_notifier_enabled_field, kSetRequestValue, expected_accepted_set_value);

    // Step 6. Notify provider that Set() was called
    std::cout << "\nConsumer: Step 6 - Notify provider that Set() was verified" << std::endl;
    set_done_synchronizer_result->Notify();

    // Step 7. Wait for additional samples sent after the provider receives the set-complete signal
    std::cout << "\nConsumer: Step 7 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> second_values_to_receive = {expected_accepted_set_value, 10, 100};
    if (!field_receiver.WaitForSamples(stop_token, second_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }
}

void run_get_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }

    auto got_value_process_synchronizer_result = ProcessSynchronizer::Create(kConsumerGotValueShmPath);
    if (!got_value_process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create GotValueProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&process_synchronizer_result, &got_value_process_synchronizer_result]() {
        process_synchronizer_result->Notify();
        got_value_process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<GetterOnlyEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "get");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Call Get() and verify the value matches what the provider set via Update()
    std::cout << "\nConsumer: Step 2 - Call Get() and verify initial value" << std::endl;
    CallGetAndCheckValue(proxy.getter_only_enabled_field, kInitialValue);

    // Step 3. Notify provider that Get() was called and the value was verified
    std::cout << "\nConsumer: Step 3 - Notify provider that Get() was called and value verified" << std::endl;
    got_value_process_synchronizer_result->Notify();

    // Step 4. Keep getting the latest value until the received value is the updated value sent by the provider
    std::cout << "\nConsumer: Step 4 - Keep calling Get() until the updated value is received" << std::endl;
    CallGetUntilExpectedValueReceived(proxy.getter_only_enabled_field, kUpdatedValue, stop_token);
}

void run_get_and_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }

    auto got_value_process_synchronizer_result = ProcessSynchronizer::Create(kConsumerGotValueShmPath);
    if (!got_value_process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create GotValueProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&process_synchronizer_result, &got_value_process_synchronizer_result]() {
        process_synchronizer_result->Notify();
        got_value_process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<GetAndNotifierEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "get_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    ProxyEventReceiver field_receiver{proxy.get_and_notifier_enabled_field};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.get_and_notifier_enabled_field};

    // Step 2. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 2 - Subscribe to field" << std::endl;
    const auto subscribe_result = proxy.get_and_notifier_enabled_field.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed in notifier scenario: ", subscribe_result.error());
    }

    // Step 3. Wait for subscription
    std::cout << "\nConsumer: Step 3 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 4. Wait for all expected samples
    std::cout << "\nConsumer: Step 4 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> first_values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, first_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    // Step 5. Verify that Get() returns the final value sent by the provider
    std::cout << "\nConsumer: Step 5 - Call Get() and verify latest received value" << std::endl;
    CallGetAndCheckValue(proxy.get_and_notifier_enabled_field, first_values_to_receive.back());

    // Step 6. Notify provider that Get() was called and the value was verified
    std::cout << "\nConsumer: Step 6 - Notify provider that Get() was called and value verified" << std::endl;
    got_value_process_synchronizer_result->Notify();

    // Step 7. Keep getting the latest value until the received value is the updated value sent by the provider
    std::cout << "\nConsumer: Step 7 - Keep calling Get() until the updated value is received" << std::endl;
    const std::vector<std::int32_t> second_values_to_receive = {10, 100};
    CallGetUntilExpectedValueReceived(
        proxy.get_and_notifier_enabled_field, second_values_to_receive.back(), stop_token);

    // Step 8. Wait for all follow-up samples sent after the provider receives the consumer signal (we should still get
    // the sample that was read by Get())
    std::cout << "\nConsumer: Step 8 - Wait for all expected samples" << std::endl;
    if (!field_receiver.WaitForSamples(stop_token, second_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }
}

void run_set_and_get_consumer()
{
    auto consumer_done_sync_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_sync_result.has_value())
    {
        FailTest("Consumer: Could not create consumer-done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&consumer_done_sync_result]() {
        consumer_done_sync_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetAndGetEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_and_get");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Get() initial value
    std::cout << "\nConsumer: Step 2 - Get() initial value" << std::endl;
    CallGetAndCheckValue(proxy.set_and_get_enabled_field, kInitialValue);

    // Step 3. Set valid value and verify accepted value via Get()
    std::cout << "\nConsumer: Step 3 - Set valid value (" << kSetRequestValue << ") and verify via Get()" << std::endl;
    const auto expected_accepted_set_value = (kSetRequestValue * 2) + 1;
    CallSetAndCheckReturnValue(proxy.set_and_get_enabled_field, kSetRequestValue, expected_accepted_set_value);
    CallGetAndCheckValue(proxy.set_and_get_enabled_field, expected_accepted_set_value);
}

void run_set_and_get_and_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto consumer_done_sync_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_sync_result.has_value())
    {
        FailTest("Consumer: Could not create consumer-done ProcessSynchronizer");
    }
    auto set_done_synchronizer_result = ProcessSynchronizer::Create(kSetDoneShmPath);
    if (!set_done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create set-done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&consumer_done_sync_result]() {
        consumer_done_sync_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetAndGetAndNotifierEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_get_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    ProxyEventReceiver field_receiver{proxy.set_and_get_and_notifier_enabled_field};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_and_get_and_notifier_enabled_field};

    // Step 2. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 2 - Subscribe to field" << std::endl;
    const auto subscribe_result = proxy.set_and_get_and_notifier_enabled_field.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed in notifier scenario: ", subscribe_result.error());
    }

    // Step 3. Wait for subscription
    std::cout << "\nConsumer: Step 3 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 4. Wait for all expected samples
    std::cout << "\nConsumer: Step 4 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> first_values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, first_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    // Step 5. Verify that Get() returns the final value sent by the provider
    std::cout << "\nConsumer: Step 5 - Call Get() and verify latest received value" << std::endl;
    CallGetAndCheckValue(proxy.set_and_get_and_notifier_enabled_field, first_values_to_receive.back());

    // Step 6. Set valid value and verify accepted return value
    std::cout << "\nConsumer: Step 6 - Set valid value (" << kSetRequestValue << ") and verify accepted value"
              << std::endl;
    const auto expected_accepted_set_value = (kSetRequestValue * 2) + 1;
    CallSetAndCheckReturnValue(
        proxy.set_and_get_and_notifier_enabled_field, kSetRequestValue, expected_accepted_set_value);

    // Step 7. Notify provider that Set() was called
    std::cout << "\nConsumer: Step 7 - Notify provider that Set() was verified" << std::endl;
    set_done_synchronizer_result->Notify();

    // Step 8. Wait for additional samples, including the transformed set value and provider follow-up values
    std::cout << "\nConsumer: Step 8 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> second_values_to_receive = {expected_accepted_set_value, 10, 100};
    if (!field_receiver.WaitForSamples(stop_token, second_values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    // Step 9. Verify Get() returns the latest value after all updates
    std::cout << "\nConsumer: Step 9 - Call Get() and verify latest value" << std::endl;
    CallGetAndCheckValue(proxy.set_and_get_and_notifier_enabled_field, second_values_to_receive.back());
}

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token, TestMode mode)
{
    switch (mode)
    {
        case TestMode::kNotifier:
            run_notifier_consumer(stop_token);
            return;
        case TestMode::kSetAndNotifier:
            run_set_and_notifier_consumer(stop_token);
            return;
        case TestMode::kGet:
            run_get_consumer(stop_token);
            return;
        case TestMode::kGetAndNotifier:
            run_get_and_notifier_consumer(stop_token);
            return;
        case TestMode::kSetAndGet:
            run_set_and_get_consumer();
            return;
        case TestMode::kSetAndGetAndNotifier:
            run_set_and_get_and_notifier_consumer(stop_token);
            return;
    }
}

}  // namespace score::mw::com::test
