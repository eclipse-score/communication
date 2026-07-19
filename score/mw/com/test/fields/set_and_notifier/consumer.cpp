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

#include "score/mw/com/test/fields/set_and_notifier/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/set_and_notifier/initial_only_field.h"
#include "score/mw/com/test/fields/set_and_notifier/set_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/test_constants.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

constexpr auto kMaxNumSamples{1U};
const std::string kNotifierConsumerReadyShmPath{"/fields_notifier_consumer_ready"};
const std::string kNotifierConsumerDoneShmPath{"/fields_notifier_consumer_done"};
const std::string kSetAndNotifierConsumerDoneShmPath{"/fields_set_and_notifier_consumer_done"};

// InstanceSpecifier::Create can only fail if the provided string is invalid.
// Verified once here; all test functions reuse this constant.
const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(std::string{kInstanceSpecifierString}).value();

}  // namespace

void run_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto consumer_ready_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerReadyShmPath);
    if (!consumer_ready_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create consumer ready ProcessSynchronizer");
    }
    auto& consumer_ready_synchronizer = consumer_ready_synchronizer_result.value();

    auto done_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&consumer_ready_synchronizer_result, &done_synchronizer_result]() {
        consumer_ready_synchronizer_result->Notify();
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<InitialOnlyProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register receive handler for the initial value
    std::cout << "\nConsumer: Step 2 - Register receive handler for the initial value" << std::endl;
    std::int32_t initial_value{};
    auto initial_value_callback = [&initial_value](const auto& sample_ptr) noexcept {
        initial_value = *sample_ptr;
    };
    ProxyEventReceiver initial_value_receiver{proxy.initial_only_field, std::move(initial_value_callback)};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.initial_only_field};

    // Step 4. Subscribe to field
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.initial_only_field.Subscribe(kMaxNumSamples);

    // Step 5. Wait for subscription and verify initial value
    std::cout << "\nConsumer: Step 5 - Wait for subscription and verify initial value" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }
    if (!initial_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) || initial_value != kInitialValue)
    {
        FailTest("Consumer: Did not receive expected initial value ", kInitialValue, " in notifier scenario");
    }

    // Step 6. Register receive handler for updated value before notifying provider, to avoid missing the
    // notification if the provider calls Update() before the handler is registered.
    std::cout << "\nConsumer: Step 6 - Register receive handler for updated value" << std::endl;
    std::int32_t updated_value{};
    auto updated_value_callback = [&updated_value](const auto& sample_ptr) noexcept {
        updated_value = *sample_ptr;
    };
    ProxyEventReceiver updated_value_receiver{proxy.initial_only_field, std::move(updated_value_callback)};

    consumer_ready_synchronizer.Notify();

    // Step 7. Verify updated value received after provider publishes update
    std::cout << "\nConsumer: Step 7 - Verify updated value after provider publishes update" << std::endl;
    const bool updated_value_received = updated_value_receiver.WaitForSamples(stop_token, kMaxNumSamples);

    proxy.initial_only_field.Unsubscribe();

    if (!updated_value_received || updated_value != kUpdatedValue)
    {
        FailTest("Consumer: Did not receive expected updated value ", kUpdatedValue, " in notifier scenario");
    }
}

void run_set_and_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kSetAndNotifierConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetAndNotifierEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register receive handler for the initial value
    std::cout << "\nConsumer: Step 2 - Register receive handler for the initial value" << std::endl;
    std::int32_t initial_value{};
    auto initial_value_callback = [&initial_value](const auto& sample_ptr) noexcept {
        initial_value = *sample_ptr;
    };
    ProxyEventReceiver initial_value_receiver{proxy.set_and_notifier_enabled_field, std::move(initial_value_callback)};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_and_notifier_enabled_field};

    // Step 4. Subscribe to field
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.set_and_notifier_enabled_field.Subscribe(kMaxNumSamples);

    // Step 5. Wait for subscription and verify initial value
    std::cout << "\nConsumer: Step 5 - Wait for subscription and verify initial value" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in set scenario");
    }
    if (!initial_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) || initial_value != kInitialValue)
    {
        FailTest("Consumer: Did not receive initial value ", kInitialValue, " in set scenario");
    }

    // Step 6. Register receive handler for the transformed value before calling Set, to avoid missing the
    // notification triggered by the Set call.
    std::cout << "\nConsumer: Step 6 - Register receive handler for the transformed value" << std::endl;
    std::int32_t transformed_value{};
    auto transformed_value_callback = [&transformed_value](const auto& sample_ptr) noexcept {
        transformed_value = *sample_ptr;
    };
    ProxyEventReceiver transformed_value_receiver{proxy.set_and_notifier_enabled_field,
                                                  std::move(transformed_value_callback)};

    // Step 7. Set new field value and verify accepted value matches expected transformed value
    std::cout << "\nConsumer: Step 7 - Set field value and verify accepted value" << std::endl;
    const auto set_result = proxy.set_and_notifier_enabled_field.Set(kSetRequestValue);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set call failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());

    if (accepted_value != kSetTransformedValue)
    {
        FailTest("Consumer: Set accepted value mismatch. Expected ", kSetTransformedValue, " but got ", accepted_value);
    }

    // Step 8. Verify transformed value received via field notification
    std::cout << "\nConsumer: Step 8 - Verify transformed value via field notification" << std::endl;
    const bool transformed_value_received = transformed_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) &&
                                            transformed_value == kSetTransformedValue;
    proxy.set_and_notifier_enabled_field.Unsubscribe();

    if (!transformed_value_received)
    {
        FailTest("Consumer: Did not receive transformed value ", kSetTransformedValue, " after Set call");
    }
}

void run_consumer(const score::cpp::stop_token& stop_token, const std::string& mode)
{
    if (mode == "notifier")
    {
        run_notifier_consumer(stop_token);
        return;
    }
    if (mode == "set_and_notifier")
    {
        run_set_and_notifier_consumer(stop_token);
        return;
    }

    // TODO: Add "get" mode consumer scenario coverage once getter-enabled field variant is introduced.
    FailTest("Consumer: Unsupported mode: ", mode);
}
}  // namespace score::mw::com::test
