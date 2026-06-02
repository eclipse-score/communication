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

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/field_consumer.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

int main(int argc, const char** argv)
{
    constexpr auto kNumRetriesArg = "num-retries";
    constexpr auto kBackoffTimeArg = "backoff-time";
    constexpr auto kModeArg = "mode";
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";

    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kNumRetriesArg, "Number of retries"},
        {kBackoffTimeArg, "Retry backoff time in milliseconds"},
        {kModeArg, "Consumer mode: notifier or set"},
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };

    const auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, parameter_description_pairs);

    const auto num_retries_result = score::mw::com::test::GetValueIfProvided<std::size_t>(args, kNumRetriesArg);
    if (!num_retries_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: missing or invalid --", kNumRetriesArg, " argument");
    }

    const auto backoff_time_result = score::mw::com::test::GetValueIfProvided<std::size_t>(args, kBackoffTimeArg);
    if (!backoff_time_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: missing or invalid --", kBackoffTimeArg, " argument");
    }

    const auto mode_result = score::mw::com::test::GetValueIfProvided<std::string>(args, kModeArg);
    if (!mode_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: missing or invalid --", kModeArg, " argument");
    }

    const auto manifest_result =
        score::mw::com::test::GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!manifest_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: missing or invalid --", kServiceInstanceManifestArg, " argument");
    }

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{manifest_result.value()});

    score::mw::com::test::run_consumer(
        num_retries_result.value(), std::chrono::milliseconds{backoff_time_result.value()}, mode_result.value());
    return EXIT_SUCCESS;
}
