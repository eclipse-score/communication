/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "score/mw/com/test/partial_restart/provider_restart_max_subscribers/provider_restart_max_subscribers_application.h"

#include "score/mw/com/test/partial_restart/provider_restart_max_subscribers/consumer.h"
#include "score/mw/com/test/partial_restart/provider_restart_max_subscribers/provider.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include "score/mw/com/test/common_test_resources/timeout_supervisor.h"
#include <amp_optional.hpp>

#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace
{

    const auto kShmProviderCheckpointControlFileName = "provider_restart_application_provider_checkpoint_file";
    const auto kShmConsumerCheckpointControlFileName = "consumer_restart_application_provider_checkpoint_file";
    const amp::string_view kProviderCheckpointControlName = "Provider";
    const amp::string_view kConsumerCheckpointControlName = "Consumer";

    const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

    using CheckPointControl = bmw::mw::com::test::CheckPointControl;

    struct TestParameters
    {
        amp::optional<std::string> service_instance_manifest{};
        std::size_t number_test_iterations{};
        bool connected_proxy_during_restart{true};
    };

    amp::optional<TestParameters> ParseTestParameters(int argc, const char **argv) noexcept
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
        ("iterations,t", po::value<std::size_t>(&test_parameters.number_test_iterations)->default_value(5), "Number of cycles (provider restarts) to be done")
        ("is-proxy-connected-during-restart,c", po::value<bool>(&test_parameters.connected_proxy_during_restart), "Whether a proxy should be connected to the skeleton during skeleton restart.");
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

    int DoProviderRestart(amp::stop_token test_stop_token,
                          int argc,
                          const char **argv,
                          bool is_proxy_connected_during_restart) noexcept
    {
        // Resources that need to be cleaned up on process exit
        bmw::mw::com::test::ObjectCleanupGuard object_cleanup_guard{};

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
        auto consumer_checkpoint_control_guard_result = bmw::mw::com::test::CreateSharedCheckPointControl(
            "Controller Step (1)", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
        if (!(consumer_checkpoint_control_guard_result.has_value()))
        {
            return EXIT_FAILURE;
        }
        auto &consumer_checkpoint_control = consumer_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_control_guard_result.value());

        auto fork_consumer_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (1)",
            "Consumer",
            [&consumer_checkpoint_control, is_proxy_connected_during_restart, test_stop_token, argc, argv]()
            {
                const std::size_t max_number_subscribers{3U};
                bmw::mw::com::test::ConsumerParameters consumer_paramaters{is_proxy_connected_during_restart,
                                                                           max_number_subscribers};

                bmw::mw::com::test::ConsumerActions consumer_actions{
                    consumer_checkpoint_control, test_stop_token, argc, argv, consumer_paramaters};
                consumer_actions.DoConsumerActions();
            });
        if (!fork_consumer_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (1) failed, exiting." << std::endl;
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
        auto provider_checkpoint_control_guard_result = bmw::mw::com::test::CreateSharedCheckPointControl(
            "Controller Step (2)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
        if (!(provider_checkpoint_control_guard_result.has_value()))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        auto &provider_checkpoint_control = provider_checkpoint_control_guard_result.value().GetObject();
        object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_control_guard_result.value());

        auto fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (2)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            std::cerr << "Controller: Step (2) failed, exiting." << std::endl;
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }
        object_cleanup_guard.AddForkProviderGuard(fork_provider_pid_guard.value());

        bmw::mw::com::test::TimeoutSupervisor timeout_supervisor{};

        // ********************************************************************************
        // Step (3) - Wait for consumer to reach checkpoint (1) [Step (C.9)] - Proxies have been created and subscriptions
        //            done. If is_proxy_connected_during_restart is not set, proxies will be destroyed. Consumer is now
        //            waiting for proceed trigger from controller [Step (C.10)].
        // ********************************************************************************
        std::cout << "Controller Step (3): Waiting for consumer to reach checkpoint 1" << std::endl;
        const auto consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (3)", consumer_notification_happened, consumer_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (4) - Trigger provider to proceed to next checkpoint (provider will call
        //            StopOffer now and wait for finish trigger [Step (P.6)])
        // ********************************************************************************
        std::cout << "Controller Step (4): Triggered provider to proceed to next checkpoint" << std::endl;
        provider_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (5) - Wait for provider to reach checkpoint (1) [Step (P.5)] - StopOffer has been
        //            successfully called.
        // ********************************************************************************
        std::cout << "Controller Step (5): Waiting for provider to reach checkpoint 1" << std::endl;
        auto provider_notification_happened = provider_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint("Controller: Step (5)", provider_notification_happened, provider_checkpoint_control, 1))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (6) - Trigger provider to finish (provider will die gracefully now)
        // ********************************************************************************
        std::cout << "Controller Step (6): Triggered provider to finish" << std::endl;
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (7) - Wait for provider process to finish
        // ********************************************************************************
        std::cout << "Controller Step (7): Waiting for provider to finish" << std::endl;
        const auto provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (7)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (8) - Trigger consumer to proceed to next checkpoint (consumer will call DoConsumerActionsAfterRestart() and
        //            will wait for FindServiceHandler to be called indicating the the service has been re-offered. If
        //            is_proxy_connected_during_restart is set, will wait for existing proxies to reconnect (events toggling
        //            from subscription-pending to subscribed). Otherwise, will create proxies and subscribe.
        // ********************************************************************************
        std::cout << "Controller Step (8): Trigger consumer to proceed to next checkpoint" << std::endl;
        consumer_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (9) - (Re)Fork the Provider process
        // ********************************************************************************
        std::cout << "Controller Step (9): Re-forking provider process" << std::endl;
        fork_provider_pid_guard = bmw::mw::com::test::ForkProcessAndRunInChildProcess(
            "Controller Step (9)", "Provider", [&provider_checkpoint_control, test_stop_token, argc, argv]()
            { bmw::mw::com::test::DoProviderActions(provider_checkpoint_control, test_stop_token, argc, argv); });
        if (!fork_provider_pid_guard.has_value())
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (10) - Wait for consumer to reach checkpoint (2) [Step (C.19)] - Subscription checks have been done.
        //             Consumer is now waiting for finish trigger from controller [Step (C.20)].
        // ********************************************************************************
        std::cout << "Controller: Waiting for consumer to reach checkpoint 2" << std::endl;
        const auto second_consumer_notification_happened = consumer_checkpoint_control.WaitForCheckpointReachedOrError(
            kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
        if (!VerifyCheckpoint(
                "Controller: Step (10)", second_consumer_notification_happened, consumer_checkpoint_control, 2))
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (11) - Trigger provider to proceed to next checkpoint (provider will call
        //             StopOffer now)
        // ********************************************************************************
        std::cout << "Controller Step (11): Trigger provider to proceed to next checkpoint" << std::endl;
        provider_checkpoint_control.ProceedToNextCheckpoint();

        // ********************************************************************************
        // Step (12) - Trigger provider to finish (provider will die gracefully now)
        // ********************************************************************************
        std::cout << "Controller Step (12): Trigger provider to finish" << std::endl;
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (13) - Wait for provider process to terminate
        // ********************************************************************************
        const auto restarted_provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (13)", fork_provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!restarted_provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Step (14) - Trigger consumer to finish (provider will die gracefully now)
        // ********************************************************************************
        std::cout << "Controller Step (14): Trigger consumer to finish" << std::endl;
        consumer_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (15) - Wait for Consumer process to terminate
        // ********************************************************************************
        const auto consumer_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (15)", fork_consumer_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_terminated)
        {
            object_cleanup_guard.CleanUp();
            return EXIT_FAILURE;
        }

        object_cleanup_guard.CleanUp();
        return EXIT_SUCCESS;
    }

} // namespace

int main(int argc, const char **argv)
{
    // Prerequisites for the test steps/sequence
    amp::stop_source test_stop_source{};
    const bool sig_term_handler_setup_success = bmw::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
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
    const auto &test_parameters = test_parameters_result.value();
    amp::set_assertion_handler(&bmw::mw::com::test::assertion_stdout_handler);
    const int mw_com_argc = test_parameters.service_instance_manifest.has_value() ? argc : 0;
    const char **mw_com_argv = test_parameters.service_instance_manifest.has_value() ? argv : nullptr;

    for (std::size_t test_iteration = 1U; test_iteration <= test_parameters.number_test_iterations; ++test_iteration)
    {
        std::cerr << "Test Main: Running iteration " << test_iteration << " of "
                  << test_parameters.number_test_iterations << " of Provider-Restart-Test" << std::endl;

        const auto test_result = DoProviderRestart(
            test_stop_source.get_token(), mw_com_argc, mw_com_argv, test_parameters.connected_proxy_during_restart);
        if (test_result != EXIT_SUCCESS)
        {
            std::cerr << "Test Main: Iteration " << test_iteration << " of " << test_parameters.number_test_iterations
                      << " of Provider-Restart-Test failed. Skipping any further iteration." << std::endl;
            return test_result;
        }
    }
    return EXIT_SUCCESS;
}
