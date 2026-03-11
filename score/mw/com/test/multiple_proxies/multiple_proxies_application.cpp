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

#include "score/mw/com/test/multiple_proxies/multiple_proxies_application.h"

#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include <future>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::MODE, Parameters::NUM_CYCLES, Parameters::CYCLE_TIME};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto mode = run_parameters.GetMode();
    const auto num_cycles = run_parameters.GetNumCycles();
    const auto stop_token = test_runner.GetStopToken();

    score::mw::com::test::EventSenderReceiver event_sender_receiver;

    if (mode == "send" || mode == "skeleton")
    {
        const auto instance_specifier_result =
            score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped1"});
        if (!instance_specifier_result.has_value())
        {
            std::cerr << "Invalid instance specifier, terminating." << std::endl;

            return EXIT_FAILURE;
        }
        const auto& instance_specifier = instance_specifier_result.value();

        const auto cycle_time = run_parameters.GetCycleTime();
        event_sender_receiver.RunAsSkeleton(instance_specifier, cycle_time, num_cycles, stop_token);

        return EXIT_SUCCESS;
    }
    else if (mode == "recv" || mode == "proxy")
    {
        const std::vector<std::string> instance_names{
            "score/cp60/MapApiLanesStamped1", "score/cp60/MapApiLanesStamped2", "score/cp60/MapApiLanesStamped3"};
        std::vector<std::future<int>> future_return_values{};
        const auto cycle_time = run_parameters.GetOptionalCycleTime();

        // Launch threads which each contain a proxy instance and receive messages from the sender.
        for (const auto& instance_name : instance_names)
        {
            const auto instance_specifier_result = score::mw::com::InstanceSpecifier::Create(std::string{instance_name});
            if (!instance_specifier_result.has_value())
            {
                std::cerr << "Invalid instance specifier, terminating." << std::endl;

                return EXIT_FAILURE;
            }
            const auto& instance_specifier = instance_specifier_result.value();

            auto future_return_value =
                std::async(std::launch::async,
                           &score::mw::com::test::EventSenderReceiver::RunAsProxy<
                               score::mw::com::test::BigDataProxy,
                               score::mw::com::impl::ProxyEvent<score::mw::com::test::MapApiLanesStamped>>,
                           &event_sender_receiver,
                           instance_specifier,
                           cycle_time,
                           num_cycles,
                           std::cref(stop_token),
                           false);
            future_return_values.push_back(std::move(future_return_value));
        }

        return score::mw::com::test::SctfTestRunner::WaitForAsyncTestResults(future_return_values);
    }
    else
    {
        std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }
}
