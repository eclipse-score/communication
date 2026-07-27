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

#include "score/mw/com/test/fields/set_and_get/consumer.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/fields/set_and_get/set_and_get_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get/test_constants.h"

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

const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(kInstanceSpecifierString).value();
const std::string kSetsDoneShmPath{"/fields_set_and_get_sets_done"};
const std::string kProviderUpdatedShmPath{"/fields_set_and_get_provider_updated"};
const std::string kConsumerDoneShmPath{"/fields_set_and_get_consumer_done"};

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
    ProxyContainer<SetAndGetProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_and_get");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Get() → verify initial value set by provider's Update()
    std::cout << "\nConsumer: Step 2 - Get() to verify initial value (" << kInitialValue << ")" << std::endl;
    {
        const auto get_result = proxy.set_and_get_enabled_field.Get();
        if (!get_result.has_value())
        {
            FailTest("Consumer: Get() call failed: ", get_result.error());
        }
        const std::int32_t value = *get_result.value();
        if (value != kInitialValue)
        {
            FailTest("Consumer: Initial Get() returned unexpected value. Expected ", kInitialValue, " but got ", value);
        }
    }

    // Step 3. Set(kValidSetValue) → verify accepted value equals kValidSetValue (within clamping range)
    std::cout << "\nConsumer: Step 3 - Set(kValidSetValue=" << kValidSetValue << ")" << std::endl;
    {
        const auto set_result = proxy.set_and_get_enabled_field.Set(kValidSetValue);
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

    // Step 4. Get() → verify field now holds kValidSetValue
    std::cout << "\nConsumer: Step 4 - Get() after valid Set, expect " << kValidSetValue << std::endl;
    {
        const auto get_result = proxy.set_and_get_enabled_field.Get();
        if (!get_result.has_value())
        {
            FailTest("Consumer: Get() after valid Set failed: ", get_result.error());
        }
        const std::int32_t value = *get_result.value();
        if (value != kValidSetValue)
        {
            FailTest("Consumer: Get() after valid Set returned unexpected value. Expected ",
                     kValidSetValue,
                     " but got ",
                     value);
        }
    }

    // Step 5. Set(kInvalidSetValue) → verify accepted value is clamped to kClampedSetValue
    std::cout << "\nConsumer: Step 5 - Set(kInvalidSetValue=" << kInvalidSetValue << "), expect clamped to "
              << kClampedSetValue << std::endl;
    {
        const auto set_result = proxy.set_and_get_enabled_field.Set(kInvalidSetValue);
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

    // Step 6. Get() → verify field holds kClampedSetValue
    std::cout << "\nConsumer: Step 6 - Get() after clamped Set, expect " << kClampedSetValue << std::endl;
    {
        const auto get_result = proxy.set_and_get_enabled_field.Get();
        if (!get_result.has_value())
        {
            FailTest("Consumer: Get() after clamped Set failed: ", get_result.error());
        }
        const std::int32_t value = *get_result.value();
        if (value != kClampedSetValue)
        {
            FailTest("Consumer: Get() after clamped Set returned unexpected value. Expected ",
                     kClampedSetValue,
                     " but got ",
                     value);
        }
    }

    // Step 7. Signal provider that all Set() tests are complete
    std::cout << "\nConsumer: Step 7 - Signal provider sets-done" << std::endl;
    sets_done_sync_result->Notify();

    // Step 8. Wait for provider to publish kUpdatedValue via Update()
    std::cout << "\nConsumer: Step 8 - Wait for provider updated notification" << std::endl;
    if (!provider_updated_sync_result->WaitWithAbort(stop_token))
    {
        FailTest("Consumer: WaitWithAbort (provider-updated) was stopped by stop_token");
    }

    // Step 9. Get() → verify field holds kUpdatedValue (sent by provider's Update() call)
    std::cout << "\nConsumer: Step 9 - Get() after provider Update, expect " << kUpdatedValue << std::endl;
    {
        const auto get_result = proxy.set_and_get_enabled_field.Get();
        if (!get_result.has_value())
        {
            FailTest("Consumer: Get() after provider Update failed: ", get_result.error());
        }
        const std::int32_t value = *get_result.value();
        if (value != kUpdatedValue)
        {
            FailTest("Consumer: Get() after provider Update returned unexpected value. Expected ",
                     kUpdatedValue,
                     " but got ",
                     value);
        }
    }

    std::cout << "\nConsumer: All checks passed." << std::endl;
}

}  // namespace score::mw::com::test
