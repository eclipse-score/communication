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

#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"

#include <gtest/gtest.h>

#include <memory>

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

TEST(SkeletonMethodBaseTests, IsNotServiceOfferedByDefault)
{
    // Given a freshly constructed SkeletonMethodBase
    SkeletonMethodBase method{"dummy_method", std::make_unique<mock_binding::SkeletonMethod>(), MethodType::kMethod};

    // Then IsServiceOffered() returns false
    EXPECT_FALSE(method.IsServiceOffered());
}

TEST(SkeletonMethodBaseTests, SetServiceOfferedTrueMarksServiceAsOffered)
{
    // Given a freshly constructed SkeletonMethodBase
    SkeletonMethodBase method{"dummy_method", std::make_unique<mock_binding::SkeletonMethod>(), MethodType::kMethod};

    // When notifying the method that the service is offered
    method.SetServiceOffered(true);

    // Then IsServiceOffered() returns true
    EXPECT_TRUE(method.IsServiceOffered());
}

TEST(SkeletonMethodBaseTests, SetServiceOfferedFalseAfterTrueMarksServiceAsNotOffered)
{
    // Given a SkeletonMethodBase which has been notified that the service is offered
    SkeletonMethodBase method{"dummy_method", std::make_unique<mock_binding::SkeletonMethod>(), MethodType::kMethod};
    method.SetServiceOffered(true);

    // When notifying the method that the service is no longer offered
    method.SetServiceOffered(false);

    // Then IsServiceOffered() returns false
    EXPECT_FALSE(method.IsServiceOffered());
}

}  // namespace

}  // namespace score::mw::com::impl
