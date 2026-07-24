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

#include <cstdint>
#include <string>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

constexpr std::int32_t kTestArgA{10};
constexpr std::int32_t kTestArgB{5};

const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/move_semantics/skeleton_method/SkeletonMethodMoveInterfaceMovedTo"})
        .value();
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/move_semantics/skeleton_method/SkeletonMethodMoveInterfaceMovedFrom"})
        .value();

enum class SkeletonMoveScenario : std::uint8_t
{
    kMoveConstructBeforeOffered = 0,
    kMoveConstructAfterOffered = 1,
    kMoveAssignBeforeOffered = 2,
    kMoveAssignAfterOffered = 3,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    SkeletonMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

/// \brief Returns true for scenarios where the skeleton is already offered before the move occurs.
bool IsAfterOffered(SkeletonMoveScenario scenario);

/// \brief Returns the number of method call iterations the consumer performs for the given scenario.
std::size_t GetNumberOfCallIterations(SkeletonMoveScenario scenario);

/// \brief Returns the expected return value of moved_method_(kTestArgA, kTestArgB) for the given
///        scenario and call iteration index.
///        iter 0: Handler A (a+b = 15) for all scenarios
///        iter 1: Handler B (a*b = 50) for kMoveConstructAfterOffered
///                Handler C (a-b = 5)  for kMoveAssignAfterOffered
std::int32_t GetExpectedResult(SkeletonMoveScenario scenario, std::size_t iteration);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_PARAMETERS_H
