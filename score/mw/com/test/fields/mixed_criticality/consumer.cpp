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
#include "score/mw/com/test/fields/mixed_criticality/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/mixed_criticality/common_resources.h"
#include "score/mw/com/test/fields/mixed_criticality/datatypes/mixed_criticality_fields.h"
#include "score/mw/com/test/fields/test_resources/getter_and_setter_checkers.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace score::mw::com::test
{
namespace
{

template <typename ProxyFieldType>
void RunNotifier(ProxyFieldType& proxy_field,
                 const std::vector<std::int32_t>& expected_samples,
                 const char* const field_name,
                 const score::cpp::stop_token& stop_token)
{
    ProxyEventReceiver event_receiver{proxy_field};
    ProxyEventStateChangeNotifier state_change_notifier{proxy_field};

    // Subscribe to notifier-capable fields.
    const auto kNumSampleSlots = kExpectedFieldSamples.size();
    const auto subscribe_notifier_only_result = proxy_field.Subscribe(kNumSampleSlots);
    if (!subscribe_notifier_only_result.has_value())
    {
        FailTest("Consumer: Subscribe failed for ", field_name, ": ", subscribe_notifier_only_result.error());
    }

    // Wait for subscriptions to become active.
    std::cout << "\nConsumer: Wait for subscriptions on " << field_name << std::endl;
    if (!state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: ", field_name, " subscription did not reach kSubscribed");
    }

    // Verify that all expected samples were received.
    if (!event_receiver.WaitForSamples(stop_token, expected_samples))
    {
        FailTest("Consumer: Did not receive all expected samples for ", field_name);
    }
}

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Fields mixed_criticality consumer failed: Could not create ProcessSynchronizer");
    }

    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<MixedCriticalityFieldsProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Subscribe to notifier-capable fields and receive all updates
    std::cout << "\nConsumer: Step 2 - Subscribe and receive notifier samples" << std::endl;
    RunNotifier(proxy.notifier_only_field, kExpectedFieldSamples, "notifier_only_field", stop_token);
    RunNotifier(proxy.setter_and_notifier_field, kExpectedFieldSamples, "setter_and_notifier_field", stop_token);
    RunNotifier(proxy.getter_and_notifier_field, kExpectedFieldSamples, "getter_and_notifier_field", stop_token);
    RunNotifier(proxy.setter_getter_notifier_field, kExpectedFieldSamples, "setter_getter_notifier_field", stop_token);

    // Step 3. Verify getters on getter-capable fields
    std::cout << "\nConsumer: Step 3 - Verify getter return values" << std::endl;
    const auto latest_expected_value = kExpectedFieldSamples.back();
    CallGetAndCheckValue(proxy.getter_and_notifier_field, latest_expected_value);
    CallGetAndCheckValue(proxy.setter_getter_notifier_field, latest_expected_value);

    // For the fields without a notifier, keep getting the latest value until the received value is the updated
    // value sent by the provider (This is because we didn't wait until all samples were received via GetNewSamples, so
    // we don't know which sample will be the latest value when we call Get())
    CallGetUntilExpectedValueReceived(proxy.setter_and_getter_field, latest_expected_value, stop_token);
    CallGetUntilExpectedValueReceived(proxy.getter_only_field, latest_expected_value, stop_token);

    // Step 4. Verify setters on setter-capable fields
    std::cout << "\nConsumer: Step 4 - Verify setter return values" << std::endl;
    CallSetAndCheckReturnValue(proxy.setter_and_notifier_field, kSetRequestValue, kExpectedAcceptedSetValue);
    CallSetAndCheckReturnValue(proxy.setter_and_getter_field, kSetRequestValue, kExpectedAcceptedSetValue);
    CallSetAndCheckReturnValue(proxy.setter_getter_notifier_field, kSetRequestValue, kExpectedAcceptedSetValue);

    // Step 5. Verify getter-capable fields reflect transformed set values
    std::cout << "\nConsumer: Step 5 - Verify getter values after Set()" << std::endl;
    CallGetAndCheckValue(proxy.setter_and_getter_field, kExpectedAcceptedSetValue);
    CallGetAndCheckValue(proxy.setter_getter_notifier_field, kExpectedAcceptedSetValue);
}

}  // namespace score::mw::com::test
