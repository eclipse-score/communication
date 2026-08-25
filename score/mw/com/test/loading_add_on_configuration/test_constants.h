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

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::test
{

constexpr const char* const kRegularServiceInstanceSpecifierString = "score/data/DataService";
constexpr const char* const kAddOnServiceInstanceSpecifierString = "score/data/SimpleDataService";
const std::string kConsumerDoneShmPath{"/consumer_done"};
const std::string kProviderReadyShmPath{"/provider_ready"};
const auto kRegularServiceInstanceSpecifier =
    InstanceSpecifier::Create(std::string{kRegularServiceInstanceSpecifierString}).value();
const auto kAddOnServiceInstanceSpecifier =
    InstanceSpecifier::Create(std::string{kAddOnServiceInstanceSpecifierString}).value();

constexpr std::size_t kTotalNumValuesToSend = 10U;
constexpr std::uint32_t kCycleTimeMs = 50;

const std::vector<std::uint32_t> kFirstServiceSamples = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const std::vector<std::uint32_t> kFirstServiceSamplesSecondCall = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const std::vector<std::uint32_t> kAddonServiceSamples = {51, 52, 53, 54, 55, 56, 57, 58, 59, 60};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_TEST_CONSTANTS_H
