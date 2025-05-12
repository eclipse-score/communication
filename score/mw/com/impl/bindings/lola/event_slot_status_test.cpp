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
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(EventSlotStatusView, SeparatesReferenceCount)
{
    RecordProperty("Verifies", "SCR-5899287");
    RecordProperty("Description", "Ensures that a slot status contains a reference count");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a EventSlotStatus that only contains a reference count
    EventSlotStatus unit{0x12345678};

    // When reading the reference count from it
    const auto value = unit.GetReferenceCount();

    // Then it equals the expected value (last 4 bytes)
    EXPECT_EQ(value, 0x12345678);
}

TEST(EventSlotStatusView, SeparatesTimeStamp)
{
    RecordProperty("Verifies", "SCR-5899287");
    RecordProperty("Description", "Ensures that a slot status contains a time stamp");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a EventSlotStatus that only contains a reference count and time stamp
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When reading the time stamp
    const auto value = unit.GetTimeStamp();

    // Then it equals the expected value (first 4 bytes)
    EXPECT_EQ(value, 0x12345678);
}

TEST(EventSlotStatusView, CanSetTimeStamp)
{
    // Given a EventSlotStatus with a number
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When setting the timestamp
    unit.SetTimeStamp(0x42525200);

    // Then the underlying value is exchanged
    EXPECT_EQ(unit.GetTimeStamp(), 0x42525200);
}

TEST(EventSlotStatusView, DefaultConstructionLeadsToInvalid)
{
    // Given a EventSlotStatus that is default constructed
    EventSlotStatus unit{};

    // When check its validity
    const auto value = unit.IsInvalid();

    // Then it is invalid
    EXPECT_TRUE(value);
}

TEST(EventSlotStatusView, CorrectlyReturnsValid)
{
    // Given a EventSlotStatus with a number (valid)
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When check its validity
    const auto value = unit.IsInvalid();

    // Then it is valid
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, CanBeMarkedInvalid)
{
    RecordProperty("Verifies", "SCR-5899287");
    RecordProperty("Description", "Ensures that can be marked invalid");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a EventSlotStatus with a number (valid)
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When marking it invalid
    unit.MarkInvalid();

    // Then it is invalid
    EXPECT_TRUE(unit.IsInvalid());
}

TEST(EventSlotStatusView, CorrectlyReturnsInWriting)
{
    // Given a EventSlotStatus with a number (not in writing)
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When check if it is in writing
    const auto value = unit.IsInWriting();

    // Then it is not in writing
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, CanBeMarkedWriting)
{
    RecordProperty("Verifies", "SCR-5899287");
    RecordProperty("Description", "Ensures that can be marked writing");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a EventSlotStatus with a number (not writing)
    EventSlotStatus unit{0x12345678ABCDEF12};

    // When marking it writing
    unit.MarkInWriting();

    // Then it is in writing
    EXPECT_TRUE(unit.IsInWriting());
}

TEST(EventSlotStatusView, ConstructFromTwoValues)
{
    // Given a EventSlotStatus constructed from two numbers
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When getting time stamp and ref count
    const auto time_stamp = unit.GetTimeStamp();
    const auto ref_count = unit.GetReferenceCount();

    // Then Both values match their respective numbers
    EXPECT_EQ(time_stamp, 0x12345678);
    EXPECT_EQ(ref_count, 0xABCDEF12);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenIfInvalid)
{
    // Given a EventSlotStatus that is invalid
    EventSlotStatus unit{0x12345678, 0xABCDEF12};
    unit.MarkInvalid();

    // When checking if the time stamp is between a value (min < timestamp < max)
    const auto value = unit.IsTimeStampBetween(0, 0xFFFFFFFF);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenIfInWriting)
{
    // Given a EventSlotStatus that is in writing
    EventSlotStatus unit{0x12345678, 0xABCDEF12};
    unit.MarkInWriting();

    // When checking if the time stamp is between a value (min < timestamp < max)
    const auto value = unit.IsTimeStampBetween(0, 0xFFFFFFFF);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, TimeStampIsInBetween)
{
    RecordProperty("Verifies", "SCR-5899287");
    RecordProperty("Description", "Ensures that a timestamp can be check inbetween two other");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a EventSlotStatus
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When checking if the time stamp is between a value (min < timestamp < max)
    const auto value = unit.IsTimeStampBetween(0, 0xFFFFFFFF);

    // Then the time stamp is in between
    EXPECT_TRUE(value);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenBecauseOfMin)
{
    // Given a EventSlotStatus
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When checking if the time stamp is between a value (min > time stamp)
    const auto value = unit.IsTimeStampBetween(0x12345679, 0xFFFFFFFF);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenBecauseOfMax)
{
    // Given a EventSlotStatus
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When checking if the time stamp is between a value (max < time stamp)
    const auto value = unit.IsTimeStampBetween(0, 0x12345677);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenIncludingBorderLow)
{
    // Given a EventSlotStatus
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When checking if the time stamp is between a value (min == searched time stamp)
    const auto value = unit.IsTimeStampBetween(0x12345678, 0xFFFFFFFF);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

TEST(EventSlotStatusView, TimeStampIsNotInBetweenIncludingBorderHigh)
{
    // Given a EventSlotStatus
    EventSlotStatus unit{0x12345678, 0xABCDEF12};

    // When checking if the time stamp is between a value (max == searched time stamp)
    const auto value = unit.IsTimeStampBetween(0, 0x12345678);

    // Then the time stamp is not in between
    EXPECT_FALSE(value);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
