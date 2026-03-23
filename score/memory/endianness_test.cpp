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
#include "score/memory/endianness.h"

#include <gtest/gtest.h>

namespace score::memory
{
namespace
{

TEST(Endianness, WeAreRunningOnALittleEndianSystem)
{
    // Listen, if this test fails, then this means that this test is no longer running on a little endian machine, _NOT_
    // that the test is wrong or the code under test! Yes, this is not a unit test, but we assume in a lot of places in
    // our code that we are running on little endian (we try to clean this up, which is why we introduce this library)
    // but still, better we have a test that makes this clear.
    EXPECT_TRUE(IsSystemLittleEndian());
}

TEST(Endianness, CannotBeLittleAndBigAtTheSameTime)
{
    EXPECT_TRUE(IsSystemLittleEndian() != IsSystemBigEndian());
}

TEST(Endianness, SwitchingBytesNecessary)
{
    EXPECT_TRUE(SwitchingBytesNecessary(Endianness::kBigEndian));
    EXPECT_FALSE(SwitchingBytesNecessary(Endianness::kLittleEndian));
}

TEST(Endianness, ByteSwap)
{
    EXPECT_EQ(ByteSwap(0x10203040U), 0x40302010U);
    EXPECT_EQ(ByteSwap(static_cast<std::uint64_t>(0x1020304050607080ULL)), 0x8070605040302010ULL);
}

TEST(Endianness, BigEndianToHostEndianness)
{
    if (IsSystemLittleEndian())
    {
        EXPECT_EQ(BigEndianToHostEndianness(0x10203040U), ByteSwap(0x10203040U));
        EXPECT_EQ(BigEndianToHostEndianness(static_cast<std::uint64_t>(0x1020304050607080ULL)),
                  ByteSwap(static_cast<std::uint64_t>(0x1020304050607080ULL)));
    }
    else if (IsSystemBigEndian())
    {
        EXPECT_EQ(BigEndianToHostEndianness(0x10203040U), 0x10203040U);
        EXPECT_EQ(BigEndianToHostEndianness(static_cast<std::uint64_t>(0x1020304050607080ULL)), 0x1020304050607080ULL);
    }
}

}  // namespace
}  // namespace score::memory
