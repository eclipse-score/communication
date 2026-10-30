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
#ifndef SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

// The instance which ends up being offered and which the consumer always connects to, regardless of scenario.
// For move-construct scenarios, the skeleton created on this instance is (possibly) move-constructed but keeps this
// instance identity. For move-assign scenarios, this is the instance whose skeleton is the move-assign target
// (i.e. it survives, holding the moved-in state, and is then offered under this same instance identity).
const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/skeleton_field_move_semantics/MoveFieldInterfaceMovedTo"}).value();

// Only used in move-assign scenarios: the source instance whose skeleton is move-assigned away (never offered
// itself under this identity).
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/skeleton_field_move_semantics/MoveFieldInterfaceMovedFrom"}).value();

constexpr std::int32_t kInitialValue{18};
constexpr std::int32_t kSetRequestValue{1234};
const std::vector<std::int32_t> kValuesToSend{20, 30, 35};
constexpr std::size_t kTotalNumValuesToSend{4U};

// Bounded random-delay window used by the "after offer" (fuzzy) scenarios to sleep a
// single random delay before performing the move operation.
constexpr std::chrono::microseconds kSequenceRaceWindowUs{100};

enum class SkeletonFieldMoveScenario : std::uint8_t
{
    kMoveConstructBeforeOffer,
    kMoveConstructAfterOffer,
    kMoveAssignBeforeOffer,
    kMoveAssignAfterOffer,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    SkeletonFieldMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

/// \brief Returns true if the scenario performs a move-assign (as opposed to a move-construct).
bool IsMoveAssignScenario(SkeletonFieldMoveScenario scenario);

/// \brief Returns true if the scenario performs the move while the service is already offered (i.e. races the
/// consumer's live API calls), as opposed to moving before offering the service.
bool IsAfterOfferScenario(SkeletonFieldMoveScenario scenario);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H
