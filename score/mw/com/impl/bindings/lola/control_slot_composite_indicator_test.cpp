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
#include "score/mw/com/impl/bindings/lola/control_slot_composite_indicator.h"
#include <cstdint>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(ControlSlotCompositeIndicator, Creation_Default)
{
    // given a default constructed ControlSlotIndicator
    ControlSlotCompositeIndicator unit{};

    // expect it to be completely invalid
    EXPECT_FALSE(unit.IsValidQM());
    EXPECT_FALSE(unit.IsValidAsilB());
    EXPECT_FALSE(unit.IsValidQmAndAsilB());
}

TEST(ControlSlotCompositeIndicator, Creation_ValidQM)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, ControlSlotCompositeIndicator::CompositeSlotTagType::QM};

    // expect QM to be valid
    EXPECT_TRUE(unit.IsValidQM());
    // but ASIL-B not
    EXPECT_FALSE(unit.IsValidAsilB());
    EXPECT_FALSE(unit.IsValidQmAndAsilB());
}

TEST(ControlSlotCompositeIndicator, Creation_ValidAsilB)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{
        slot_index, slot_asilb, ControlSlotCompositeIndicator::CompositeSlotTagType::ASIL_B};

    // expect ASIL-B to be valid
    EXPECT_TRUE(unit.IsValidAsilB());
    // but QM not
    EXPECT_FALSE(unit.IsValidQM());
    EXPECT_FALSE(unit.IsValidQmAndAsilB());
}

TEST(ControlSlotCompositeIndicator, Creation_ValidQmAndAsilB)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_asilb{27U};
    ControlSlotType slot_qm{28U};
    // given a ControlSlotCompositeIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // expect ASIL-B and QM to be valid
    EXPECT_TRUE(unit.IsValidAsilB());
    EXPECT_TRUE(unit.IsValidQM());
    EXPECT_TRUE(unit.IsValidQmAndAsilB());
}

TEST(ControlSlotCompositeIndicator, GetSlotQM)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, ControlSlotCompositeIndicator::CompositeSlotTagType::QM};

    // expect slot being accessible and containing expected value
    EXPECT_EQ(unit.GetSlotQM(), 27U);
}

TEST(ControlSlotCompositeIndicator, GetSlotAsilB)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{
        slot_index, slot_asilb, ControlSlotCompositeIndicator::CompositeSlotTagType::ASIL_B};

    // expect slot being accessible and containing expected value
    EXPECT_EQ(unit.GetSlotAsilB(), 27U);
}

TEST(ControlSlotCompositeIndicator, GetIndex_QmOnly)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot for QM
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, ControlSlotCompositeIndicator::CompositeSlotTagType::QM};

    // expect index being accessible and containing expected value
    EXPECT_EQ(unit.GetIndex(), slot_index);
}

TEST(ControlSlotCompositeIndicator, GetIndex_AsilBOnly)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotCompositeIndicator created with a given index/slot for ASIL-B
    ControlSlotCompositeIndicator unit{
        slot_index, slot_asilb, ControlSlotCompositeIndicator::CompositeSlotTagType::ASIL_B};

    // expect index being accessible and containing expected value
    EXPECT_EQ(unit.GetIndex(), slot_index);
}

TEST(ControlSlotCompositeIndicator, Copy)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // and a copy from it
    ControlSlotCompositeIndicator unit2{unit};

    // expect the members of both being equal
    EXPECT_EQ(unit.GetIndex(), unit2.GetIndex());
    EXPECT_EQ(unit.GetSlotQM(), unit2.GetSlotQM());
    EXPECT_EQ(unit.GetSlotAsilB(), unit2.GetSlotAsilB());
}

TEST(ControlSlotCompositeIndicator, Equal)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // and a copy from it
    ControlSlotCompositeIndicator unit2{unit};

    // expect both comparing equal
    EXPECT_EQ(unit, unit2);
}

TEST(ControlSlotCompositeIndicator, NotEqual)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // and a 2nd unit with a different qm-slot
    ControlSlotType slot_qm2{27U};
    ControlSlotCompositeIndicator unit2{slot_index, slot_qm2, slot_asilb};

    // expect both NOT comparing equal
    EXPECT_FALSE(unit == unit2);
}

TEST(ControlSlotCompositeIndicator, Reset)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // when calling Reset()
    unit.Reset();

    // expect, that the unit is invalid
    EXPECT_FALSE(unit.IsValidQM());
    EXPECT_FALSE(unit.IsValidAsilB());
}

TEST(ControlSlotCompositeIndicatorDeathTest, QmSlotAccessDies)
{
    // given a default constructed ControlSlotCompositeIndicator
    ControlSlotCompositeIndicator unit{};

    // expect it to die, when accessing the qm-slot
    EXPECT_DEATH(unit.GetSlotQM(), ".*");
}

TEST(ControlSlotCompositeIndicatorDeathTest, AsilBSlotAccessDies)
{
    // given a default constructed ControlSlotCompositeIndicator
    ControlSlotCompositeIndicator unit{};

    // expect it to die, when accessing the asil-b slot
    EXPECT_DEATH(unit.GetSlotAsilB(), ".*");
}

TEST(ControlSlotCompositeIndicatorDeathTest, IndexAccessDies)
{
    // given a default constructed ControlSlotCompositeIndicator
    ControlSlotCompositeIndicator unit{};

    // expect it to die, when accessing the index
    EXPECT_DEATH(unit.GetIndex(), ".*");
}

TEST(ControlSlotCompositeIndicatorDeathTest, IndexAccessAfterResetDies)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // when calling Reset()
    unit.Reset();

    // expect it to die, when accessing the index afterwards
    EXPECT_DEATH(unit.GetIndex(), ".*");
}

TEST(ControlSlotCompositeIndicatorDeathTest, SlotAccessAfterResetDies)
{
    SlotIndexType slot_index{42U};
    ControlSlotType slot_qm{27U};
    ControlSlotType slot_asilb{27U};
    // given a ControlSlotIndicator created with a given index/slot
    ControlSlotCompositeIndicator unit{slot_index, slot_qm, slot_asilb};

    // when calling Reset()
    unit.Reset();

    // expect it to die, when accessing a slot afterwards
    EXPECT_DEATH(unit.GetSlotQM(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
