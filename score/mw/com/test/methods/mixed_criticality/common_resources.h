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
#ifndef SCORE_MW_COM_TEST_METHODS_MIXED_CRITICALITY_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_METHODS_MIXED_CRITICALITY_COMMON_RESOURCES_H
/// This file contains information shared between the provider and consumer.

#include <string>
#include <vector>
namespace score::mw::com::test
{

const std::vector<std::vector<std::size_t>> kEnabledMethodIDs{std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U}};

const std::size_t kConfiguredNumberOfConsumers{kEnabledMethodIDs.size()};
auto GetServiceInstanceManifestPath(int argc, const char** argv) -> std::string;

}  // namespace score::mw::com::test
#endif  // SCORE_MW_COM_TEST_METHODS_MIXED_CRITICALITY_COMMON_RESOURCES_H
