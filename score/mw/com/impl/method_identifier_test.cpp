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
#include "score/mw/com/impl/method_identifier.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(UniqueMethodIdentifierTest, EqualIdentifiersAreEqual)
{
    // Given two identifiers with the same name and type
    const UniqueMethodIdentifier lhs{"foo", MethodType::kMethod};
    const UniqueMethodIdentifier rhs{"foo", MethodType::kMethod};

    // When comparing for equality
    // Then they are equal
    EXPECT_TRUE(lhs == rhs);
    EXPECT_FALSE(lhs != rhs);
}

TEST(UniqueMethodIdentifierTest, DifferentNamesAreNotEqual)
{
    // Given two identifiers with different names but same type
    const UniqueMethodIdentifier lhs{"foo", MethodType::kMethod};
    const UniqueMethodIdentifier rhs{"bar", MethodType::kMethod};

    // When comparing
    // Then they are not equal
    EXPECT_FALSE(lhs == rhs);
    EXPECT_TRUE(lhs != rhs);
}

TEST(UniqueMethodIdentifierTest, DifferentTypesAreNotEqual)
{
    // Given two identifiers with same name but different type
    const UniqueMethodIdentifier lhs{"foo", MethodType::kGet};
    const UniqueMethodIdentifier rhs{"foo", MethodType::kSet};

    // When comparing
    // Then they are not equal
    EXPECT_FALSE(lhs == rhs);
    EXPECT_TRUE(lhs != rhs);
}

TEST(UniqueMethodIdentifierTest, LessThanComparesNameFirst)
{
    // Given identifiers where lhs name < rhs name
    const UniqueMethodIdentifier lhs{"aaa", MethodType::kSet};
    const UniqueMethodIdentifier rhs{"bbb", MethodType::kGet};

    // When comparing with operator<
    // Then lhs < rhs because name is the primary sort key
    EXPECT_TRUE(lhs < rhs);
    EXPECT_FALSE(rhs < lhs);
}

TEST(UniqueMethodIdentifierTest, LessThanComparesTypeWhenNamesEqual)
{
    // Given identifiers with same name but different types
    const UniqueMethodIdentifier lhs{"foo", MethodType::kGet};
    const UniqueMethodIdentifier rhs{"foo", MethodType::kSet};

    // When comparing with operator<
    // Then ordering falls back to the MethodType enum value, so kGet < kSet
    EXPECT_TRUE(lhs < rhs);
    EXPECT_FALSE(rhs < lhs);
}

TEST(UniqueMethodIdentifierTest, LessThanReturnsFalseForEqualIdentifiers)
{
    // Given two equal identifiers
    const UniqueMethodIdentifier lhs{"foo", MethodType::kMethod};
    const UniqueMethodIdentifier rhs{"foo", MethodType::kMethod};

    // When comparing with operator<
    // Then neither is less than the other
    EXPECT_FALSE(lhs < rhs);
    EXPECT_FALSE(rhs < lhs);
}

}  // namespace
}  // namespace score::mw::com::impl
