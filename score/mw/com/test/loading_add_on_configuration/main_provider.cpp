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

#include "score/mw/com/test/loading_add_on_configuration/test_constants.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/loading_add_on_configuration/common_resources.h"
#include "score/mw/com/test/loading_add_on_configuration/provider.h"

#include <cstdlib>
#include <iostream>

namespace
{

std::string ParseServiceInstanceManifest(int argc, const char** argv)
{
    const std::string service_instance_manifest_name = "addon_manifest";

    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{service_instance_manifest_name, ""}});
    return score::mw::com::test::GetValue<std::string>(args, service_instance_manifest_name);
}

}  // namespace

int main(int argc, const char** argv)
{

    const auto config = score::mw::com::test::ParseConfig(argc, argv);

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{config.config_file_path});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    // Create the process synchronizers once so the same underlying shared memory object is reused across both
    // rounds of run_provider()
    auto done_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Provider: Could not create done ProcessSynchronizer");
    }
    auto provider_ready_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kProviderReadyShmPath);
    if (!provider_ready_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Provider: Could not create provider ready ProcessSynchronizer");
    }

    // 1st step: Run provider with service instance defined in initial mw::com config
    score::mw::com::test::run_provider(stop_source.get_token(),
                                       score::mw::com::test::kRegularServiceInstanceSpecifier,
                                       *done_synchronizer_result,
                                       *provider_ready_synchronizer_result,
                                       score::mw::com::test::kFirstServiceSamples);

    // 2nd step: Load add-on configuration and merge into existing configuration
    const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv);
    const auto add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    if (!add_on_load_result.has_value())
    {
        std::cout << "Sender: Failed to load add-on configuration: " << add_on_load_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // 3rd step: Rerun provider with initial service as in previous provider run
    score::mw::com::test::run_provider(stop_source.get_token(),
                                       score::mw::com::test::kRegularServiceInstanceSpecifier,
                                       *done_synchronizer_result,
                                       *provider_ready_synchronizer_result,
                                       score::mw::com::test::kFirstServiceSamplesSecondCall);

    // 4th step: Run provider with new service instance defined in add-on config
    score::mw::com::test::run_provider(stop_source.get_token(),
                                       score::mw::com::test::kAddOnServiceInstanceSpecifier,
                                       *done_synchronizer_result,
                                       *provider_ready_synchronizer_result,
                                       score::mw::com::test::kAddonServiceSamples);

    return EXIT_SUCCESS;
}
