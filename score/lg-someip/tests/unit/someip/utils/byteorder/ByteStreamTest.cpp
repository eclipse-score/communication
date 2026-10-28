/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#include <chrono>
#include <gtest/gtest.h>
#include <memory>

#include <utils/byteorder/bytestream.h>

using namespace lgsomeip;

TEST(ByteStreamTest, ByteStreamTest) {
    std::uint32_t value = 0x11223344;
    std::uint32_t result;

    set_byte_stream(reinterpret_cast<std::uint8_t*>(&result), &value, 4);
    ASSERT_EQ(0x44332211, result);

    get_byte_stream(&value, reinterpret_cast<std::uint8_t*>(&result), 4);
    ASSERT_EQ(0x11223344, value);
}
