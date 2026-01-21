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

#include "score/mw/com/test/service_discovery_during_consumer_crash/service_discovery_during_consumer_crash_application.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/service_discovery_during_consumer_crash/consumer.h"
#include "score/mw/com/test/service_discovery_during_consumer_crash/provider.h"

#include <score/optional.hpp>
#include <score/stop_token.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

namespace
{
const std::string_view kShmProviderCheckpointControlFileName =
    "service_discovery_during_consumer_crash_application_application_provider_checkpoint_file";
const std::string_view kShmConsumerCheckpointControlFileName =
    "service_discovery_during_consumer_crash_application_application_consumer_checkpoint_file";
const std::string_view kProviderCheckpointControlName = "Provider";
const std::string_view kConsumerCheckpointControlName = "Consumer";

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

/// \brief Test parameters for the ITF test.
struct TestParameters
{
    score::cpp::optional<std::string> service_instance_manifest{};
    std::size_t number_test_iterations{};
};

score::cpp::optional<TestParameters> ParseTestParameters(int argc, const char** argv) noexcept
{
    namespace po = boost::program_options;

    TestParameters test_parameters{};

    // Reading in cmd-line params
    po::options_description options;

    std::string service_instance_manifest{};

    // clang-format off
    options.add_options()
        ("help", "Display the help message")
        ("service_instance_manifest", po::value<std::string>(&service_instance_manifest)->default_value(""), "Path to the com configuration file")
        ("iterations,t", po::value<std::size_t>(&test_parameters.number_test_iterations), "Number of cycles (provider restarts) to be done");
    // clang-format on
    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << std::endl;
        return {};
    }

    po::notify(args);

    if (!service_instance_manifest.empty())
    {
        test_parameters.service_instance_manifest = service_instance_manifest;
    }
    return test_parameters;
}

int DoConsumerRestart(const score::cpp::stop_token& test_stop_token, int argc, const char** argv) noexcept
{
    // Resources that need to be cleaned up on process exit
    score::mw::com::test::ObjectCleanupGuard object_cleanup_guard{};

    // ********************************************************************************
    // Begin of test steps/sequence.
    // These are now the test steps, which the Controller (our main) does.
    // ********************************************************************************

    // ********************************************************************************
    // Step (1) - Fork consumer process and set up checkpoint-communication-objects in
    //            controller and consumer process to be able to communicate between
    //            them.
    // ********************************************************************************

    // Note. We cannot use a SharedMemoryObjectGuard with RAII semantics because after forking the process, a duplicate
    // would be made leading to a double destruction. Therefore, we have to manually clean up the resource before
    // exiting.
    auto consumer_checkpoint_control_guard_result = score::mw::com::test::CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_guard_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)", "Consumer", [&consumer_checkpoint_control, &test_stop_token, argc, argv]() {
            score::mw::com::test::DoConsumerActionsFirstTime(consumer_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (2) - Fork provider process and set up checkpoint-communication-objects in
    //            controller and provider process be able to communicate between them.
    // ********************************************************************************

    // Create the checkpoint-communication-objects/shared-memory object and let the controller be the "owner" as
    // this checkpoint-communication-objects will be re-used later, if the provider process gets re-forked.
    auto provider_checkpoint_control_guard_result = score::mw::com::test::CreateSharedCheckPointControl(
        "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!(provider_checkpoint_control_guard_result.has_value()))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

    auto fork_provider_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]() {
            score::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv);
        });
    if (!fork_provider_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

    score::mw::com::test::TimeoutSupervisor timeout_supervisor{};

    // ********************************************************************************
    // Step (3) - Wait for consumer to reach checkpoint (1) [Step (C.1)] - StartFindService can be called.
    //            Then wait for the provider to reach check point 1 - Skeleton has been created and is ready
    //            to offer a service.
    // ********************************************************************************
    std::cerr << "Controller Step (3): Waiting for consumer to reach checkpoint 1" << std::endl;
    const auto consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)", consumer_notification_happened, consumer_checkpoint_control, 1))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    std::cerr << "Controller Step (3): Waiting for provider to reach checkpoint 1" << std::endl;
    const auto provider_notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)", provider_notification_happened, provider_checkpoint_control, 1))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (4) - Signal the provider process to offer a service and the consumer to
    //              call StartFindService then wait a random amount of time.
    //              Kill the consumer.
    // ********************************************************************************

    std::cerr << "Controller Step (4): Signal Provider process to create a skeleton and start offer service."
              << std::endl;
    provider_checkpoint_control.ProceedToNextCheckpoint();
    consumer_checkpoint_control.ProceedToNextCheckpoint();

    std::cout << "Controller Step (4): Sleeping for a few ns." << std::endl;
    auto random_time = score::mw::com::test::get_random_time();
    std::this_thread::sleep_for(random_time);
    std::cerr << "Controller Step (4): Kill Consumer" << std::endl;

    if (!fork_consumer_pid_guard.value().KillChildProcess())
    {
        std::cerr << "Controller Step (4): Consumer could not be killed" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (5) - Refork the consumer
    // ********************************************************************************

    auto re_fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (5)", "Consumer", [&consumer_checkpoint_control, argc, argv]() {
            score::mw::com::test::DoConsumerActionsAfterRestart(consumer_checkpoint_control, argc, argv);
        });
    if (!re_fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(re_fork_consumer_pid_guard.value());

    // ********************************************************************************
    // Step (6) - Wait for the reforked consumer to reach its checkpoint.
    //            StartFindService finished and found a service.
    // ********************************************************************************

    std::cerr << "Controller Step (6): Waiting for reforked consumer to reach checkpoint 1" << std::endl;
    const auto reforked_consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint(
            "Controller: Step (6)", reforked_consumer_notification_happened, consumer_checkpoint_control, 1))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (7) - Trigger Provider to finish. Wait for the Provider and reforked consumer
    //            to finish process to finish. Clean up.
    // ********************************************************************************
    provider_checkpoint_control.FinishActions();

    const auto reforked_consumer_terminated = WaitForChildProcessToTerminate(
        "Controller: Step (7)", re_fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!reforked_consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    const auto provider_terminated = WaitForChildProcessToTerminate(
        "Controller: Step (7)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!provider_terminated)
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    object_cleanup_guard.CleanUp();
    return EXIT_SUCCESS;
}
}  // namespace

int main(int argc, const char** argv)
{
    // Prerequisites for the test steps/sequence
    score::cpp::stop_source test_stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Test main: Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing.\n";
    }

    const auto test_parameters_result = ParseTestParameters(argc, argv);
    if (!(test_parameters_result.has_value()))
    {
        std::cerr << "Test main: Could not parse test parameters, exiting." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& test_parameters = test_parameters_result.value();
    score::cpp::set_assertion_handler(&score::mw::com::test::assertion_stdout_handler);
    const int mw_com_argc = test_parameters.service_instance_manifest.has_value() ? argc : 0;
    const char** mw_com_argv = test_parameters.service_instance_manifest.has_value() ? argv : nullptr;

    for (std::size_t test_iteration = 1U; test_iteration <= test_parameters.number_test_iterations; ++test_iteration)
    {
        std::cerr << "Test Main: Running iteration " << test_iteration << " of "
                  << test_parameters.number_test_iterations << " of Consumer-Restart-Test" << std::endl;

        const auto test_result = DoConsumerRestart(test_stop_source.get_token(), mw_com_argc, mw_com_argv);
        if (test_result != EXIT_SUCCESS)
        {
            std::cerr << "Test Main: Iteration " << test_iteration << " of " << test_parameters.number_test_iterations
                      << " of Provider-Restart-Test failed. Skipping any further iteration." << std::endl;
            std::cerr << "~~~~~~~~~~~~~~~~~~~~~~~~~ CONTROLLER FAILED in it: " << test_iteration
                      << " !!! ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~n" << std::endl;
            return test_result;
        }
        std::cerr << "========================= CONTROLLER FINISHED SUCCESSFULLY in it: " << test_iteration
                  << " !!! ============================\n"
                  << std::endl;
    }

    return EXIT_SUCCESS;
}
