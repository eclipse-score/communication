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
#include "score/mw/com/test/fields/set_and_notifier/common_resources.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"

namespace score::mw::com::test
{
namespace
{

TestMode ParseTestMode(std::string_view mode)
{
    if (mode == "notifier")
    {
        return TestMode::kNotifier;
    }
    if (mode == "set_and_notifier")
    {
        return TestMode::kSetAndNotifier;
    }
    if (mode == "get")
    {
        return TestMode::kGet;
    }
    if (mode == "get_and_notifier")
    {
        return TestMode::kGetAndNotifier;
    }
    if (mode == "set_and_get")
    {
        return TestMode::kSetAndGet;
    }
    if (mode == "set_get_and_notifier")
    {
        return TestMode::kSetAndGetAndNotifier;
    }
    FailTest("Unsupported --mode value: ", mode);

    // Unreachable, but required to avoid compiler warning: "error: non-void function
    // does not return a value in all control paths [-Werror,-Wreturn-type]"
    return TestMode::kNotifier;
}

}  // namespace

TestConfig ParseConfig(int argc, const char** argv)
{
    constexpr auto kModeArg = "mode";
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";

    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kModeArg, "Test mode: notifier, set_and_set, get, get_and_notifier, set_and_get, set_get_and_notifier"},
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };

    const auto args = ParseCommandLineArguments(argc, argv, parameter_description_pairs);

    const auto mode_result = GetValueIfProvided<std::string>(args, kModeArg);
    if (!mode_result.has_value())
    {
        FailTest("Missing or invalid --", kModeArg, " argument");
    }

    const auto mode = ParseTestMode(mode_result.value());

    const auto manifest_result = GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!manifest_result.has_value())
    {
        FailTest("Missing or invalid --", kServiceInstanceManifestArg, " argument");
    }

    return TestConfig{mode, manifest_result.value()};
}

}  // namespace score::mw::com::test
