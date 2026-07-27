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

#include "score/mw/com/test/fields/set_get_and_notifier/consumer.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/set_get_and_notifier/set_get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_get_and_notifier/test_constants.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::test
{

ConsumerConfig ParseConsumerConfig(int argc, const char** argv)
{
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";
    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };
    const auto args = ParseCommandLineArguments(argc, argv, parameter_description_pairs);
    const auto manifest_result = GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!manifest_result.has_value())
    {
        FailTest("Consumer: missing or invalid --", kServiceInstanceManifestArg, " argument");
    }
    return ConsumerConfig{manifest_result.value()};
}

namespace
{

constexpr auto kMaxNumSamples{1U};
const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(kInstanceSpecifierString).value();
const std::string kSetsDoneShmPath{"/fields_set_get_notifier_sets_done"};
const std::string kProviderUpdatedShmPath{"/fields_set_get_notifier_provider_updated"};
const std::string kConsumerDoneShmPath{"/fields_set_get_notifier_consumer_done"};

/// Helper: call Get() on the field and verify it equals expected_value; fail test otherwise.
void verify_get(auto& field, std::int32_t expected_value, const char* context)
{
    const auto get_result = field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer [", context, "]: Get() call failed: ", get_result.error());
    }
    const std::int32_t value = *get_result.value();
    if (value != expected_value)
    {
        FailTest("Consumer [", context, "]: Get() returned ", value, " but expected ", expected_value);
    }
}

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token)
{
    auto sets_done_sync_result = ProcessSynchronizer::Create(kSetsDoneShmPath);
    if (!sets_done_sync_result.has_value())
    {
        FailTest("Consumer: Could not create sets-done ProcessSynchronizer");
    }
    auto provider_updated_sync_result = ProcessSynchronizer::Create(kProviderUpdatedShmPath);
    if (!provider_updated_sync_result.has_value())
    {
        FailTest("Consumer: Could not create provider-updated ProcessSynchronizer");
    }
    auto consumer_done_sync_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_sync_result.has_value())
    {
        FailTest("Consumer: Could not create consumer-done ProcessSynchronizer");
    }
    ExitFunctionGuard consumer_done_guard{[&consumer_done_sync_result]() {
        consumer_done_sync_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetGetAndNotifierProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_get_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register receive handler for the initial value before subscribing
    std::cout << "\nConsumer: Step 2 - Register receive handler for initial value" << std::endl;
    std::int32_t notifier_initial_value{};
    auto initial_callback = [&notifier_initial_value](const auto& sample_ptr) noexcept {
        notifier_initial_value = *sample_ptr;
    };
    ProxyEventReceiver initial_value_receiver{proxy.set_get_and_notifier_enabled_field, std::move(initial_callback)};

    // Step 3. Register state change handler and subscribe
    std::cout << "\nConsumer: Step 3 - Subscribe to field" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_get_and_notifier_enabled_field};
    std::ignore = proxy.set_get_and_notifier_enabled_field.Subscribe(kMaxNumSamples);
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed");
    }

    // Step 4. Verify initial value via notifier (GetNewSamples) and via Get()
    std::cout << "\nConsumer: Step 4 - Verify initial value via notifier and Get()" << std::endl;
    if (!initial_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) || notifier_initial_value != kInitialValue)
    {
        FailTest(
            "Consumer: Notifier initial value mismatch. Expected ", kInitialValue, " but got ", notifier_initial_value);
    }
    verify_get(proxy.set_get_and_notifier_enabled_field, kInitialValue, "initial");

    // Step 5. Set(kValidSetValue) — register receiver first to avoid missing the notification
    std::cout << "\nConsumer: Step 5 - Set(kValidSetValue=" << kValidSetValue << ")" << std::endl;
    std::int32_t notifier_valid_value{};
    auto valid_set_callback = [&notifier_valid_value](const auto& sample_ptr) noexcept {
        notifier_valid_value = *sample_ptr;
    };
    ProxyEventReceiver valid_set_receiver{proxy.set_get_and_notifier_enabled_field, std::move(valid_set_callback)};
    {
        const auto set_result = proxy.set_get_and_notifier_enabled_field.Set(kValidSetValue);
        if (!set_result.has_value())
        {
            FailTest("Consumer: Set(kValidSetValue) call failed: ", set_result.error());
        }
        const std::int32_t accepted_value = *set_result.value();
        if (accepted_value != kValidSetValue)
        {
            FailTest("Consumer: Set(kValidSetValue) accepted value mismatch. Expected ",
                     kValidSetValue,
                     " but got ",
                     accepted_value);
        }
    }

    // Step 6. Verify valid Set() result via notifier (GetNewSamples) and via Get()
    std::cout << "\nConsumer: Step 6 - Verify kValidSetValue via notifier and Get()" << std::endl;
    if (!valid_set_receiver.WaitForSamples(stop_token, kMaxNumSamples) || notifier_valid_value != kValidSetValue)
    {
        FailTest("Consumer: Notifier after valid Set mismatch. Expected ",
                 kValidSetValue,
                 " but got ",
                 notifier_valid_value);
    }
    verify_get(proxy.set_get_and_notifier_enabled_field, kValidSetValue, "after valid Set");

    // Step 7. Set(kInvalidSetValue) — register receiver first
    std::cout << "\nConsumer: Step 7 - Set(kInvalidSetValue=" << kInvalidSetValue << "), expect clamped to "
              << kClampedSetValue << std::endl;
    std::int32_t notifier_clamped_value{};
    auto clamped_set_callback = [&notifier_clamped_value](const auto& sample_ptr) noexcept {
        notifier_clamped_value = *sample_ptr;
    };
    ProxyEventReceiver clamped_set_receiver{proxy.set_get_and_notifier_enabled_field, std::move(clamped_set_callback)};
    {
        const auto set_result = proxy.set_get_and_notifier_enabled_field.Set(kInvalidSetValue);
        if (!set_result.has_value())
        {
            FailTest("Consumer: Set(kInvalidSetValue) call failed: ", set_result.error());
        }
        const std::int32_t accepted_value = *set_result.value();
        if (accepted_value != kClampedSetValue)
        {
            FailTest("Consumer: Set(kInvalidSetValue) accepted value mismatch. Expected clamped ",
                     kClampedSetValue,
                     " but got ",
                     accepted_value);
        }
    }

    // Step 8. Verify clamped Set() result via notifier (GetNewSamples) and via Get()
    std::cout << "\nConsumer: Step 8 - Verify clamped value via notifier and Get()" << std::endl;
    if (!clamped_set_receiver.WaitForSamples(stop_token, kMaxNumSamples) || notifier_clamped_value != kClampedSetValue)
    {
        FailTest("Consumer: Notifier after clamped Set mismatch. Expected ",
                 kClampedSetValue,
                 " but got ",
                 notifier_clamped_value);
    }
    verify_get(proxy.set_get_and_notifier_enabled_field, kClampedSetValue, "after clamped Set");

    // Step 9. Signal provider that all Set() tests are complete
    std::cout << "\nConsumer: Step 9 - Signal provider sets-done" << std::endl;
    sets_done_sync_result->Notify();

    // Step 10. Wait for provider to publish kUpdatedValue
    std::cout << "\nConsumer: Step 10 - Wait for provider updated notification" << std::endl;
    if (!provider_updated_sync_result->WaitWithAbort(stop_token))
    {
        FailTest("Consumer: WaitWithAbort (provider-updated) was stopped by stop_token");
    }

    // Step 11. Register receiver for the updated value and wait for notification
    std::cout << "\nConsumer: Step 11 - Verify kUpdatedValue via notifier and Get()" << std::endl;
    std::int32_t notifier_updated_value{};
    auto updated_callback = [&notifier_updated_value](const auto& sample_ptr) noexcept {
        notifier_updated_value = *sample_ptr;
    };
    ProxyEventReceiver updated_value_receiver{proxy.set_get_and_notifier_enabled_field, std::move(updated_callback)};
    if (!updated_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) || notifier_updated_value != kUpdatedValue)
    {
        FailTest(
            "Consumer: Notifier after Update mismatch. Expected ", kUpdatedValue, " but got ", notifier_updated_value);
    }
    verify_get(proxy.set_get_and_notifier_enabled_field, kUpdatedValue, "after Update");

    proxy.set_get_and_notifier_enabled_field.Unsubscribe();

    std::cout << "\nConsumer: All checks passed." << std::endl;
}

}  // namespace score::mw::com::test
