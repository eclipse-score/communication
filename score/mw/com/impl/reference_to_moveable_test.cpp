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
#include "score/mw/com/impl/reference_to_moveable.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace
{

// Note. Functional tests for ReferenceToMoveable are in enable_reference_to_moveable_from_this_test.cpp as the intended
// way of creating / using a ReferenceToMoveable is for a class to inherit from EnableReferenceToMoveableFromThis.

TEST(ReferenceToMoveableTest, NotCopyableOrMoveable)
{
    static_assert(!std::is_copy_constructible_v<ReferenceToMoveable<int>::Reference>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<ReferenceToMoveable<int>::Reference>, "Is wrongly copyable");
    static_assert(!std::is_move_constructible_v<ReferenceToMoveable<int>::Reference>, "Is wrongly move constructible");
    static_assert(!std::is_move_assignable_v<ReferenceToMoveable<int>::Reference>, "Is wrongly move assignable");
}

TEST(ReferenceToMoveableTest, NotCopyable)
{
    static_assert(!std::is_copy_constructible_v<ReferenceToMoveable<int>>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<ReferenceToMoveable<int>>, "Is wrongly copyable");
}

TEST(ReferenceToMoveableTest, IsMoveable)
{
    static_assert(std::is_move_constructible_v<ReferenceToMoveable<int>>, "Is not move constructible");
    static_assert(std::is_move_assignable_v<ReferenceToMoveable<int>>, "Is not move assignable");
}

}  // namespace
}  // namespace score::mw::com::impl
