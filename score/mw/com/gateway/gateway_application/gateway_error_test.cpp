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
#include "score/mw/com/gateway/gateway_application/gateway_error.h"

#include <gtest/gtest.h>

namespace score::mw::com::gateway
{
namespace
{

class GatewayErrorTest : public ::testing::Test
{
  protected:
    void TestErrorMessage(const GatewayErrorc error_code, const std::string_view expected_error_output)
    {
        const auto error_code_test =
            gateway_error_domain_dummy_.MessageFor(static_cast<score::result::ErrorCode>(error_code));
        ASSERT_EQ(error_code_test, expected_error_output);
    }

    GatewayErrorDomain gateway_error_domain_dummy_{};
};

TEST_F(GatewayErrorTest, MessageForSkeletonCreationFailed)
{
    TestErrorMessage(GatewayErrorc::kSkeletonCreationFailed,
                     "Creation of generic skeleton on destination gateway failed.");
}

TEST_F(GatewayErrorTest, MessageForSkeletonOfferFailed)
{
    TestErrorMessage(GatewayErrorc::kSkeletonOfferFailed,
                     "Offering of generic skeleton on destination gateway failed.");
}

TEST_F(GatewayErrorTest, MessageForSkeletonStopOfferFailedFallsBackToDefault)
{
    TestErrorMessage(GatewayErrorc::kSkeletonStopOfferFailed, "unknown gateway error");
}

TEST_F(GatewayErrorTest, MessageForUnknownServiceInstance)
{
    TestErrorMessage(GatewayErrorc::kUnknownServiceInstance,
                     "Gateway received an API call for an unknown service instance identifier.");
}

TEST_F(GatewayErrorTest, MessageForUnknownServiceElement)
{
    TestErrorMessage(GatewayErrorc::kUnknownServiceElement,
                     "Gateway received an API call for an unknown service element within an instance identifier.");
}

TEST_F(GatewayErrorTest, MessageForReceiveHandlerRegistrationFailed)
{
    TestErrorMessage(
        GatewayErrorc::kReceiveHandlerRegistrationFailed,
        "Gateway couldn't register event-receive-handler at its generic proxy to forward event update notifications.");
}

TEST_F(GatewayErrorTest, MessageForNonWhitelistedService)
{
    TestErrorMessage(GatewayErrorc::kNonWhitelistedService,
                     "Gateway received request to provide a non-whitelised service.");
}

TEST_F(GatewayErrorTest, MessageForDefault)
{
    TestErrorMessage(static_cast<GatewayErrorc>(-1), "unknown gateway error");
}

TEST(GatewayError, MakeErrorExpectedError)
{
    const score::result::Error err{MakeError(GatewayErrorc::kUnknownServiceInstance)};
    EXPECT_EQ(*err, static_cast<int>(GatewayErrorc::kUnknownServiceInstance));
    EXPECT_TRUE(err.UserMessage().empty());
}

TEST(GatewayError, MakeErrorExpectedErrorWithUserMessage)
{
    constexpr std::string_view kUserMessage{"test user message"};
    const score::result::Error err{MakeError(GatewayErrorc::kSkeletonOfferFailed, kUserMessage)};
    EXPECT_EQ(*err, static_cast<int>(GatewayErrorc::kSkeletonOfferFailed));
    EXPECT_EQ(err.UserMessage(), kUserMessage);
}

}  // namespace
}  // namespace score::mw::com::gateway
