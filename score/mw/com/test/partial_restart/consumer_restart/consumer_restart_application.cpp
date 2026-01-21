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

#include "score/mw/com/test/partial_restart/consumer_restart/consumer_restart_application.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/consumer_restart/consumer.h"
#include "score/mw/com/test/partial_restart/consumer_restart/provider.h"

#include <score/optional.hpp>
#include <score/stop_token.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

const std::string_view kShmProviderCheckpointControlFileName = "consumer_restart_application_provider_checkpoint_file";
const std::string_view kShmConsumerCheckpointControlFileName = "consumer_restart_application_consumer_checkpoint_file";
const std::string_view kProviderCheckpointControlName = "Provider";
const std::string_view kConsumerCheckpointControlName = "Consumer";

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

/// \brief Test parameters for the ITF test variants for consumer restart.
/// \details Consult ../README.md
///          We have two variants for consumer restart ITF. This is reflected
///          in the test parameter kill_consumer:
///          ITF variant 5: Consumer graceful/normal restart -> kill_consumer{false}
///          ITF variant 6: Consumer kill/crash restart -> kill_consumer{true}
struct TestParameters
{
    score::cpp::optional<std::string> service_instance_manifest{};
    std::size_t number_test_iterations{};
    /// \brief shall the consumer be killed (true) or gracefully shutdown (false) before restart.
    bool kill_consumer{false};
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
        ("iterations,t", po::value<std::size_t>(&test_parameters.number_test_iterations), "Number of cycles (provider restarts) to be done")
        ("kill", po::value<bool>(&test_parameters.kill_consumer), "Shall the consumer get killed before restart or gracefully shutdown?");
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

    if (service_instance_manifest != "")
    {
        test_parameters.service_instance_manifest = service_instance_manifest;
    }
    return test_parameters;
}

int DoConsumerRestart(score::cpp::stop_token test_stop_token, int argc, const char** argv, bool kill_consumer) noexcept
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
    //            them. Consumer will wait for FindServiceHandler to be called, indicating that the service has been
    //            offered.
    // ********************************************************************************

    // Note. We cannot use a SharedMemoryObjectGuard with RAII semantics because after forking the process, a duplicate
    // would be made leading to a double destruction. Therefore, we have to manually clean up the resource before
    // exiting.
    auto consumer_checkpoint_control_shm_object_creator_result = score::mw::com::test::CreateSharedCheckPointControl(
        "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!(consumer_checkpoint_control_shm_object_creator_result.has_value()))
    {
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint_control = consumer_checkpoint_control_shm_object_creator_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(
        consumer_checkpoint_control_shm_object_creator_result.value());

    auto fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (1)",
        "Consumer",
        [&consumer_checkpoint_control, kill_consumer, test_stop_token, argc, argv]() {
            const score::mw::com::test::ConsumerParameters consumer_parameters{kill_consumer};

            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_parameters);
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
    // Step (3) - Wait for consumer to reach checkpoint (1) [Step (C.7)] - Proxy has been created and
    //            subscription done and at least N events were received from the provider. If kill_consumer is set,
    //            proxy will keep receiving events until killed. Otherwise, it will wait for the Controller to trigger
    //            it to finish.
    // ********************************************************************************
    std::cout << "Controller Step (3): Waiting for consumer to reach checkpoint 1" << std::endl;
    const auto consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller: Step (3)", consumer_notification_happened, consumer_checkpoint_control, 1))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    if (kill_consumer)
    {
        // ********************************************************************************
        // Step (4.1) - Trigger consumer process to "wait for kill".
        // ********************************************************************************
        std::cout << "Controller Step (4.1): Trigger consumer to wait for kill" << std::endl;
        consumer_checkpoint_control.WaitForKill();
        while (!consumer_checkpoint_control.IsChildWaitingForKill())
        {
            const std::chrono::milliseconds poll_interval{100U};
            std::this_thread::sleep_for(poll_interval);
            std::cout << "Controller Step (4.1): Waiting for consumer to switch to 'wait for kill' state" << std::endl;
        }
        std::cout << "Controller Step (4.1): Kill consumer" << std::endl;
        const auto kill_result = fork_consumer_pid_guard.value().KillChildProcess();

        // ********************************************************************************
        // Step (4.2) - Wait for consumer process to terminate
        // ********************************************************************************
        if (!kill_result)
        {
            std::cerr << "Controller Step (4.2): Failed to kill consumer process" << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        else
        {
            std::cout << "Controller Step (4.2): Consumer process terminated" << std::endl;
            consumer_checkpoint_control.SetChildWaitingForKill(false);
        }
    }
    else
    {
        // ********************************************************************************
        // Step (4.3) - Trigger consumer to finish (consumer will terminate gracefully now)
        // ********************************************************************************
        std::cout << "Controller Step (4.3): Trigger consumer to finish" << std::endl;
        consumer_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (4.4) - Wait for consumer process to finish
        // ********************************************************************************
        std::cout << "Controller Step (4.4): Waiting for consumer to finish" << std::endl;
        const auto consumer_terminated = WaitForChildProcessToTerminate(
            "Controller Step (4.4)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
    }

    // ********************************************************************************
    // Step (5) - (Re)Fork the Consumer process which takes kill_consumer = false, so it will expect to terminate
    // gracefully.
    // ********************************************************************************
    std::cout << "Controller Step (5): Re-forking consumer process" << std::endl;
    fork_consumer_pid_guard = score::mw::com::test::ForkProcessAndRunInChildProcess(
        "Controller Step (5)", "Consumer", [&consumer_checkpoint_control, test_stop_token, argc, argv]() {
            const bool kill_consumer_after_restart{false};
            const score::mw::com::test::ConsumerParameters consumer_parameters{kill_consumer_after_restart};

            score::mw::com::test::DoConsumerActions(
                consumer_checkpoint_control, test_stop_token, argc, argv, consumer_parameters);
        });
    if (!fork_consumer_pid_guard.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (6) - Wait for consumer to reach checkpoint (1) [Step (C.7)] - Proxy has been created and
    //            subscription done and at least N events were received from the provider. Proxy will wait for the
    //            Controller to trigger it to finish.
    // ********************************************************************************
    std::cout << "Controller Step (6): Waiting for consumer to reach checkpoint 2" << std::endl;
    const auto second_consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Controller Step (6)", second_consumer_notification_happened, consumer_checkpoint_control, 1))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (7) - Trigger Consumer to finish (consumer will terminate gracefully now)
    // ********************************************************************************
    std::cout << "Controller Step (7): Trigger consumer to finish" << std::endl;
    consumer_checkpoint_control.FinishActions();

    // ********************************************************************************
    // Step (8) - Wait for Consumer process to finish
    // ********************************************************************************
    const auto restarted_consumer_terminated = WaitForChildProcessToTerminate(
        "Controller: Step (8)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!restarted_consumer_terminated)
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // ********************************************************************************
    // Step (9) - Trigger Provider to finish (provider will terminate gracefully now)
    // ********************************************************************************
    std::cout << "Controller Step (9): Trigger provider to finish" << std::endl;
    provider_checkpoint_control.FinishActions();

    // ********************************************************************************
    // Step (10) - Wait for Provider process to finish
    // ********************************************************************************
    const auto restarted_provider_terminated = WaitForChildProcessToTerminate(
        "Controller: Step (10)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
    if (!restarted_provider_terminated)
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

        const auto test_result =
            DoConsumerRestart(test_stop_source.get_token(), mw_com_argc, mw_com_argv, test_parameters.kill_consumer);
        if (test_result != EXIT_SUCCESS)
        {
            std::cerr << "Test Main: Iteration " << test_iteration << " of " << test_parameters.number_test_iterations
                      << " of Provider-Restart-Test failed. Skipping any further iteration." << std::endl;
            return test_result;
        }
    }

    return EXIT_SUCCESS;
}
