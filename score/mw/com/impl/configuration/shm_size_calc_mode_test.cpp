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
#include "score/mw/com/impl/configuration/shm_size_calc_mode.h"
#include <gtest/gtest.h>
#include <ostream>

namespace score::mw::com::impl
{
namespace
{

TEST(ShmSizeCalculationModeTest, OperatorStreamOutputsCorrectStringForkSimulation)
{
    // Given a ShmSizeCalculationMode set to kSimulation
    std::ostringstream oss;

    // When streaming  to ostringstream
    oss << ShmSizeCalculationMode::kSimulation;

    // Then the output should match "SIMULATION"
    EXPECT_EQ(oss.str(), "SIMULATION");
}

TEST(ShmSizeCalculationModeTest, OperatorStreamOutputsUnknownForInvalidValue)
{
    // Given a ShmSizeCalculationMode set to an invalid value
    std::ostringstream oss;
    auto invalid_value = static_cast<ShmSizeCalculationMode>(0xFF);

    // When streaming  to ostringstream
    oss << invalid_value;

    // Then the output should match "unknown"
    EXPECT_EQ(oss.str(), "(unknown)");
}

}  // namespace
}  // namespace score::mw::com::impl
