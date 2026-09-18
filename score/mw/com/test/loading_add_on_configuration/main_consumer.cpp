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

#include "score/mw/com/test/loading_add_on_configuration/common_resources.h"
#include "score/mw/com/test/loading_add_on_configuration/consumer.h"
#include "score/mw/com/test/loading_add_on_configuration/test_constants.h"
#include "score/mw/com/test/loading_add_on_configuration/types/addon_interface.h"
#include "score/mw/com/test/loading_add_on_configuration/types/example_interface.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include "score/mw/com/runtime.h"

#include <cstdlib>
#include <iostream>

namespace
{

std::string ParseServiceInstanceManifest(int argc, const char** argv, std::string manifest_name)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{manifest_name, ""}});
    return score::mw::com::test::GetValue<std::string>(args, manifest_name);
}

// \brief Checks whether the "invalid-addon-only" flag was passed on the command line.
//
// When set, the process only attempts to merge the (intentionally) invalid add-on configuration and then exits,
// instead of running the full consumer sequence. This is used by a dedicated, separate process invocation of this
// binary to test that loading an invalid add-on configuration is rejected: merging is expected to make the runtime
// call std::terminate(), so the process is expected to be killed with SIGABRT rather than exit normally.
bool ParseInvalidAddonOnlyFlag(int argc, const char** argv)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"invalid-addon-only", ""}});
    const auto flag_result = score::mw::com::test::GetValueIfProvided<bool>(args, "invalid-addon-only");
    return flag_result.has_value() && flag_result.value();
}

// \brief Checks whether the "addon-no-merge" flag was passed on the command line.
//
// When set, the process does *not* merge the add-on configuration at all, and instead directly attempts to create a
// proxy for the add-on service instance specifier. Since the add-on service is unknown to this process' local
// configuration, service discovery is expected to fail and the process is expected to exit gracefully with a
// controlled failure (via FailTest(), i.e. EXIT_FAILURE), not crash. This is used to test that a consumer which was
// never updated with the add-on configuration cannot accidentally "see" a service instance it doesn't know about,
// while also not misbehaving/crashing.
bool ParseAddonNoMergeFlag(int argc, const char** argv)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"addon-no-merge", ""}});
    const auto flag_result = score::mw::com::test::GetValueIfProvided<bool>(args, "addon-no-merge");
    return flag_result.has_value() && flag_result.value();
}

int RunInvalidAddOnConfigTestCase(int argc, const char** argv)
{
    // Try to merge an invalid add-on configuration which should fail, because its service identifier is already
    // in use. The runtime is expected to call std::terminate().
    const auto invalid_service_instance_manifest_path =
        ParseServiceInstanceManifest(argc, argv, "invalid_addon_manifest");
    const auto invalid_add_on_load_result = score::mw::com::runtime::AddConfiguration(
        score::mw::com::runtime::RuntimeConfiguration{invalid_service_instance_manifest_path});

    if (invalid_add_on_load_result.has_value())
    {
        std::cerr << "Consumer: Could load invalid add-on configuration which should not be possible" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RunAddOnServiceSearchWithoutConfigMergeTestCase()
{
    // Attempting to find/create a proxy for the add-on service instance specifier is expected to fail gracefully
    // since this process' local configuration has no knowledge of that instance specifier, because the configuration
    // was never merged
    auto discovery_attempt_done_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kAddonDiscoveryAttemptDoneShmPath);
    if (!discovery_attempt_done_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: Could not create discovery attempt done ProcessSynchronizer");
    }
    auto& discovery_attempt_done_synchronizer = *discovery_attempt_done_synchronizer_result;

    // Notify the peer provider that the discovery attempt has concluded (whether it fails as expected via
    // FailTest(), or if it was successful), so add-on service does not have to be offered anymore.
    score::mw::com::test::ExitFunctionGuard discovery_attempt_done_guard{[&discovery_attempt_done_synchronizer]() {
        discovery_attempt_done_synchronizer.Notify();
    }};

    std::cout << "\nConsumer: Attempting to use add-on service without having merged its configuration" << std::endl;
    score::mw::com::test::ProxyContainer<score::mw::com::test::AddonInterfaceProxy> proxy_container{};
    proxy_container.CreateProxy(score::mw::com::test::kAddOnServiceInstanceSpecifier,
                                "Consumer: Unexpectedly able to find/create add-on proxy without merge:");
    // If we ever get here, discovery unexpectedly succeeded, which should not be possible.
    std::cerr << "Consumer: Could create add-on proxy without merging its configuration, which should not be "
                 "possible"
              << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main(int argc, const char** argv)
{
    const auto config = score::mw::com::test::ParseConfig(argc, argv);

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{config.config_file_path});

    if (ParseInvalidAddonOnlyFlag(argc, argv))
    {
        return RunInvalidAddOnConfigTestCase(argc, argv);
    }

    if (ParseAddonNoMergeFlag(argc, argv))
    {
        return RunAddOnServiceSearchWithoutConfigMergeTestCase();
    }

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    // Create the process synchronizers once so the same underlying shared memory object is reused across both
    // rounds of run_consumer()
    auto process_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    auto provider_ready_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kProviderReadyShmPath);
    if (!provider_ready_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Consumer: Could not create provider ready ProcessSynchronizer");
    }

    // 1st step: Run consumer with service defined in initial mw::com configuration
    score::mw::com::test::run_consumer<std::uint32_t, score::mw::com::test::ExampleInterfaceProxy>(
        stop_source.get_token(),
        score::mw::com::test::kRegularServiceInstanceSpecifier,
        *process_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kFirstServiceSamples);

    // 2nd step: Load add-on configuration and merge into existing configuration
    const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv, "addon_manifest");
    const auto add_on_load_result = score::mw::com::runtime::AddConfiguration(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    if (!add_on_load_result.has_value())
    {
        std::cout << "Sender: Failed to load add-on configuration: " << add_on_load_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    // 3rd step: Rerun consumer with initial service as in previous consumer run
    score::mw::com::test::run_consumer<std::uint32_t, score::mw::com::test::ExampleInterfaceProxy>(
        stop_source.get_token(),
        score::mw::com::test::kRegularServiceInstanceSpecifier,
        *process_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kFirstServiceSamplesSecondCall);

    // 4th step: Run consumer with new service instance as defined in add-on config
    score::mw::com::test::run_consumer<score::mw::com::test::ExampleData, score::mw::com::test::AddonInterfaceProxy>(
        stop_source.get_token(),
        score::mw::com::test::kAddOnServiceInstanceSpecifier,
        *process_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kAddonServiceSamples);

    return EXIT_SUCCESS;
}
