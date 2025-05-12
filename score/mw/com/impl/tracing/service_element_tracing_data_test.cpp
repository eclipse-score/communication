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
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

TEST(ServiceElementTracingDataEqualityTest, PositiveComparison)
{
    const ServiceElementTracingData data1{8, 1};
    const ServiceElementTracingData data2{8, 1};

    EXPECT_TRUE(data1 == data2);
}

TEST(ServiceElementTracingDataEqualityTest, NegativeComparisonInFirstElement)
{
    const ServiceElementTracingData data1{7, 4};
    const ServiceElementTracingData data2{8, 4};

    EXPECT_FALSE(data1 == data2);
}

TEST(ServiceElementTracingDataEqualityTest, NegativeComparisonInSecondElement)
{
    const ServiceElementTracingData data1{8, 3};
    const ServiceElementTracingData data2{8, 1};

    EXPECT_FALSE(data1 == data2);
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
