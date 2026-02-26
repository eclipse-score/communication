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

#include "score/mw/com/test/data_slots_read_only/data_slots_read_only_application.h"

#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/types.h"

#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

/**
 * Test that checks that trying to modify the shared memory data segment (i.e. where the data samples are stored should
 * fail (e.g. via a segfault))
 */
int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{
        Parameters::MODE, Parameters::NUM_CYCLES, Parameters::CYCLE_TIME, Parameters::SHOULD_MODIFY_DATA_SEGMENT};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto mode = run_parameters.GetMode();
    const auto num_cycles = run_parameters.GetNumCycles();
    const auto should_modify_data_segment = run_parameters.GetShouldModifyDataSegment();
    const auto stop_token = test_runner.GetStopToken();

    score::mw::com::test::EventSenderReceiver event_sender_receiver;

    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;

        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    if (mode == "send" || mode == "skeleton")
    {
        const auto cycle_time = run_parameters.GetCycleTime();
        event_sender_receiver.RunAsSkeleton(instance_specifier, cycle_time, num_cycles, stop_token);

        return EXIT_SUCCESS;
    }

    if (mode == "recv" || mode == "proxy")
    {
        const auto cycle_time = run_parameters.GetOptionalCycleTime();
        score::cpp::ignore = event_sender_receiver.RunAsProxy(
            instance_specifier, cycle_time, num_cycles, std::cref(stop_token), should_modify_data_segment);

        // If RunAsProxy doesn't crash when trying to write to the data segment, then the test should fail.
        return should_modify_data_segment ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
    return EXIT_FAILURE;
}
