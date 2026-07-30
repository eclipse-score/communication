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

#include "score/mw/com/test/common_test_resources/command_line_parser.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const std::string kNotifierConsumerDoneShmPath{"/fields_notifier_consumer_done"};
const std::string kSetAndNotifierConsumerDoneShmPath{"/fields_set_and_notifier_consumer_done"};

// InstanceSpecifier::Create can only fail if the provided string is invalid.
// Verified once here; all test functions reuse this constant.
const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(std::string{kInstanceSpecifierString}).value();

}  // namespace

void run_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&done_synchronizer_result]() {
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<InitialOnlyProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register unified receive handler that verifies samples arrive in the expected order
    std::cout << "\nConsumer: Step 2 - Register unified receive handler" << std::endl;
    constexpr auto kMaxNumSamples{2U};
    ProxyEventReceiver field_receiver{proxy.initial_only_field};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.initial_only_field};

    // Step 4. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.initial_only_field.Subscribe(kMaxNumSamples);

    // Step 5. Wait for subscription
    std::cout << "\nConsumer: Step 5 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 6. Wait for all expected samples
    std::cout << "\nConsumer: Step 6 - Wait for all expected samples" << std::endl;
    std::size_t sample_index{0U};
    auto value_callback = [&sample_index](const auto& sample_ptr) noexcept {
        if (sample_index == 0U && *sample_ptr != kInitialValue)
        {
            FailTest("Consumer: Did not receive expected initial value ", kInitialValue, " in notifier scenario");
        }
        else if (sample_index == 1U && *sample_ptr != kUpdatedValue)
        {
            FailTest("Consumer: Did not receive expected updated value ", kUpdatedValue, " in notifier scenario");
        }
        ++sample_index;
    };
    if (!field_receiver.WaitForSamples(stop_token, kMaxNumSamples, std::move(value_callback)))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    proxy.initial_only_field.Unsubscribe();
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

    // Step 2. Register unified receive handler that verifies samples arrive in the expected order
    std::cout << "\nConsumer: Step 2 - Register unified receive handler" << std::endl;
    constexpr auto kMaxNumSamples{1U};
    ProxyEventReceiver field_receiver{proxy.set_and_notifier_enabled_field};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_and_notifier_enabled_field};

    // Step 4. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.set_and_notifier_enabled_field.Subscribe(kMaxNumSamples);

    // Step 5. Wait for subscription and verify initial value
    std::cout << "\nConsumer: Step 5 - Wait for subscription and verify initial value" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in set scenario");
    }

    std::size_t sample_index{0U};
    auto value_callback = [&sample_index](const auto& sample_ptr) noexcept {
        if (sample_index == 0U && *sample_ptr != kInitialValue)
        {
            FailTest("Consumer: Did not receive initial value ", kInitialValue, " in set scenario");
        }
        else if (sample_index == 1U && *sample_ptr != kSetRequestValue * 2 + 1)
        {
            FailTest("Consumer: Did not receive transformed value ", kSetRequestValue * 2 + 1, " after Set call");
        }
        ++sample_index;
    };
    if (!field_receiver.WaitForSamples(stop_token, 1U, std::move(value_callback)))
    {
        FailTest("Consumer: Did not receive initial value ", kInitialValue, " in set scenario");
    }

    // Step 6. Set new field value and verify accepted value matches expected transformed value
    std::cout << "\nConsumer: Step 6 - Set field value and verify accepted value" << std::endl;
    const auto set_result = proxy.set_and_notifier_enabled_field.Set(kSetRequestValue);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set call failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());

    if (accepted_value != kSetRequestValue * 2 + 1)
    {
        FailTest(
            "Consumer: Set accepted value mismatch. Expected ", kSetRequestValue * 2 + 1, " but got ", accepted_value);
    }

    // Step 7. Verify transformed value received via field notification
    std::cout << "\nConsumer: Step 7 - Verify transformed value via field notification" << std::endl;
    if (!field_receiver.WaitForSamples(stop_token, 1U, value_callback))
    {
        FailTest("Consumer: Did not receive transformed value ", kSetRequestValue * 2 + 1, " after Set call");
    }
    proxy.set_and_notifier_enabled_field.Unsubscribe();
}

std::optional<ConsumerMode> ParseConsumerMode(std::string_view mode)
{
    if (mode == "notifier")
    {
        return ConsumerMode::kNotifier;
    }
    if (mode == "set_and_notifier")
    {
        return ConsumerMode::kSetAndNotifier;
    }
    // TODO: Add "get" mode consumer scenario coverage once getter-enabled field variant is introduced.
    return std::nullopt;
}

ConsumerConfig ParseConsumerConfig(int argc, const char** argv)
{
    constexpr auto kModeArg = "mode";
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";

    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kModeArg, "Consumer mode: notifier or set_and_notifier"},
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };

    const auto args = ParseCommandLineArguments(argc, argv, parameter_description_pairs);

    const auto mode_result = GetValueIfProvided<std::string>(args, kModeArg);
    if (!mode_result.has_value())
    {
        FailTest("Consumer: missing or invalid --", kModeArg, " argument");
    }

    const auto mode = ParseConsumerMode(mode_result.value());
    if (!mode.has_value())
    {
        FailTest("Consumer: unsupported --", kModeArg, " value: ", mode_result.value());
    }

    const auto manifest_result = GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!manifest_result.has_value())
    {
        FailTest("Consumer: missing or invalid --", kServiceInstanceManifestArg, " argument");
    }

    return ConsumerConfig{mode.value(), manifest_result.value()};
}

void run_consumer(const score::cpp::stop_token& stop_token, ConsumerMode mode)
{
    switch (mode)
    {
        case ConsumerMode::kNotifier:
            run_notifier_consumer(stop_token);
            return;
        case ConsumerMode::kSetAndNotifier:
            run_set_and_notifier_consumer(stop_token);
            return;
    }
}
}  // namespace score::mw::com::test
