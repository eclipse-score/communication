/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/
#include "score/mw/com/test/all_service_elements/consumer.h"

#include "score/mw/com/test/all_service_elements/all_service_elements_datatype.h"
#include "score/mw/com/test/all_service_elements/test_constants.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/test_resources/getter_and_setter_checkers.h"

#include <future>
#include <iostream>
#include <vector>

namespace score::mw::com::test
{
namespace
{

void CallMethodsOrFail(AllServiceElementsProxy& proxy)
{
    const auto with_in_args_and_return_result =
        proxy.with_in_args_and_return(kInArgsAndReturnMethodTestValueA, kInArgsAndReturnMethodTestValueB);
    if (!with_in_args_and_return_result.has_value())
    {
        FailTest(kFailureMessagePrefix,
                 " Consumer: with_in_args_and_return call failed: ",
                 with_in_args_and_return_result.error());
    }
    const auto expected_sum = kInArgsAndReturnMethodTestValueA + kInArgsAndReturnMethodTestValueB;
    if (*(with_in_args_and_return_result.value()) != expected_sum)
    {
        FailTest(kFailureMessagePrefix,
                 " Consumer: with_in_args_and_return expected ",
                 expected_sum,
                 " but got ",
                 *(with_in_args_and_return_result.value()));
    }

    const auto with_in_args_only_result =
        proxy.with_in_args_only(kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB);
    if (!with_in_args_only_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Consumer: with_in_args_only call failed: ", with_in_args_only_result.error());
    }

    const auto with_return_only_result = proxy.with_return_only();
    if (!with_return_only_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Consumer: with_return_only call failed: ", with_return_only_result.error());
    }
    if (*(with_return_only_result.value()) != kReturnOnlyMethodReturnValue)
    {
        FailTest(kFailureMessagePrefix,
                 " Consumer: with_return_only expected ",
                 kReturnOnlyMethodReturnValue,
                 " but got ",
                 *(with_return_only_result.value()));
    }

    const auto without_args_or_return_result = proxy.without_args_or_return();
    if (!without_args_or_return_result.has_value())
    {
        FailTest(kFailureMessagePrefix,
                 " Consumer: without_args_or_return call failed: ",
                 without_args_or_return_result.error());
    }
}

template <typename EventType>
void SubscribeAndReceiveEventsOrFail(EventType& proxy_event,
                                     const std::vector<TestType>& expected_values,
                                     const score::cpp::stop_token& stop_token)
{
    ProxyEventReceiver event_receiver{proxy_event};
    ProxyEventStateChangeNotifier subscription_notifier{proxy_event};

    const auto subscribe_event_result = proxy_event.Subscribe(expected_values.size());
    if (!subscribe_event_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Consumer: Subscribe to event failed: ", subscribe_event_result.error());
    }

    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest(kFailureMessagePrefix, " Consumer: Event subscription did not reach kSubscribed");
    }

    if (!event_receiver.WaitForSamples(stop_token, expected_values))
    {
        FailTest(kFailureMessagePrefix, " Consumer: Did not receive all expected samples for event");
    }
}

}  // namespace

void RunConsumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Consumer: Could not create consumer-done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<AllServiceElementsProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Call all methods and verify their results
    std::cout << "\nConsumer: Step 2 - Call methods" << std::endl;
    CallMethodsOrFail(proxy);

    // Step 3. Subscribe to events and fields and verify received samples. We call these asynchronously to ensure that
    // there are no issues with calling these functions on different service elements concurrently. We ensure that each
    // step is finished before the next one begins because the test itself assumes this (e.g. calling Get will get the
    // latest value that was received by GetNewSamples)
    std::cout << "\nConsumer: Step 3 - Test events" << std::endl;
    std::vector<std::future<void>> step_3_futures{};
    // Call a lambda within each async since the compiler cannot deduce the template type when calling
    // SubscribeAndReceiveEventsOrFail directly within std::async.
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(proxy.event_1, kEvent1ValuesToSend, stop_token);
    }));
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(proxy.event_2, kEvent2ValuesToSend, stop_token);
    }));
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(proxy.get_and_notifier_enabled_field, kAllGetAndNotifierValues, stop_token);
    }));
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(
            proxy.set_and_get_and_notifier_enabled_field, kAllSetAndGetAndNotifierValues, stop_token);
    }));
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(proxy.notifier_only_enabled_field, kAllNotifierOnlyValues, stop_token);
    }));
    step_3_futures.push_back(std::async(std::launch::async, [&]() {
        SubscribeAndReceiveEventsOrFail(proxy.set_and_notifier_enabled_field, kAllSetAndNotifierValues, stop_token);
    }));

    for (auto& future : step_3_futures)
    {
        future.get();
    }

    // Step 4. Test getters
    std::cout << "\nConsumer: Step 4 - Test getters" << std::endl;
    std::vector<std::future<void>> step_4_futures{};
    step_4_futures.push_back(std::async(std::launch::async, [&]() {
        CallGetAndCheckValue(proxy.get_only_enabled_field, kGetOnlyFieldInitialValue);
    }));
    step_4_futures.push_back(std::async(std::launch::async, [&]() {
        CallGetAndCheckValue(proxy.get_and_notifier_enabled_field, kGetAndNotifierFieldValuesToSend.back());
    }));
    step_4_futures.push_back(std::async(std::launch::async, [&]() {
        CallGetAndCheckValue(proxy.set_and_get_enabled_field, kSetAndGetFieldInitialValue);
    }));
    step_4_futures.push_back(std::async(std::launch::async, [&]() {
        CallGetAndCheckValue(proxy.set_and_get_and_notifier_enabled_field,
                             kSetAndGetAndNotifierFieldValuesToSend.back());
    }));

    for (auto& future : step_4_futures)
    {
        future.get();
    }

    // Step 5. Test setters
    std::cout << "\nConsumer: Step 5 - Test setters" << std::endl;
    std::vector<std::future<void>> step_5_futures{};
    step_5_futures.push_back(std::async(std::launch::async, [&]() {
        CallSetAndCheckReturnValue(
            proxy.set_and_get_enabled_field, kSetAndGetRequestValue, (kSetAndGetRequestValue * 2) + 1);
    }));
    step_5_futures.push_back(std::async(std::launch::async, [&]() {
        CallSetAndCheckReturnValue(
            proxy.set_and_notifier_enabled_field, kSetAndNotifierRequestValue, (kSetAndNotifierRequestValue * 2) + 1);
    }));
    step_5_futures.push_back(std::async(std::launch::async, [&]() {
        CallSetAndCheckReturnValue(proxy.set_and_get_and_notifier_enabled_field,
                                   kSetAndGetAndNotifierRequestValue,
                                   (kSetAndGetAndNotifierRequestValue * 2) + 1);
    }));

    for (auto& future : step_5_futures)
    {
        future.get();
    }
}

}  // namespace score::mw::com::test
