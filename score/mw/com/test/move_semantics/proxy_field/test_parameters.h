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
#ifndef SCORE_MW_COM_TEST_PROXY_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_PROXY_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

// Instance used by the MoveConstruct scenario (single instance: only the proxy handle moves, the channel stays the
// same), and, in the MoveAssign scenario, the target instance whose proxy is overwritten (destroyed) by the move
// assignment.
const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/proxy_field_move_semantics/MoveFieldInterfaceMovedTo"}).value();

// Only used in the MoveAssign scenario: the source instance whose proxy is move-assigned into the moved-to proxy.
// After the move, the moved-to proxy must reach this instance's channel (and therefore its Set-handler transform and
// values).
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/proxy_field_move_semantics/MoveFieldInterfaceMovedFrom"}).value();

// Initial field value for the "MovedTo" instance.
constexpr std::int32_t kInitialValueMovedTo{18};

// Initial field value for the "MovedFrom" instance (only used in the MoveAssign scenario).
constexpr std::int32_t kInitialValueMovedFrom{7};

// The consumer calls Set() once, after moving the proxy and subscribing, using kSetRequestValue.
constexpr std::int32_t kSetRequestValue{1234};

enum class ProxyMoveScenario : std::uint8_t
{
    kMoveConstructAfterCreate,
    kMoveAssignAfterCreate,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    ProxyMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

/// \brief Set-handler transform used by the "MovedTo" instance: value = (value * 2) + 1.
constexpr std::int32_t DoubleAndIncrement(const std::int32_t value) noexcept
{
    return (value * 2) + 1;
}

/// \brief Set-handler transform used by the "MovedFrom" instance: value = value + 100.
constexpr std::int32_t AddOneHundred(const std::int32_t value) noexcept
{
    return value + 100;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PROXY_FIELD_MOVE_SEMANTICS_TEST_PARAMETERS_H
