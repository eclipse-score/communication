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
#include "score/mw/com/impl/bindings/lola/coherent_event_slot_status.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(CoherentEventSlotStatusTest, DefaultConstructedIsInvalid)
{
    const CoherentEventSlotStatus unit{};
    EXPECT_TRUE(unit.IsInvalid());
    EXPECT_EQ(unit.GetTimeStamp(), CoherentEventSlotStatus::INVALID_TIMESTAMP);
}

TEST(CoherentEventSlotStatusTest, ConstructedWithTimestampIsNotInvalid)
{
    const CoherentEventSlotStatus unit{CoherentEventSlotStatus::FIRST_VALID_TIMESTAMP};
    EXPECT_FALSE(unit.IsInvalid());
    EXPECT_EQ(unit.GetTimeStamp(), CoherentEventSlotStatus::FIRST_VALID_TIMESTAMP);
}

TEST(CoherentEventSlotStatusTest, SetTimeStampUpdatesTimeStamp)
{
    CoherentEventSlotStatus unit{};
    unit.SetTimeStamp(42U);
    EXPECT_EQ(unit.GetTimeStamp(), 42U);
    EXPECT_FALSE(unit.IsInvalid());
}

TEST(CoherentEventSlotStatusTest, MarkInvalidResetsToInvalid)
{
    CoherentEventSlotStatus unit{42U};
    unit.MarkInvalid();
    EXPECT_TRUE(unit.IsInvalid());
}

TEST(CoherentEventSlotStatusTest, OnlyContainsTheTimestamp)
{
    static_assert(sizeof(CoherentEventSlotStatus) == sizeof(CoherentEventSlotStatus::EventTimeStamp),
                  "CoherentEventSlotStatus shall only contain the 4 byte timestamp.");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
