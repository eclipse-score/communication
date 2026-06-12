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
#include "score/mw/com/impl/field_tags.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(ContainsTypeTest, ReturnsTrueWhenTargetTypeIsInPack)
{
    // Given a tag pack containing the target type (at any position)
    // When checking contains_type
    // Then the result is true
    static_assert(detail::contains_type<WithGetter, WithGetter>::value);
    static_assert(detail::contains_type<WithSetter, WithSetter>::value);
    static_assert(detail::contains_type<WithNotifier, WithNotifier>::value);
    static_assert(detail::contains_type<WithGetter, WithGetter, WithSetter>::value);
    static_assert(detail::contains_type<WithGetter, WithGetter, WithSetter, WithNotifier>::value);
    static_assert(detail::contains_type<WithSetter, WithGetter, WithSetter, WithNotifier>::value);
    static_assert(detail::contains_type<WithNotifier, WithGetter, WithSetter, WithNotifier>::value);
}

TEST(ContainsTypeTest, ReturnsFalseWhenPackIsEmpty)
{
    // Given an empty tag pack
    // When checking contains_type for any target type
    // Then the result is false
    static_assert(!detail::contains_type<WithGetter>::value);
    static_assert(!detail::contains_type<WithSetter>::value);
    static_assert(!detail::contains_type<WithNotifier>::value);
}

TEST(ContainsTypeTest, ReturnsFalseWhenTargetTypeIsAbsentFromPack)
{
    // Given a tag pack that does not contain the target type
    // When checking contains_type
    // Then the result is false
    static_assert(!detail::contains_type<WithGetter, WithSetter>::value);
    static_assert(!detail::contains_type<WithGetter, WithSetter, WithNotifier>::value);
    static_assert(!detail::contains_type<WithSetter, WithGetter, WithNotifier>::value);
    static_assert(!detail::contains_type<WithNotifier, WithGetter, WithSetter>::value);
}

TEST(IsTagEnabledTest, ReturnsTrueWhenFieldTypesMatchAndTargetTagIsInPack)
{
    // Given FieldType == ClassFieldType and the target tag in the pack
    // When checking is_tag_enabled
    // Then the result is true
    using FieldType = int;
    static_assert(detail::is_tag_enabled<FieldType, FieldType, WithGetter, WithGetter>::value);
    static_assert(detail::is_tag_enabled<FieldType, FieldType, WithSetter, WithGetter, WithSetter>::value);
    static_assert(
        detail::is_tag_enabled<FieldType, FieldType, WithNotifier, WithGetter, WithSetter, WithNotifier>::value);
}

TEST(IsTagEnabledTest, ReturnsFalseWhenFieldTypesDoNotMatch)
{
    // Given FieldType != ClassFieldType.
    // When checking is_tag_enabled
    // Then the result is false even if the target tag is in the pack
    static_assert(!detail::is_tag_enabled<int, double, WithGetter, WithGetter>::value);
    static_assert(!detail::is_tag_enabled<int, double, WithNotifier, WithGetter, WithSetter, WithNotifier>::value);
}

TEST(IsTagEnabledTest, ReturnsFalseWhenTargetTagIsAbsentFromPack)
{
    // Given FieldType == ClassFieldType but the target tag is not in the pack
    // When checking is_tag_enabled
    // Then the result is false
    using FieldType = int;
    static_assert(!detail::is_tag_enabled<FieldType, FieldType, WithGetter, WithSetter>::value);
    static_assert(!detail::is_tag_enabled<FieldType, FieldType, WithGetter, WithSetter, WithNotifier>::value);
    static_assert(!detail::is_tag_enabled<FieldType, FieldType, WithGetter>::value);
}

}  // namespace
}  // namespace score::mw::com::impl
