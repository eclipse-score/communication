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
#ifndef SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

constexpr std::int32_t kTestArgA{10};
constexpr std::int32_t kTestArgB{5};

// Upper bound (in milliseconds) for the provider's random pre-move sleep in "after offered" (fuzzy)
// scenarios.
constexpr std::chrono::milliseconds kMaxFuzzySleepDuration{500};

// Handler A is always registered first and always computes a + b, for all scenarios.
constexpr std::int32_t kFirstHandlerExpectedResult{kTestArgA + kTestArgB};

// The second handler (Handler B/C) always computes a * b, for all scenarios.
constexpr std::int32_t kSecondHandlerExpectedResult{kTestArgA * kTestArgB};

const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/move_semantics/skeleton_method/SkeletonMethodMoveInterfaceMovedTo"})
        .value();
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/move_semantics/skeleton_method/SkeletonMethodMoveInterfaceMovedFrom"})
        .value();

enum class SkeletonMoveScenario : std::uint8_t
{
    kMoveConstructBeforeOffered,
    kMoveConstructAfterOffered,
    kMoveAssignBeforeOffered,
    kMoveAssignAfterOffered,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    SkeletonMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_PARAMETERS_H
