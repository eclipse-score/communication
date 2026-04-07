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

/// \brief ASIL-B provider main entry point for mixed criticality method tests.
/// This provider runs at ASIL-B level and offers a service with add_numbers method.
/// An ASIL-B provider automatically serves both QM and ASIL-B consumers.

#include "score/mw/com/runtime.h"

#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/provider_runner.h"
#include "score/mw/com/test/methods/mixed_criticality/common_resources.h"

#include <score/stop_token.hpp>
#include <string>

int main(int argc, const char** argv)
{
    std::cout << "Provider: Starting provider process...\n";
    auto service_instance_manifest_path = score::mw::com::test::GetServiceInstanceManifestPath(argc, argv);

    score::mw::com::test::InitializeRuntime(service_instance_manifest_path);

    // we have one consumer which spawns one proxy thus the consumer repetition is a vector of lenght 1 with element
    // value 1. This information is required by the provider to calculate how many times a proxy will call all of it's
    // registered methods.
    constexpr std::size_t num_consumer{1U};
    constexpr std::size_t num_proxy_per_consumer{1U};
    std::vector<std::size_t> consumer_repetition(num_consumer, num_proxy_per_consumer);
    auto expected_method_call_count = score::mw::com::test::CalculateExpectedMethodCallCounts(
        consumer_repetition, score::mw::com::test::kEnabledMethodIDs);

    score::mw::com::test::SetupAssertHandler();
    score::cpp::stop_source stop_source{};
    score::mw::com::test::run_provider(stop_source.get_token(), expected_method_call_count, num_consumer);
}
