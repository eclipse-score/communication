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
#include "score/mw/com/gateway/transport_layer/sample/messages/message_error.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::gateway
{
namespace
{

class GatewayErrorMessageFixture : public ::testing::Test
{
  protected:
    void testErrorMessage(MessageErrorc errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest = ComErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    MessageErrorDomain ComErrorDomainDummy{};
};

TEST_F(GatewayErrorMessageFixture, MessageForBufferTooSmall)
{
    testErrorMessage(MessageErrorc::kBufferTooSmall, "Serialization buffer too small");
}

TEST_F(GatewayErrorMessageFixture, MessageForPayloadInvalid)
{
    testErrorMessage(MessageErrorc::kPayloadInvalid, "Format/layout of the message payload is invalid");
}

TEST_F(GatewayErrorMessageFixture, MessageForDefaultError)
{
    auto one_past_the_last_lable = static_cast<std::uint32_t>(MessageErrorc::kPayloadInvalid) + 1;
    testErrorMessage(static_cast<MessageErrorc>(one_past_the_last_lable), "unknown message error");
}

TEST_F(GatewayErrorMessageFixture, MakeErrorConstructsError)
{
    auto err = MakeError(MessageErrorc::kPayloadInvalid, "test");
    EXPECT_EQ(*err, static_cast<score::result::ErrorCode>(MessageErrorc::kPayloadInvalid));
    EXPECT_EQ(err.UserMessage(), "test");
}

}  // namespace

}  // namespace score::mw::com::gateway
