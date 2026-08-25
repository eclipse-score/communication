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
#include "score/mw/com/test/loading_add_on_configuration/provider.h"
#include "score/mw/com/test/loading_add_on_configuration/test_constants.h"
#include "score/mw/com/test/loading_add_on_configuration/types/addon_interface.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <cstdlib>
#include <functional>
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
// When set, the process only attempts to merge the invalid add-on configuration and then exits,
// instead of running the full provider sequence. This is used by a dedicated, separate process invocation of this
// binary to test that loading an invalid add-on configuration is rejected.
bool ParseInvalidAddonOnlyFlag(int argc, const char** argv)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"invalid-addon-only", ""}});
    const auto flag_result = score::mw::com::test::GetValueIfProvided<bool>(args, "invalid-addon-only");
    return flag_result.has_value() && flag_result.value();
}

// \brief Checks whether the "offer-addon-only" flag was passed on the command line.
//
// When set, the process merges the add-on configuration, offers *only* the add-on service (not the base service),
// and keeps it offered until the peer consumer signals that it has concluded its discovery attempt, before exiting.
// This is used together with a consumer that never merges the add-on
// configuration, to prove that such a consumer cannot discover the add-on service (rather than that no one is
// offering it at all).
bool ParseOfferAddonOnlyFlag(int argc, const char** argv)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"offer-addon-only", ""}});
    const auto flag_result = score::mw::com::test::GetValueIfProvided<bool>(args, "offer-addon-only");
    return flag_result.has_value() && flag_result.value();
}

// \brief Checks whether the "merge-during-stream" flag was passed on the command line.
//
// When set, the add-on configuration is merged synchronously in the middle of the 1st publish loop (i.e.
// while the initial service is actively streaming samples to a consumer), instead of merging it in between
// communication.
bool ParseMergeDuringStreamFlag(int argc, const char** argv)
{
    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"merge-during-stream", ""}});
    const auto flag_result = score::mw::com::test::GetValueIfProvided<bool>(args, "merge-during-stream");
    return flag_result.has_value() && flag_result.value();
}

int RunAddOnServiceOnlyTestCase(int argc, const char** argv)
{
    // Merge the (valid) add-on configuration and offer only the add-on service, keeping it offered until the peer
    // consumer (that never merged the add-on configuration) signals that it has concluded its (expected to fail)
    // service discovery attempt.
    const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv, "addon_manifest");
    const auto add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    if (!add_on_load_result.has_value())
    {
        std::cout << "Provider: Failed to load add-on configuration: " << add_on_load_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    auto discovery_attempt_done_synchronizer_result =
        score::mw::com::test::ProcessSynchronizer::Create(score::mw::com::test::kAddonDiscoveryAttemptDoneShmPath);
    if (!discovery_attempt_done_synchronizer_result.has_value())
    {
        score::mw::com::test::FailTest("Provider: Could not create discovery attempt done ProcessSynchronizer");
    }

    std::cout << "\nProvider - AddOn Only: Step 1 - Create skeleton" << std::endl;
    score::mw::com::test::SkeletonContainer<score::mw::com::test::AddonInterfaceSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(score::mw::com::test::kAddOnServiceInstanceSpecifier, "provider");

    std::cout << "\nProvider - AddOn Only: Step 2 - Offer add-on service" << std::endl;
    skeleton_container.OfferService("provider");

    // Keep the service offered until the consumer (which never merged the add-on configuration) signals that it
    // has concluded its service discovery attempt (expected to fail).
    std::cout << "\nProvider - AddOn Only: Step 3 - Wait for consumer's discovery attempt to conclude" << std::endl;
    if (!discovery_attempt_done_synchronizer_result->WaitWithAbort(stop_source.get_token()))
    {
        score::mw::com::test::FailTest(
            "Provider: WaitWithAbort (discovery attempt done) was stopped by stop_token instead of notification");
    }

    return EXIT_SUCCESS;
}

int RunInvalidAddOnConfigTestCase(int argc, const char** argv)
{
    // Try to merge an invalid add-on configuration which should fail, because its service identifier is already
    // in use. The runtime is expected to call std::terminate().
    const auto invalid_service_instance_manifest_path =
        ParseServiceInstanceManifest(argc, argv, "invalid_addon_manifest");
    const auto invalid_add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
        score::mw::com::runtime::RuntimeConfiguration{invalid_service_instance_manifest_path});

    if (invalid_add_on_load_result.has_value())
    {
        std::cout << "Provider: Could load invalid add-on configuration which should not be possible" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
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

    if (ParseOfferAddonOnlyFlag(argc, argv))
    {
        return RunAddOnServiceOnlyTestCase(argc, argv);
    }

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

    // 1st step: Run provider with service instance defined in initial mw::com config. If requested, merge the
    // add-on configuration while this service is offered and data actively streamed to consumer.
    const bool merge_during_stream = ParseMergeDuringStreamFlag(argc, argv);

    std::function<void()> mid_stream_callback{};
    if (merge_during_stream)
    {
        const auto addon_manifest_path = ParseServiceInstanceManifest(argc, argv, "addon_manifest");
        mid_stream_callback = [addon_manifest_path]() {
            const auto result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
                score::mw::com::runtime::RuntimeConfiguration{addon_manifest_path});
            if (!result.has_value())
            {
                std::cerr << "Provider: Failed to load add-on configuration mid-stream: " << result.error()
                          << std::endl;
            }
        };
    }

    score::mw::com::test::run_provider<std::uint32_t, score::mw::com::test::ExampleInterfaceSkeleton>(
        stop_source.get_token(),
        score::mw::com::test::kRegularServiceInstanceSpecifier,
        *done_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kFirstServiceSamples,
        mid_stream_callback);

    // 2nd step: Load add-on configuration and merge into existing configuration (skipped if it was already merged
    // mid-stream in step 1 above).
    if (!merge_during_stream)
    {
        const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv, "addon_manifest");
        const auto add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
            score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

        if (!add_on_load_result.has_value())
        {
            std::cerr << "Provider: Failed to load add-on configuration: " << add_on_load_result.error() << std::endl;
            return EXIT_FAILURE;
        }
    }

    // 3rd step: Rerun provider with initial service as in previous provider run
    score::mw::com::test::run_provider<std::uint32_t, score::mw::com::test::ExampleInterfaceSkeleton>(
        stop_source.get_token(),
        score::mw::com::test::kRegularServiceInstanceSpecifier,
        *done_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kFirstServiceSamplesSecondCall);

    // 4th step: Run provider with new service instance defined in add-on config
    score::mw::com::test::run_provider<score::mw::com::test::ExampleData, score::mw::com::test::AddonInterfaceSkeleton>(
        stop_source.get_token(),
        score::mw::com::test::kAddOnServiceInstanceSpecifier,
        *done_synchronizer_result,
        *provider_ready_synchronizer_result,
        score::mw::com::test::kAddonServiceSamples);

    return EXIT_SUCCESS;
}
