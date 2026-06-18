/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/test/skeleton_move_semantics/test_parameters.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"

namespace score::mw::com::test
{

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
{
    auto args = ParseCommandLineArguments(
        argc, argv, {{kScenario, ""}, {kNumberOfSamplesToSend, ""}, {kServiceInstanceManifest, ""}});

    const auto scenario_index = GetValue<std::size_t>(args, kScenario);
    if (scenario_index >= static_cast<std::size_t>(SkeletonMoveScenario::kNumberOfScenarios))
    {
        FailTest("Consumer: ",
                 kScenario,
                 " value ",
                 scenario_index,
                 " is out of range. Valid values are between 0 and ",
                 static_cast<std::uint8_t>(SkeletonMoveScenario::kNumberOfScenarios) - 1,
                 ".");
    }
    const auto scenario = static_cast<SkeletonMoveScenario>(scenario_index);

    const auto num_samples_to_send = GetValue<std::size_t>(args, kNumberOfSamplesToSend);
    if (num_samples_to_send == 0U)
    {
        FailTest("Consumer: ", kNumberOfSamplesToSend, " value must be larger than 0.");
    }

    auto service_instance_manifest = GetValue<std::string>(args, kServiceInstanceManifest);

    return {scenario, num_samples_to_send, service_instance_manifest};
}

}  // namespace score::mw::com::test
