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
#ifndef SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kNumberOfSamplesToSend{"number-of-samples-to-send"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

constexpr std::size_t kNumberOfSamplesToSendPerOffer{10U};

const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/skeleton_event_move_semantics/MoveEventInterfaceMovedTo"}).value();
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/skeleton_event_move_semantics/MoveEventInterfaceMovedFrom"}).value();

enum class SkeletonMoveScenario : std::uint8_t
{
    kMoveConstructNotOffered,
    kMoveConstructOffered,
    kMoveAssignNotOffered,
    kMoveAssignOffered,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    SkeletonMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

/// \brief Determines the number of times the provider will send n samples based on the test scenario.
///
/// The number of samples expected to be received by the consumer is equal to number_of_samples_to_send_per_offer *
/// number_of_send_iterations.
std::size_t GetNumberOfSendIterations(SkeletonMoveScenario scenario);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_PARAMETERS_H
