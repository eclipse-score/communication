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
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/sample_allocatee_guard.h"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

class MyEvent final : public SkeletonEventBinding
{
  public:
    Result<void> PrepareOffer(const std::optional<InitializeSampleCallback>&) noexcept override
    {
        return {};
    }
    void PrepareStopOffer() noexcept override {}
    Result<void> Send(const void*,
                      std::optional<typename SkeletonEventBinding::SendTraceCallback>,
                      SampleAllocateeGuard) noexcept override
    {
        return {};
    }
    Result<void> Send(SampleAllocateePtr<void>,
                      std::optional<SkeletonEventBinding::SendTraceCallback>) noexcept override
    {
        return {};
    }
    Result<SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard guard) noexcept override
    {
        return MakeSampleAllocateePtr(mock_binding::SampleAllocateePtr{&test_sample_buffer_, [](void*) noexcept {}},
                                      std::move(guard));
    }
    Result<SamplePtr<void>> GetLatestSample(QualityType) override
    {
        return SamplePtr<void>{mock_binding::SamplePtr<void>{&test_sample_buffer_, [](void*) noexcept {}},
                               SampleReferenceGuard{}};
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    }
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData) noexcept override {}
    memory::DataTypeSizeInfo GetSizeInfo() const noexcept override
    {
        return sample_data_type_size_info;
    }
    Result<void> Notify() noexcept override
    {
        return {};
    }
    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback) noexcept override
    {
        return {};
    }
    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override
    {
        return {};
    }

  private:
    memory::DataTypeSizeInfo sample_data_type_size_info{sizeof(std::uint8_t), alignof(std::uint8_t)};
    std::uint8_t test_sample_buffer_{};
};

TEST(SkeletonEventBindingTest, CanGetSizeInfoOfLiteralType)
{
    MyEvent unit{};
    EXPECT_EQ(unit.GetSizeInfo().Size(), sizeof(std::uint8_t));
    EXPECT_EQ(unit.GetSizeInfo().Alignment(), alignof(std::uint8_t));
}

TEST(SkeletonEventBindingTest, SkeletonEventBindingShouldNotBeCopyable)
{
    static_assert(!std::is_copy_constructible<MyEvent>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MyEvent>::value, "Is wrongly copyable");
}

TEST(SkeletonEventBindingTest, SkeletonEventBindingShouldNotBeMoveable)
{
    static_assert(!std::is_move_constructible<MyEvent>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<MyEvent>::value, "Is wrongly moveable");
}

}  // namespace
}  // namespace score::mw::com::impl
