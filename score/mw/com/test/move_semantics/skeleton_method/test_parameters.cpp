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

bool IsAfterOffered(const SkeletonMoveScenario scenario)
{
    return (scenario == SkeletonMoveScenario::kMoveConstructAfterOffered) ||
           (scenario == SkeletonMoveScenario::kMoveAssignAfterOffered);
}

std::size_t GetNumberOfCallIterations(const SkeletonMoveScenario scenario)
{
    if (scenario == SkeletonMoveScenario::kMoveConstructBeforeOffered)
    {
        // In this scenario, the provider will:
        //   - Create Skeleton A
        //   - Register Handler A on Skeleton A
        //   - Move construct: Skeleton B = std::move(Skeleton A)  [before OfferService]
        //   - Offer Skeleton B
        //   - Wait for consumer to call Handler A via proxy
        // The consumer therefore expects one call iteration (Handler A: a + b = 15).
        return 1U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveConstructAfterOffered)
    {
        // In this scenario, the provider will:
        //   - Create Skeleton A
        //   - Register Handler A on Skeleton A
        //   - Offer Skeleton A
        //   - Wait for consumer proxy to be created (SHM now exists)
        //   - Move construct: Skeleton B = std::move(Skeleton A)  [after SHM exists]
        //   - Signal consumer: move done
        //   - Wait for consumer to call Handler A via proxy
        //   - Register Handler B on Skeleton B
        //   - Signal consumer: Handler B registered
        //   - Wait for consumer to call Handler B via proxy
        // The consumer therefore expects two call iterations:
        //   iter 0: Handler A (a + b = 15), iter 1: Handler B (a * b = 50).
        return 2U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveAssignBeforeOffered)
    {
        // In this scenario, the provider will:
        //   - Create Skeleton A (kMovedTo identity) and Skeleton B (kMovedFrom identity)
        //   - Register Handler A on Skeleton A
        //   - Move assign: Skeleton B = std::move(Skeleton A)  [before OfferService]
        //     Skeleton B now holds kMovedTo identity + Handler A
        //   - Offer Skeleton B
        //   - Wait for consumer to call Handler A via proxy
        // The consumer therefore expects one call iteration (Handler A: a + b = 15).
        return 1U;
    }
    else if (scenario == SkeletonMoveScenario::kMoveAssignAfterOffered)
    {
        // In this scenario, the provider will:
        //   - Create Skeleton A (kMovedTo identity) and Skeleton B (kMovedFrom identity)
        //   - Register Handler A on Skeleton A, Handler B on Skeleton B
        //   - Offer both skeletons
        //   - Wait for consumer to create both proxies (single notify)
        //   - Move assign: Skeleton B = std::move(Skeleton A)  [after SHM exists for both]
        //     Skeleton B now holds kMovedTo identity + Handler A
        //   - Signal consumer: move done
        //   - Wait for consumer to call Handler A via kMovedTo proxy
        //   - Register Handler C on Skeleton B (which internally stores Skeleton A's handle)
        //   - Signal consumer: Handler C registered
        //   - Wait for consumer to call Handler C via kMovedTo proxy
        // The consumer therefore expects two call iterations:
        //   iter 0: Handler A (a + b = 15), iter 1: Handler C (a - b = 5).
        return 2U;
    }

    FailTest("GetNumberOfCallIterations: Unknown scenario");
    return 0U;
}

std::int32_t GetExpectedResult(const SkeletonMoveScenario scenario, const std::size_t iteration)
{
    switch (scenario)
    {
        case SkeletonMoveScenario::kMoveConstructBeforeOffered:
        case SkeletonMoveScenario::kMoveAssignBeforeOffered:
            // Only 1 iteration, always Handler A (a + b)
            return kTestArgA + kTestArgB;

        case SkeletonMoveScenario::kMoveConstructAfterOffered:
            // iter 0: Handler A (a + b), iter 1: Handler B (a * b)
            return (iteration == 0U) ? kTestArgA + kTestArgB : kTestArgA * kTestArgB;

        case SkeletonMoveScenario::kMoveAssignAfterOffered:
            // iter 0: Handler A (a + b), iter 1: Handler C (a - b)
            return (iteration == 0U) ? kTestArgA + kTestArgB : kTestArgA - kTestArgB;

        case SkeletonMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("GetExpectedResult: Unknown scenario or iteration");
            return 0;
    }
}

}  // namespace score::mw::com::test
