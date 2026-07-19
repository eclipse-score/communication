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
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/fields/set_and_notifier/consumer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct ConsumerConfig
{
    std::string mode;
    std::string manifest;
};

ConsumerConfig ParseConfig(int argc, const char** argv)
{
    constexpr auto kModeArg = "mode";
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";

    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kModeArg, "Consumer mode: notifier or set"},
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };

    const auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, parameter_description_pairs);

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

    return ConsumerConfig{mode_result.value(), manifest_result.value()};
}

}  // namespace

int main(int argc, const char** argv)
{
    const auto config = ParseConfig(argc, argv);

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{config.manifest});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    score::mw::com::test::run_consumer(stop_source.get_token(), config.mode);
    return EXIT_SUCCESS;
}
