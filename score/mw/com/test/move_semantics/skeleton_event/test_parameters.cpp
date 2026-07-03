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
#include "score/mw/com/test/move_semantics/skeleton_event/test_parameters.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"

namespace score::mw::com::test
{

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
{
    auto args = ParseCommandLineArguments(argc, argv, {{kScenario, ""}, {kServiceInstanceManifest, ""}});

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

    auto service_instance_manifest = GetValue<std::string>(args, kServiceInstanceManifest);

    return {scenario, service_instance_manifest};
}

std::size_t GetNumberOfSendIterations(const SkeletonMoveScenario scenario)
{
    if (scenario == SkeletonMoveScenario::kMoveConstructNotOffered)
    {
        // In this scenario, the provider will:
        //   - Create a skeleton
        //   - move construct the skeleton
        //   - offer the service
        //   - send samples
        //   - stop offering the service
        //   - offer the service again
        //   - send samples again
        // The receiver therefore expects two send iterations.
        return 2U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveConstructOffered)
    {
        // In this scenario, the provider will:
        //   - Create a skeleton
        //   - offer the service
        //   - send samples
        //   - move construct the skeleton while the service is offered
        //   - send samples
        //   - stop offering the service
        //   - offer the service again
        //   - send samples again
        // The receiver therefore expects three send iterations.
        return 3U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveAssignNotOffered)
    {
        // In this scenario, the provider will:
        //   - Create 2 skeletons
        //   - move assign the skeleton
        //   - offer the moved-to service
        //   - send samples
        //   - stop offering the service
        //   - offer the service again
        //   - send samples again
        // The receiver therefore expects two send iterations.
        return 2U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveAssignOffered)
    {
        // In this scenario, the provider will:
        //   - Create 2 skeletons
        //   - offer both services
        //   - send samples in both services
        //   - move assign the skeleton
        //   - send samples from moved-to service
        //   - stop offering the moved-to service
        //   - offer the moved-to service again
        //   - send samples again
        // The receiver connected to the moved-from service expects one send iteration. The receiver connected to
        // the moved-to service expects three send iterations.
        return 3U;
    }
    else
    {
        FailTest("Unknown scenario, cannot determine number of send iterations");
        return 0U;
    }
}

}  // namespace score::mw::com::test
