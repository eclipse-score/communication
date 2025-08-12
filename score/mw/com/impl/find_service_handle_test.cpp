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

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include "score/mw/log/slot_handle.h"
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::Return;

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

TEST(FindServiceHandle, StreamOperatorOutputsUid)
{
    // Given a FindServiceHandle
    const auto unit = make_FindServiceHandle(2U);

    // When calling the stream operator
    std::stringstream buffer{};
    buffer << unit;

    // Then the output should contain the underlying Uid
    ASSERT_EQ(buffer.str(), "2");
}

TEST(FindServiceHandle, LogStreamOperatorOutputsUid)
{
    const std::size_t uid{2U};

    // Given a mocked LogRecorder
    mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);

    // Expecting that StartRecord and StopRecord will be called with a SlotHandle containing the uid of the
    // FindServiceHandle to be logged
    mw::log::SlotHandle handle{uid};
    EXPECT_CALL(recorder_mock, StartRecord(_, mw::log::LogLevel::kVerbose)).WillOnce(Return(handle));
    EXPECT_CALL(recorder_mock, StopRecord(handle)).Times(1);

    // Given a FindServiceHandle
    const auto unit = make_FindServiceHandle(uid);

    // When calling the stream operator
    score::mw::log::LogVerbose() << unit;
}

TEST(FindServiceHandleView, GetUid)
{
    const auto unit = make_FindServiceHandle(42U);

    ASSERT_EQ(FindServiceHandleView{unit}.getUid(), 42U);
}

}  // namespace
}  // namespace score::mw::com::impl
