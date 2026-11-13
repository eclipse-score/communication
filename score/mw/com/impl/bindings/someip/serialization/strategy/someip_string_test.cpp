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
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_strategy_test_fixtures.h"

#include <gtest/gtest.h>

#include <score/string.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

TEST_F(SomeIpStrategyFixture, StringIsUtf8WithBomAndTerminator)
{
    // Given a string ...
    const score::cpp::pmr::string value{"Hi"};

    // ... when it is serialized and deserialized ...
    RoundTrip(value);

    // ... then a 4-byte big-endian length precedes the UTF-8 BOM, characters, and null terminator (no padding).
    ASSERT_EQ(buffer_.size(), 4U + 3U + 2U + 1U);  // length + BOM + characters + terminator
    auto it = buffer_.cbegin();
    EXPECT_EQ(*it++, 0x00);  // uint32 big-endian length = BOM(3) + "Hi"(2) + terminator(1) = 6
    EXPECT_EQ(*it++, 0x00);
    EXPECT_EQ(*it++, 0x00);
    EXPECT_EQ(*it++, 0x06);
    EXPECT_EQ(*it++, 0xEF);  // UTF-8 BOM
    EXPECT_EQ(*it++, 0xBB);
    EXPECT_EQ(*it++, 0xBF);
    EXPECT_EQ(*it++, 'H');
    EXPECT_EQ(*it++, 'i');
    EXPECT_EQ(*it++, 0x00);  // null terminator
}

TEST_F(SomeIpStrategyFixture, StringLongerThanLengthFieldReturnsError)
{
    // Given a 1-byte length field and a string whose encoded size (BOM + characters + terminator) exceeds it ...
    const SomeIpLayout layout{{ByteOrder::kBigEndian, kAlign8Bits, sizeof(std::uint8_t)}, nullptr, 0U};
    const SomeIpStrategy strategy{layout};
    const Serializer<SomeIpStrategy> unit{strategy};
    const score::cpp::pmr::string value(std::numeric_limits<std::uint8_t>::max(), 'x');
    std::vector<std::uint8_t> buffer{};

    // ... when it is serialized ...
    const auto result = unit.Serialize(value, buffer);

    // ... then the overflow is reported instead of writing a truncated length.
    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace serialization
}  // namespace score
