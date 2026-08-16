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
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

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
        FailTest("skeleton_method_move_semantics: ",
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

std::int32_t GetFirstHandlerExpectedResult(const SkeletonMoveScenario scenario)
{
    if (scenario >= SkeletonMoveScenario::kNumberOfScenarios)
    {
        FailTest("GetFirstHandlerExpectedResult: Unknown scenario");
        return 0;
    }
    // Handler A is always registered first and always computes a + b, for all scenarios.
    return kTestArgA + kTestArgB;
}

std::int32_t GetSecondHandlerExpectedResult(const SkeletonMoveScenario scenario)
{
    if (scenario >= SkeletonMoveScenario::kNumberOfScenarios)
    {
        FailTest("GetSecondHandlerExpectedResult: Unknown scenario");
        return 0;
    }
    // The second handler (Handler B/C) always computes a * b, for all scenarios.
    return kTestArgA * kTestArgB;
}

}  // namespace score::mw::com::test
