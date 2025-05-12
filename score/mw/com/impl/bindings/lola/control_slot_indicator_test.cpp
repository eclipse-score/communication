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
#include "score/mw/com/impl/bindings/lola/control_slot_indicator.h"
#include <cstdint>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(ControlSlotIndicator, Creation_Default)
{
    // given a default constructed ControlSlotIndicator
    ControlSlotIndicator unit{};

    // expect it to be invalid
    EXPECT_FALSE(unit.IsValid());
}

TEST(ControlSlotIndicator, Creation_Valid)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotIndicator unit{slot_index, slot};

    // expect it to be valid
    EXPECT_TRUE(unit.IsValid());
}

TEST(ControlSlotIndicator, GetSlot)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotIndicator unit{slot_index, slot};

    // expect slot being accessible and containing expected value
    EXPECT_EQ(unit.GetSlot(), 27U);
}

TEST(ControlSlotIndicator, GetIndex)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotIndicator unit{slot_index, slot};

    // expect it to be valid
    EXPECT_EQ(unit.GetIndex(), 42U);
}

TEST(ControlSlotIndicator, Copy)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotIndicator unit{slot_index, slot};

    // and a copy from it
    ControlSlotIndicator unit2{unit};

    // expect the members of both being equal
    EXPECT_EQ(unit.GetIndex(), unit2.GetIndex());
    EXPECT_EQ(unit.GetSlot(), unit2.GetSlot());
}

TEST(ControlSlotIndicator, Reset)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotIndicator unit{slot_index, slot};

    // when calling Reset()
    unit.Reset();

    // expect, that the unit is invalid
    EXPECT_FALSE(unit.IsValid());
}

TEST(ControlSlotIndicatorDeathTest, SlotAccessDies)
{
    // given a default constructed ControlSlotIndicator
    ControlSlotIndicator unit{};

    // expect it to die, when accessing the slot
    EXPECT_DEATH(unit.GetSlot(), ".*");
}

TEST(ControlSlotIndicatorDeathTest, IndexAccessDies)
{
    // given a default constructed ControlSlotIndicator
    ControlSlotIndicator unit{};

    // expect it to die, when accessing the index
    EXPECT_DEATH(unit.GetIndex(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
