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

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/proxy_restart_shall_not_affect_other_proxies/consumer.h"
#include "score/mw/com/test/partial_restart/proxy_restart_shall_not_affect_other_proxies/provider.h"

#include <boost/program_options.hpp>

#include <cassert>

namespace
{
    const auto kShmConsumer1CheckpointControlFileName = "consumer_1_checks_number_of_allocations_checkpoint_file";
    const auto kShmConsumer2CheckpointControlFileName = "consumer_2_checks_number_of_allocations_checkpoint_file";
    const auto kShmProviderCheckpointControlFileName = "provider_checks_number_of_allocations_checkpoint_file";
    const amp::string_view kConsumer1CheckpointControlName = "Consumer_1";
    const amp::string_view kConsumer2CheckpointControlName = "Consumer_2";
    const amp::string_view kProviderCheckpointControlName = "Skeleton";
    const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};

    struct TestParameters
    {
        std::size_t number_consumer_restart{};
        bool kill_consumer{false};
    };

    using namespace bmw::mw::com::test;

    bool DoControllerActions(const TestParameters &test_parameters, amp::stop_token stop_token)
    {
        ObjectCleanupGuard object_cleanup_guard{};
        //*************************************************
        // Step (1)- Create check point for provider(p)
        //*************************************************
        auto Provider_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller Step (1)", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
        if (!(Provider_checkpoint_control_guard_result.has_value()))
        {
            std::cout << "Controller: Error creating SharedMemoryObjectGuard for Provider_checkpoint_control, exiting\n.";
            return false;
        }

        object_cleanup_guard.AddProviderCheckpointControlGuard(Provider_checkpoint_control_guard_result.value());
        auto &provider_checkpoint_control = Provider_checkpoint_control_guard_result.value().GetObject();

        //***************************************************
        // Step (2)- fork provider
        //***************************************************
        auto provider_pid_guard = ForkProcessAndRunInChildProcess(
            "Controller Step (2):", "Provider:", [&provider_checkpoint_control, &stop_token]()
            { PerformProviderActions(provider_checkpoint_control, stop_token); });
        if (!provider_pid_guard.has_value())
        {
            object_cleanup_guard.CleanUp();
            return false;
        }
        object_cleanup_guard.AddForkProviderGuard(provider_pid_guard.value());

        //***************************************************
        // Step (3)- Create check point for First consumer(c1)
        //***************************************************
        auto consumer_1_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller Step (3)", kShmConsumer1CheckpointControlFileName, kConsumer1CheckpointControlName);
        if (!(consumer_1_checkpoint_control_guard_result.has_value()))
        {
            std::cout << "Parent: Error creating SharedMemoryObjectGuard for Consumer_1_checkpoint_control, exiting\n.";
            object_cleanup_guard.CleanUp();
            return false;
        }
        object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_1_checkpoint_control_guard_result.value());
        auto &consumer_1_checkpoint_control = consumer_1_checkpoint_control_guard_result.value().GetObject();

        //***************************************************
        // Step (4)- fork c1
        //***************************************************
        auto consumer_1_pid_guard = ForkProcessAndRunInChildProcess(
            "Controller Step (4):", "Consumer:", [&consumer_1_checkpoint_control, &stop_token]()
            { PerformFirstConsumerActions(consumer_1_checkpoint_control, stop_token); });
        if (!consumer_1_pid_guard.has_value())
        {
            object_cleanup_guard.CleanUp();
            return false;
        }
        object_cleanup_guard.AddForkConsumerGuard(consumer_1_pid_guard.value());

        //***************************************************
        // Step (5)- wait till c1 reaches check point 1
        //***************************************************
        if ((WaitAndVerifyCheckPoint(
                 "Controller Step (5):", consumer_1_checkpoint_control, 1, stop_token, kMaxWaitTimeToReachCheckpoint) !=
             EXIT_SUCCESS))
        {
            object_cleanup_guard.CleanUp();
            return false;
        }

        //*****************************************************
        // Step (6)- Create check point for Second consumer(c2)
        //*****************************************************
        auto consumer_2_checkpoint_control_guard_result = CreateSharedCheckPointControl(
            "Controller Step (6)", kShmConsumer2CheckpointControlFileName, kConsumer2CheckpointControlName);
        if (!(consumer_2_checkpoint_control_guard_result.has_value()))
        {
            std::cout << "Controller Step (6): Error creating SharedMemoryObjectGuard for Consumer_2_checkpoint_control, "
                         "exiting\n.";
            object_cleanup_guard.CleanUp();
            return false;
        }
        object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_2_checkpoint_control_guard_result.value());
        auto &consumer_2_checkpoint_control = consumer_2_checkpoint_control_guard_result.value().GetObject();

        //***************************************************
        // Step (7)- fork c2
        //***************************************************
        auto consumer_2_pid_guard = ForkProcessAndRunInChildProcess(
            "Controller Step (7):", "Consumer 2:", [&consumer_2_checkpoint_control, &stop_token, &test_parameters]()
            { PerformSecondConsumerActions(
                  consumer_2_checkpoint_control, stop_token, test_parameters.number_consumer_restart + 1); });
        if (!consumer_2_pid_guard.has_value())
        {
            object_cleanup_guard.CleanUp();
            return false;
        }
        object_cleanup_guard.AddForkConsumerGuard(consumer_2_pid_guard.value());

        //***************************************************
        // Step (8)- wait till c2 reaches check point 1
        //***************************************************
        if ((WaitAndVerifyCheckPoint(
                 "Controller Step (8):", consumer_2_checkpoint_control, 1, stop_token, kMaxWaitTimeToReachCheckpoint) !=
             EXIT_SUCCESS))
        {
            object_cleanup_guard.CleanUp();
            return false;
        }

        assert(test_parameters.number_consumer_restart >= 1);
        for (std::size_t i = 0; i < test_parameters.number_consumer_restart; i++)
        {
            //***************************************************
            // Step (9)- in the case of normal restart c1 will finish execution.
            // In the case of crash restart c1 will be killed.
            //***************************************************
            if (test_parameters.kill_consumer)
            {
                std::cout << "Controller (Step 9): killing c1:" << consumer_1_pid_guard.value().GetPid() << " \n";
                auto kill_result = consumer_1_pid_guard.value().KillChildProcess();
                if (!kill_result)
                {
                    std::cerr << "Controller: Step (9) failed. Error killing provider child process" << std::endl;
                    object_cleanup_guard.CleanUp();
                    return false;
                }
            }
            else
            {
                std::cout << "Controller (Step 9): Let c1 finish execution\n";
                consumer_1_checkpoint_control.FinishActions();

                std::cout << "Controller Step (9): Waiting for first consumer to finish" << std::endl;
                const auto consumer_terminated = WaitForChildProcessToTerminate(
                    "Controller Step (9)", consumer_1_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
                if (!consumer_terminated)
                {
                    object_cleanup_guard.CleanUp();
                    return false;
                }
            }

            //***************************************************
            // Step (10)- fork c1
            //***************************************************
            consumer_1_pid_guard = ForkProcessAndRunInChildProcess(
                "Controller Step (11):", "Consumer 1:", [&consumer_1_checkpoint_control, &stop_token]()
                { PerformFirstConsumerActions(consumer_1_checkpoint_control, stop_token); });
            if (!consumer_1_pid_guard.has_value())
            {
                object_cleanup_guard.CleanUp();
                return false;
            }
            object_cleanup_guard.AddForkConsumerGuard(consumer_1_pid_guard.value());

            //***************************************************
            // Step (11)- wait till c1 reaches check point 1
            //***************************************************
            if ((WaitAndVerifyCheckPoint("Controller Step (11):",
                                         consumer_1_checkpoint_control,
                                         1,
                                         stop_token,
                                         kMaxWaitTimeToReachCheckpoint) != EXIT_SUCCESS))
            {
                object_cleanup_guard.CleanUp();
                return false;
            }

            //***************************************************
            // Step (12)- tell c2 to proceed
            //***************************************************
            std::cout << "Controller Step (12): tell consumer 2 to proceed\n";
            consumer_2_checkpoint_control.ProceedToNextCheckpoint();

            //***************************************************
            // Step (13)- wait till c2 reaches check point M. Where M ranges from 2 to a specific number.
            //***************************************************
            const std::uint8_t checkpoint_no{static_cast<std::uint8_t>(i + 2)};
            if ((WaitAndVerifyCheckPoint("Controller Step (13):",
                                         consumer_2_checkpoint_control,
                                         checkpoint_no,
                                         stop_token,
                                         kMaxWaitTimeToReachCheckpoint) != EXIT_SUCCESS))
            {
                object_cleanup_guard.CleanUp();
                return false;
            }
        }
        //***************************************************
        // Step (14)- Trigger p to finish
        //***************************************************
        std::cout << "Controller Step (14): Trigger provider to finish" << std::endl;
        provider_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (15) - Wait for p to finish
        // ********************************************************************************
        const auto provider_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (15)", provider_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!provider_terminated)
        {
            object_cleanup_guard.CleanUp();
            return false;
        }

        //***************************************************
        // Step (16)- Trigger c1 to finish
        //***************************************************
        std::cout << "Controller Step (16): Trigger first consumer to finish" << std::endl;
        consumer_1_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (17) - Wait for c1 to finish
        // ********************************************************************************
        const auto consumer_1_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (17)", consumer_1_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_1_terminated)
        {
            object_cleanup_guard.CleanUp();
            return false;
        }

        //***************************************************
        // Step (18)- Trigger c2 to finish
        //***************************************************
        std::cout << "Controller Step (18): Trigger provider to finish" << std::endl;
        consumer_2_checkpoint_control.FinishActions();

        // ********************************************************************************
        // Step (19) - Wait for c2 to finish
        // ********************************************************************************
        const auto consumer_2_terminated = WaitForChildProcessToTerminate(
            "Controller: Step (17)", consumer_2_pid_guard.value(), kMaxWaitTimeToReachCheckpoint);
        if (!consumer_2_terminated)
        {
            object_cleanup_guard.CleanUp();
            return false;
        }

        if (!object_cleanup_guard.CleanUp())
        {
            std::cerr << "Controller Step (19): cleanup failed\n";
            return false;
        }

        return true;
    }

    amp::optional<TestParameters> ParseTestParameters(int argc, const char **argv) noexcept
    {
        namespace po = boost::program_options;
        TestParameters test_parameters{};

        // Reading in cmd-line params
        po::options_description options;

        // clang-format off
    options.add_options()
        ("help", "Display the help message")
        ("number-consumer-restarts,n", po::value<std::size_t>(&test_parameters.number_consumer_restart)->required(), "Number of cycles (consumer restarts) to be done")
        ("kill", po::value<bool>(&test_parameters.kill_consumer)->required(), "Shall the consumer get killed before restart or gracefully shutdown?");
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
        return test_parameters;
    }

} // namespace

int main(int argc, const char **argv)
{
    // Prerequisites for the test steps/sequence
    amp::stop_source test_stop_source{};
    const bool sig_term_handler_setup_success = bmw::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cout << "Test main: Unable to set signal handler for SIGINT and/or SIGTERM.\n";
        return EXIT_FAILURE;
    }

    const auto test_parameters_result = ParseTestParameters(argc, argv);
    if (!(test_parameters_result.has_value()))
    {
        std::cerr << "Test main: Could not parse test parameters, exiting." << std::endl;
        return EXIT_FAILURE;
    }
    const auto &test_parameters = test_parameters_result.value();

    if (!DoControllerActions(test_parameters, test_stop_source.get_token()))
    {
        return EXIT_FAILURE;
    }

    return 0;
}
