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

#ifndef SCORE_MW_COM_TEST_SERVICE_DISCOVERY_OFFER_AND_SEARCH_TEST_DATATYPE_H
#define SCORE_MW_COM_TEST_SERVICE_DISCOVERY_OFFER_AND_SEARCH_TEST_DATATYPE_H

#include "score/mw/com/test/common_test_resources/test_interface.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace score::mw::com::test
{
constexpr std::string_view kFileName = "/tmp/service_discovery_offer_and_search.txt";
constexpr std::string_view kInstanceSpecifierStringClient = "test/service_discovery_offer_and_search_client";
constexpr std::string_view kInstanceSpecifierStringServiceFirst =
    "test/service_discovery_offer_and_search_service_first";
constexpr std::string_view kInstanceSpecifierStringServiceSecond =
    "test/service_discovery_offer_and_search_service_second";
const std::int32_t kNumberOfOfferedServices = 2;
const std::int32_t kTestValue = 986;

using TestDataProxy = score::mw::com::AsProxy<TestInterface>;
using TestDataSkeleton = score::mw::com::AsSkeleton<TestInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SERVICE_DISCOVERY_OFFER_AND_SEARCH_TEST_DATATYPE_H
