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
#include "score/mw/com/impl/find_service_handle.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(FindServiceHandle, CanBeCopiedAndEqualCompared)
{
    RecordProperty("Verifies", "SCR-21789762");
    RecordProperty("Description", "Checks CopyAssignment operator and EqualComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_FindServiceHandle(1U);
    const auto unitCopy = unit;

    ASSERT_EQ(unit, unitCopy);
}

TEST(FindServiceHandle, LessCompareable)
{
    RecordProperty("Verifies", "SCR-21789762");
    RecordProperty("Description", "Checks LessComparableOperator");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto unit = make_FindServiceHandle(2U);
    const auto less = make_FindServiceHandle(1U);

    ASSERT_LT(less, unit);
}

TEST(FindServiceHandleView, GetUid)
{
    const auto unit = make_FindServiceHandle(42U);

    ASSERT_EQ(FindServiceHandleView{unit}.getUid(), 42U);
}

}  // namespace
}  // namespace score::mw::com::impl
