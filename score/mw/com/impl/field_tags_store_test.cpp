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

}  // namespace
}  // namespace score::mw::com::impl
