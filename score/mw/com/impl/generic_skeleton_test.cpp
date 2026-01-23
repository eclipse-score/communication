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
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/test/skeleton_binding_factory_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

class GenericSkeletonTest : public ::testing::Test
{
  public:
    GenericSkeletonTest()
    {
        auto skeleton_binding_mock = std::make_unique<mock_binding::Skeleton>();
        skeleton_binding_mock_ = skeleton_binding_mock.get();

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Return(ByMove(std::move(skeleton_binding_mock))));
    }

    RuntimeMockGuard runtime_mock_guard_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GenericSkeletonTest, CreateWithInstanceSpecifier)
{
    auto instance_specifier = InstanceSpecifier::Create("a/b/c").value();
    auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, ResolveInstanceIdentifier(instance_specifier))
        .WillOnce(Return(instance_identifier));

    auto skeleton_result = GenericSkeleton::Create(instance_specifier);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, CreateWithInstanceIdentifier)
{
    auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();
    auto skeleton_result = GenericSkeleton::Create(instance_identifier);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, AddEvent)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateInstanceIdentifier()).value();
    const SizeInfo size_info{16, 8};
    auto event_result = skeleton.AddEvent("MyEvent", size_info);
    ASSERT_TRUE(event_result.has_value());

    // Adding the same event again fails
    auto second_event_result = skeleton.AddEvent("MyEvent", size_info);
    ASSERT_FALSE(second_event_result.has_value());
}

TEST_F(GenericSkeletonTest, OfferAndStopOfferService)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateInstanceIdentifier()).value();

    EXPECT_CALL(*skeleton_binding_mock_, OfferService()).WillOnce(Return(score::result::Blank{}));
    auto offer_result = skeleton.OfferService();
    EXPECT_TRUE(offer_result.has_value());

    EXPECT_CALL(*skeleton_binding_mock_, StopOfferService());
    skeleton.StopOfferService();
}

TEST_F(GenericSkeletonTest, CreateFailsIfBindingFails)
{
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_)).WillOnce(Return(ByMove(nullptr)));
    auto skeleton_result = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateInstanceIdentifier());
    ASSERT_FALSE(skeleton_result.has_value());
    EXPECT_EQ(skeleton_result.error(), ComErrc::kBindingFailure);
}

} // namespace
} // namespace score::mw::com::impl