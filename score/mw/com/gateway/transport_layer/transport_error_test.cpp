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
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include <gtest/gtest.h>

namespace score::mw::com::gateway
{
namespace
{

class TransportErrorTest : public ::testing::Test
{
  protected:
    void TestErrorMessage(const TransportErrorc error_code, const std::string_view expected_error_output)
    {
        const auto error_code_test =
            transport_error_domain_dummy_.MessageFor(static_cast<score::result::ErrorCode>(error_code));
        ASSERT_EQ(error_code_test, expected_error_output);
    }

    TransportErrorDomain transport_error_domain_dummy_{};
};

TEST_F(TransportErrorTest, MessageForServerSetupFailed)
{
    TestErrorMessage(TransportErrorc::kServerSetupFailed, "Gateway server could not be setup.");
}

TEST_F(TransportErrorTest, MessageForServerListeningReturned)
{
    TestErrorMessage(TransportErrorc::kServerListeningReturned, "Server returned from its listening thread.");
}

TEST_F(TransportErrorTest, MessageForClientSetupFailed)
{
    TestErrorMessage(TransportErrorc::kClientSetupFailed, "Gateway client could not be setup.");
}

TEST_F(TransportErrorTest, MessageForFailedToProvideService)
{
    TestErrorMessage(TransportErrorc::kFailedToProvideService, "Failed to provide service.");
}

TEST_F(TransportErrorTest, MessageForConnectionFailure)
{
    TestErrorMessage(TransportErrorc::kConnectionFailure, "Transport connection failed.");
}

TEST_F(TransportErrorTest, MessageForNotConnected)
{
    TestErrorMessage(TransportErrorc::kNotConnected, "Transport not connected.");
}

TEST_F(TransportErrorTest, MessageForTimeout)
{
    TestErrorMessage(TransportErrorc::kTimeout, "Transport operation timed out.");
}

TEST_F(TransportErrorTest, MessageForInvalidMessage)
{
    TestErrorMessage(TransportErrorc::kInvalidMessage, "Invalid transport message.");
}

TEST_F(TransportErrorTest, MessageForNotSupported)
{
    TestErrorMessage(TransportErrorc::kNotSupported, "Operation not supported by this transport.");
}

TEST_F(TransportErrorTest, MessageForSendFailure)
{
    TestErrorMessage(TransportErrorc::kSendFailure, "Failed to send message.");
}

TEST_F(TransportErrorTest, MessageForReceiveFailure)
{
    TestErrorMessage(TransportErrorc::kReceiveFailure, "Failed to receive message.");
}

TEST_F(TransportErrorTest, MessageForDefault)
{
    TestErrorMessage(static_cast<TransportErrorc>(-1), "unknown transport error");
}

TEST(TransportError, MakeErrorExpectedError)
{
    const score::result::Error err{MakeError(TransportErrorc::kReceiveFailure)};
    EXPECT_EQ(*err, static_cast<int>(TransportErrorc::kReceiveFailure));
    EXPECT_TRUE(err.UserMessage().empty());
}

TEST(TransportError, MakeErrorExpectedErrorWithUserMessage)
{
    constexpr std::string_view kUserMessage{"test user message"};
    const score::result::Error err{MakeError(TransportErrorc::kConnectionFailure, kUserMessage)};
    EXPECT_EQ(*err, static_cast<int>(TransportErrorc::kConnectionFailure));
    EXPECT_EQ(err.UserMessage(), kUserMessage);
}

}  // namespace
}  // namespace score::mw::com::gateway
