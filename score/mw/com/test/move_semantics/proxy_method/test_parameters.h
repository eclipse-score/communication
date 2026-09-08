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
#ifndef SCORE_MW_COM_TEST_PROXY_METHOD_MOVE_SEMANTICS_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_PROXY_METHOD_MOVE_SEMANTICS_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{

const std::string kScenario{"scenario"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

constexpr std::int32_t kTestValueA{42};
constexpr std::int32_t kTestValueB{17};

const InstanceSpecifier kInstanceSpecifierMovedTo =
    InstanceSpecifier::Create(std::string{"test/proxy_method_move_semantics/MoveMethodInterfaceMovedTo"}).value();
const InstanceSpecifier kInstanceSpecifierMovedFrom =
    InstanceSpecifier::Create(std::string{"test/proxy_method_move_semantics/MoveMethodInterfaceMovedFrom"}).value();

enum class ProxyMoveScenario : std::uint8_t
{
    kMoveConstructAfterMethodCall,
    kMoveAssignAfterMethodCall,
    kNumberOfScenarios
};

struct CombinedTestConfiguration
{
    ProxyMoveScenario scenario;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PROXY_METHOD_MOVE_SEMANTICS_TEST_PARAMETERS_H
