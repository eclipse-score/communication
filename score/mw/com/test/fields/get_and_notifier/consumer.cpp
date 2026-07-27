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

#include "score/mw/com/test/fields/get_and_notifier/consumer.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/get_and_notifier/get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/get_and_notifier/test_constants.h"
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
const std::string kConsumerDoneShmPath{"/fields_get_and_notifier_consumer_done"};

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<GetAndNotifierProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "get_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register receive handler for the initial value (before subscribing to avoid missing the
    //         notification from the initial Update() that the provider called before offering service)
    std::cout << "\nConsumer: Step 2 - Register receive handler for the initial value" << std::endl;
    std::int32_t notifier_value{};
    auto notifier_callback = [&notifier_value](const auto& sample_ptr) noexcept {
        notifier_value = *sample_ptr;
    };
    ProxyEventReceiver initial_value_receiver{proxy.get_and_notifier_enabled_field, std::move(notifier_callback)};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.get_and_notifier_enabled_field};

    // Step 4. Subscribe to field
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.get_and_notifier_enabled_field.Subscribe(kMaxNumSamples);

    // Step 5. Wait for subscription
    std::cout << "\nConsumer: Step 5 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed");
    }

    // Step 6. Verify initial value received via notifier (GetNewSamples)
    std::cout << "\nConsumer: Step 6 - Verify initial value via notifier" << std::endl;
    if (!initial_value_receiver.WaitForSamples(stop_token, kMaxNumSamples) || notifier_value != kInitialValue)
    {
        FailTest("Consumer: Did not receive expected initial value via notifier. Expected ",
                 kInitialValue,
                 " but got ",
                 notifier_value);
    }

    // Step 7. Verify same initial value via Get() (WithGetter)
    std::cout << "\nConsumer: Step 7 - Verify initial value via Get()" << std::endl;
    const auto get_result = proxy.get_and_notifier_enabled_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() call failed: ", get_result.error());
    }
    const std::int32_t getter_value = *get_result.value();
    if (getter_value != kInitialValue)
    {
        FailTest("Consumer: Get() returned unexpected value. Expected ", kInitialValue, " but got ", getter_value);
    }

    proxy.get_and_notifier_enabled_field.Unsubscribe();

    std::cout << "\nConsumer: All checks passed." << std::endl;
}

}  // namespace score::mw::com::test
