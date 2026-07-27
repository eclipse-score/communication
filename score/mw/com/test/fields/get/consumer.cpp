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

#include "score/mw/com/test/fields/get/consumer.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/fields/get/getter_only_field.h"
#include "score/mw/com/test/fields/get/test_constants.h"

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
const std::string kConsumerDoneShmPath{"/fields_getter_consumer_done"};

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
    ProxyContainer<GetterOnlyProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "get");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Call Get() to retrieve the field value set by the provider via Update()
    std::cout << "\nConsumer: Step 2 - Call Get() to retrieve current field value" << std::endl;
    const auto get_result = proxy.getter_only_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() call failed: ", get_result.error());
    }
    const std::int32_t retrieved_value = *get_result.value();

    // Step 3. Verify that Get() returned the value set by the provider's Update() call
    std::cout << "\nConsumer: Step 3 - Verify retrieved value (" << retrieved_value
              << ") matches expected initial value (" << kInitialValue << ")" << std::endl;
    if (retrieved_value != kInitialValue)
    {
        FailTest("Consumer: Get() returned unexpected value. Expected ", kInitialValue, " but got ", retrieved_value);
    }

    std::cout << "\nConsumer: All checks passed." << std::endl;
}

}  // namespace score::mw::com::test
