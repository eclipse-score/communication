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
#include "score/mw/com/impl/mocking/skeleton_base_mock.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

namespace score::mw::com::impl
{
namespace
{

class SkeletonMockFixture : public ::testing::Test
{
  public:
    SkeletonMockFixture()
    {
        unit_.InjectMock(skeleton_mock_);
    }

    SkeletonBaseMock skeleton_mock_{};
    SkeletonBase unit_{nullptr, MakeFakeInstanceIdentifier(1U)};
};

TEST_F(SkeletonMockFixture, OfferServiceDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonBase constructed with an empty binding an dummy InstanceIdentifier and an injected
    // SkeletonBaseMock

    // Expecting that OfferService will be called on the mock which returns a valid result
    EXPECT_CALL(skeleton_mock_, OfferService()).WillOnce(Return(Result<void>{}));

    // When OfferService is called on the SkeletonBase
    auto result = unit_.OfferService();

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result, Result<void>{});
}

TEST_F(SkeletonMockFixture, OfferServiceReturnsErrorWhenMockReturnsError)
{
    // Given a SkeletonBase constructed with an empty binding an dummy InstanceIdentifier and an injected
    // SkeletonBaseMock

    // Expecting that OfferService will be called on the mock which returns a valid result
    const auto error_code = ComErrc::kServiceInstanceAlreadyOffered;
    EXPECT_CALL(skeleton_mock_, OfferService()).WillOnce(Return(MakeUnexpected(error_code)));

    // When OfferService is called on the SkeletonBase
    auto result = unit_.OfferService();

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonMockFixture, StopOfferServiceDispatchesToMockAfterInjectingMock)
{
    // Given a SkeletonBase constructed with an empty binding an dummy InstanceIdentifier and an injected
    // SkeletonBaseMock

    // Expecting that StopOfferService will be called on the mock
    EXPECT_CALL(skeleton_mock_, StopOfferService());

    // When StopOfferService is called on the SkeletonBase
    unit_.StopOfferService();
}

}  // namespace
}  // namespace score::mw::com::impl
