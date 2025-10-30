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
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

template <typename SampleType>
class MyEvent final : public SkeletonEventBinding<SampleType>
{
  public:
    ResultBlank PrepareOffer() noexcept override
    {
        return {};
    }
    void PrepareStopOffer() noexcept override {}
    ResultBlank Send(
        const SampleType&,
        score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>) noexcept override
    {
        return {};
    }
    ResultBlank Send(
        SampleAllocateePtr<SampleType>,
        score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>) noexcept override
    {
        return {};
    }
    Result<SampleAllocateePtr<SampleType>> Allocate() noexcept override
    {
        return MakeSampleAllocateePtr(std::make_unique<SampleType>());
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData) noexcept override {}
};

TEST(SkeletonEventBindingTest, CanGetMaxSizeOfLiteralType)
{
    MyEvent<std::uint8_t> unit{};
    EXPECT_EQ(unit.GetMaxSize(), 1);
}

TEST(SkeletonEventBindingTest, SkeletonEventBindingShouldNotBeCopyable)
{
    static_assert(!std::is_copy_constructible<MyEvent<std::uint8_t>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MyEvent<std::uint8_t>>::value, "Is wrongly copyable");
}

TEST(SkeletonEventBindingTest, SkeletonEventBindingShouldNotBeMoveable)
{
    static_assert(!std::is_move_constructible<MyEvent<std::uint8_t>>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<MyEvent<std::uint8_t>>::value, "Is wrongly moveable");
}

}  // namespace
}  // namespace score::mw::com::impl
