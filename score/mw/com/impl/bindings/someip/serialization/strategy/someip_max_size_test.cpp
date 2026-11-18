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

#include <score/static_vector.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace
{

TEST_F(SomeIpStrategyFixture, GetMaxSizeBoundsTheSerializedSize)
{
    // id (2) + [ length field (4) + capacity 4 * sizeof(uint32) (16) ] = 22
    constexpr std::size_t expected_max{sizeof(std::uint16_t) + sizeof(std::uint32_t) + (4U * sizeof(std::uint32_t))};
    EXPECT_EQ(GetMaxSize<BoundedMessage>(), expected_max);

    // A full vector reaches exactly the worst case.
    BoundedMessage full{};
    full.values = score::cpp::static_vector<std::uint32_t, 4U>{1U, 2U, 3U, 4U};
    auto result = unit_.Serialize(full, buffer_);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(buffer_.size(), GetMaxSize<BoundedMessage>());

    // A partially filled vector stays within the worst case.
    BoundedMessage few{};
    few.values = score::cpp::static_vector<std::uint32_t, 4U>{1U};
    std::vector<std::uint8_t> small_buffer{};
    result = unit_.Serialize(few, small_buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_LE(small_buffer.size(), GetMaxSize<BoundedMessage>());
}

}  // namespace
}  // namespace serialization
}  // namespace score
