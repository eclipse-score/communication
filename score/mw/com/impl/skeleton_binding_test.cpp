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
#include "score/mw/com/impl/skeleton_binding.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

class MySkeleton final : public SkeletonBinding
{
  public:
    ResultBlank PrepareOffer(SkeletonEventBindings&,
                             SkeletonFieldBindings&,
                             std::optional<RegisterShmObjectTraceCallback>) noexcept override
    {
        return {};
    }
    void PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback>) noexcept override {}
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    };
    bool VerifyAllMethodsRegistered() const override
    {
        return true;
    }
};

TEST(SkeletonBindingTest, SkeletonBindingShouldNotBeCopyable)
{
    static_assert(!std::is_copy_constructible<MySkeleton>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MySkeleton>::value, "Is wrongly copyable");
}

TEST(SkeletonBindingTest, SkeletonBindingShouldNotBeMoveable)
{
    static_assert(!std::is_move_constructible<MySkeleton>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<MySkeleton>::value, "Is wrongly moveable");
}

}  // namespace
}  // namespace score::mw::com::impl
