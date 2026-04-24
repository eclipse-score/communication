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
#include "score/mw/com/impl/method_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(MethodTypeToStringTest, ReturnsMethodForKMethod)
{
    // Given a MethodType of kMethod
    // When converting to string
    // Then the result is "Method"
    EXPECT_EQ(to_string(MethodType::kMethod), "Method");
}

TEST(MethodTypeToStringTest, ReturnsGetForKGet)
{
    // Given a MethodType of kGet
    // When converting to string
    // Then the result is "Get"
    EXPECT_EQ(to_string(MethodType::kGet), "Get");
}

TEST(MethodTypeToStringTest, ReturnsSetForKSet)
{
    // Given a MethodType of kSet
    // When converting to string
    // Then the result is "Set"
    EXPECT_EQ(to_string(MethodType::kSet), "Set");
}

TEST(MethodTypeToStringTest, ReturnsUnknownForKUnknown)
{
    // Given a MethodType of kUnknown
    // When converting to string
    // Then the result is "Unknown"
    EXPECT_EQ(to_string(MethodType::kUnknown), "Unknown");
}

TEST(MethodTypeToStringTest, ReturnsInvalidForOutOfRangeValue)
{
    // Given an out-of-range MethodType value
    const auto invalid_value = static_cast<MethodType>(255);

    // When converting to string
    // Then the result is "Invalid"
    EXPECT_EQ(to_string(invalid_value), "Invalid");
}

}  // namespace
}  // namespace score::mw::com::impl
