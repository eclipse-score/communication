/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#ifndef PLATFORM_AAS_MW_COM_TEST_SERVICE_DISCOVERY_SEARCH_AND_OFFER_TEST_DATATYPE_H
#define PLATFORM_AAS_MW_COM_TEST_SERVICE_DISCOVERY_SEARCH_AND_OFFER_TEST_DATATYPE_H

#include "score/mw/com/test/common_test_resources/test_interface.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                constexpr const char *kFileName = "/tmp/test_service_discovery_search_and_offer.txt";
                constexpr const char *const kInstanceSpecifierString = "test/service_discovery_search_and_offer";
                const std::int32_t kTestValue = 18;
                const std::int32_t kNumberOfOfferedServices = 1;

                using TestDataProxy = bmw::mw::com::AsProxy<TestInterface>;
                using TestDataSkeleton = bmw::mw::com::AsSkeleton<TestInterface>;

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_SERVICE_DISCOVERY_SEARCH_AND_OFFER_TEST_DATATYPE_H
