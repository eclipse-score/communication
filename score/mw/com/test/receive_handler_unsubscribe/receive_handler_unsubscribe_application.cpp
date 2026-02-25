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

#include "score/mw/com/test/receive_handler_unsubscribe/receive_handler_unsubscribe_application.h"

#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include <future>
#include <utility>
#include <vector>

/**
 * Integration test to test code requirement SCR-6346501: "A receive handler that was set by a user shall be no longer
 * invoked after an unsubscribe."
 *
 * This test will create a skeleton and proxy within the same process. The skeleton will publish events and the proxy
 * will receive notifications via a callback registered with SetReceiveHandler(). When Unsubscribe is called on the
 * proxy side, it will be checked that the skeleton publishes at least one more event and the proxy does not call the
 * callback registered to that event.
 */
int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{
        Parameters::NUM_CYCLES, Parameters::CYCLE_TIME, Parameters::SERVICE_INSTANCE_MANIFEST};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto num_cycles = run_parameters.GetNumCycles();
    const auto stop_token = test_runner.GetStopToken();
    const auto cycle_time = run_parameters.GetCycleTime();

    score::mw::com::test::EventSenderReceiver event_sender_receiver;

    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;

        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    std::vector<std::future<int>> future_return_values;
    auto skeleton_return_value = std::async(std::launch::async,
                                            &score::mw::com::test::EventSenderReceiver::RunAsSkeleton,
                                            &event_sender_receiver,
                                            instance_specifier,
                                            cycle_time,
                                            num_cycles,
                                            std::cref(stop_token));
    future_return_values.push_back(std::move(skeleton_return_value));

    auto proxy_return_value = std::async(std::launch::async,
                                         &score::mw::com::test::EventSenderReceiver::RunAsProxyReceiveHandlerOnly,
                                         &event_sender_receiver,
                                         instance_specifier,
                                         std::cref(stop_token));
    future_return_values.push_back(std::move(proxy_return_value));

    // Wait for all threads to finish and check that they finished safely
    return score::mw::com::test::SctfTestRunner::WaitForAsyncTestResults(future_return_values);
}
