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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_TEST_FIXTURES_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_TEST_FIXTURES_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_strategy.h"

#include "score/serialization/serializer.h"

#include <gtest/gtest.h>

#include <score/static_vector.hpp>

#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{

class SomeIpStrategyFixture : public ::testing::Test
{
  public:
    SomeIpStrategy strategy_{};
    Serializer<SomeIpStrategy> unit_{strategy_};
    std::vector<std::uint8_t> buffer_{};

    template <typename TypeToTest>
    void RoundTrip(const TypeToTest& value)
    {
        // Given we serialize a value ...
        auto result = unit_.Serialize(value, buffer_);
        ASSERT_TRUE(result.has_value());

        // ... when deserializing it ...
        TypeToTest new_value{};
        result = unit_.DeSerialize(buffer_, new_value);
        ASSERT_TRUE(result.has_value());

        // ... then the values match.
        EXPECT_EQ(value, new_value);
    }
};

struct Inner
{
    std::uint16_t id{0xAABB};
    std::uint8_t flag{0x01};
};
SCORE_STRUCT_VISITABLE(Inner, id, flag)

inline bool operator==(const Inner& lhs, const Inner& rhs)
{
    return (lhs.id == rhs.id) && (lhs.flag == rhs.flag);
}

struct Outer
{
    std::uint32_t a{0x01020304};
    Inner inner{};
};
SCORE_STRUCT_VISITABLE(Outer, a, inner)

inline bool operator==(const Outer& lhs, const Outer& rhs)
{
    return (lhs.a == rhs.a) && (lhs.inner == rhs.inner);
}

struct BoundedMessage
{
    std::uint16_t id{0x0102};
    score::cpp::static_vector<std::uint32_t, 4U> values{};
};
SCORE_STRUCT_VISITABLE(BoundedMessage, id, values)

struct Measurement
{
    std::uint16_t a{};  // field 0
    std::uint32_t b{};  // field 1
    std::uint32_t c{};  // field 2
};
SCORE_STRUCT_VISITABLE(Measurement, a, b, c)

}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_TEST_FIXTURES_H
