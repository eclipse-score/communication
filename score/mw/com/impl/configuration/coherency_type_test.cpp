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
#include "score/mw/com/impl/configuration/coherency_type.h"
#include <gtest/gtest.h>
#include <ostream>

namespace score::mw::com::impl
{
namespace
{

TEST(CoherencyTypeTest, OperatorStreamOutputsCorrectStringForNonCoherent)
{
    // Given a CoherencyType set to kNonCoherent
    std::ostringstream oss;

    // When streaming to ostringstream
    oss << CoherencyType::kNonCoherent;

    // Then the output should match "NON_COHERENT"
    EXPECT_EQ(oss.str(), "NON_COHERENT");
}

TEST(CoherencyTypeTest, OperatorStreamOutputsCorrectStringForCoherent)
{
    // Given a CoherencyType set to kCoherent
    std::ostringstream oss;

    // When streaming to ostringstream
    oss << CoherencyType::kCoherent;

    // Then the output should match "COHERENT"
    EXPECT_EQ(oss.str(), "COHERENT");
}

TEST(CoherencyTypeTest, OperatorStreamOutputsCorrectStringForMixed)
{
    // Given a CoherencyType set to kMixed
    std::ostringstream oss;

    // When streaming to ostringstream
    oss << CoherencyType::kMixed;

    // Then the output should match "MIXED"
    EXPECT_EQ(oss.str(), "MIXED");
}

TEST(CoherencyTypeTest, OperatorStreamOutputsUnknownForInvalidValue)
{
    // Given a CoherencyType set to an invalid value
    std::ostringstream oss;
    auto invalid_value = static_cast<CoherencyType>(0xFF);

    // When streaming to ostringstream
    oss << invalid_value;

    // Then the output should match "(unknown)"
    EXPECT_EQ(oss.str(), "(unknown)");
}

}  // namespace
}  // namespace score::mw::com::impl
