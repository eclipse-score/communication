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

#include "third_party/traceability/doc/sample_library/unit_2/bar.h"

#include <gtest/gtest.h>

TEST(Bar, AssertNumber)
{
    ::testing::Test::RecordProperty("lobster-tracing", "SampleLibrary.SampleRequirement");
    unit_2::Bar unit{std::make_unique<unit_1::Foo>()};

    EXPECT_TRUE(unit.AssertNumber());
}
