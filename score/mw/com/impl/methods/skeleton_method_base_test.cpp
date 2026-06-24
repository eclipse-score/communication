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
#include "score/mw/com/impl/methods/skeleton_method_base.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace
{

TEST(SkeletonMethodBaseTests, NotCopyable)
{
    static_assert(!std::is_copy_constructible<SkeletonMethodBase>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonMethodBase>::value, "Is wrongly copyable");
}

TEST(SkeletonMethodBaseTests, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonMethodBase>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonMethodBase>::value, "Is not move assignable");
}

}  // namespace

}  // namespace score::mw::com::impl
