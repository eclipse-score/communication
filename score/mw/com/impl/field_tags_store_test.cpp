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
#include "score/mw/com/impl/field_tags_store.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <tuple>

namespace score::mw::com::impl
{
namespace
{

TEST(FieldTagsStoreTest, CreatingWithNotifierReturnsTrueOnlyForHasNotifier)
{
    // When creating a FieldTagsStore with WithNotifier tag
    auto store = FieldTagsStore::Create<WithNotifier>();

    // Then only HasNotifier() returns true
    EXPECT_TRUE(store.HasNotifier());
    EXPECT_FALSE(store.HasSetter());
    EXPECT_FALSE(store.HasGetter());
}

TEST(FieldTagsStoreTest, CreatingWithGetterReturnsTrueOnlyForHasGetter)
{
    // When creating a FieldTagsStore with WithGetter tag
    auto store = FieldTagsStore::Create<WithGetter>();

    // Then only HasGetter() returns true
    EXPECT_FALSE(store.HasNotifier());
    EXPECT_FALSE(store.HasSetter());
    EXPECT_TRUE(store.HasGetter());
}

TEST(FieldTagsStoreTest, CreatingWithNotifierAndSetterReturnsTrueForNotifierAndSetter)
{
    // When creating a FieldTagsStore with WithNotifier and WithSetter tags
    auto store = FieldTagsStore::Create<WithNotifier, WithSetter>();

    // Then only HasNotifier() and HasSetter() return true
    EXPECT_TRUE(store.HasNotifier());
    EXPECT_TRUE(store.HasSetter());
    EXPECT_FALSE(store.HasGetter());
}

TEST(FieldTagsStoreTest, CreatingWithGetterAndSetterReturnsTrueForGetterAndSetter)
{
    // When creating a FieldTagsStore with WithGetter and WithSetter tags
    auto store = FieldTagsStore::Create<WithGetter, WithSetter>();

    // Then only HasGetter() and HasSetter() return true
    EXPECT_FALSE(store.HasNotifier());
    EXPECT_TRUE(store.HasSetter());
    EXPECT_TRUE(store.HasGetter());
}

TEST(FieldTagsStoreTest, CreatingWithNotifierAndGetterAndSetterReturnsTrueForAllTags)
{
    // When creating a FieldTagsStore with the same tags as in SkeletonFieldCreationFixture
    auto store = FieldTagsStore::Create<WithNotifier, WithGetter, WithSetter>();

    // Then all corresponding tag flags return true
    EXPECT_TRUE(store.HasNotifier());
    EXPECT_TRUE(store.HasSetter());
    EXPECT_TRUE(store.HasGetter());
}

// All 2^3 possible combinations of the notifier, getter and setter tags. Every combination is distinct.
const std::array<FieldTagsStore, 8> kAllTagCombinations{FieldTagsStore::Create<>(),
                                                        FieldTagsStore::Create<WithNotifier>(),
                                                        FieldTagsStore::Create<WithGetter>(),
                                                        FieldTagsStore::Create<WithSetter>(),
                                                        FieldTagsStore::Create<WithNotifier, WithGetter>(),
                                                        FieldTagsStore::Create<WithNotifier, WithSetter>(),
                                                        FieldTagsStore::Create<WithGetter, WithSetter>(),
                                                        FieldTagsStore::Create<WithNotifier, WithGetter, WithSetter>()};

class FieldTagsStoreEqualityFixture : public ::testing::TestWithParam<std::tuple<std::size_t, std::size_t>>
{
};

TEST_P(FieldTagsStoreEqualityFixture, StoresAreEqualIfAndOnlyIfTheyHaveTheSameTags)
{
    const auto [lhs_index, rhs_index] = GetParam();

    // Given two FieldTagsStores created from any combination of tags
    const auto& lhs = kAllTagCombinations.at(lhs_index);
    const auto& rhs = kAllTagCombinations.at(rhs_index);

    // When comparing them
    // Then they are equal only if both were created from the same combination of tags
    EXPECT_EQ(lhs == rhs, lhs_index == rhs_index);
}

INSTANTIATE_TEST_SUITE_P(FieldTagsStoreEqualityFixture,
                         FieldTagsStoreEqualityFixture,
                         ::testing::Combine(::testing::Range<std::size_t>(0U, kAllTagCombinations.size()),
                                            ::testing::Range<std::size_t>(0U, kAllTagCombinations.size())));

}  // namespace
}  // namespace score::mw::com::impl
