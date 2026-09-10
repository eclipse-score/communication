/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_TEST_CONSTANTS_H
#define SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_TEST_CONSTANTS_H

#include "score/mw/com/test/loading_add_on_configuration/types/addon_interface.h"

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::test
{

constexpr const char* const kRegularServiceInstanceSpecifierString = "score/data/DataService";
constexpr const char* const kAddOnServiceInstanceSpecifierString = "score/data/AddOnService";
const std::string kConsumerDoneShmPath{"/consumer_done"};
const std::string kProviderReadyShmPath{"/provider_ready"};
const std::string kAddonDiscoveryAttemptDoneShmPath{"/addon_discovery_attempt_done"};
const auto kRegularServiceInstanceSpecifier =
    InstanceSpecifier::Create(std::string{kRegularServiceInstanceSpecifierString}).value();
const auto kAddOnServiceInstanceSpecifier =
    InstanceSpecifier::Create(std::string{kAddOnServiceInstanceSpecifierString}).value();

constexpr std::size_t kTotalNumValuesToSend = 10U;
constexpr std::uint32_t kCycleTimeMs = 50;

const std::vector<std::uint32_t> kFirstServiceSamples = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const std::vector<std::uint32_t> kFirstServiceSamplesSecondCall = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const std::vector<score::mw::com::test::ExampleData> kAddonServiceSamples = {{51, 52, true},
                                                                             {53, 54, false},
                                                                             {55, 56, true},
                                                                             {57, 58, false},
                                                                             {59, 60, true},
                                                                             {61, 62, false},
                                                                             {63, 64, true},
                                                                             {65, 66, false},
                                                                             {67, 68, true},
                                                                             {69, 70, false}};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_TEST_CONSTANTS_H
