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
#include "score/mw/com/impl/bindings/lola/methods/method_error.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

class MethodErrorMessageForFixture : public ::testing::Test
{
  protected:
    void TestErrorMessage(MethodErrc error_code, std::string_view expected_error_output)
    {
        const auto error_code_test =
            method_error_domain_dummy_.MessageFor(static_cast<score::result::ErrorCode>(error_code));
        ASSERT_EQ(error_code_test, expected_error_output);
    }

    MethodErrorDomain method_error_domain_dummy_{};
};

TEST_F(MethodErrorMessageForFixture, MessageForSkeletonAlreadyDestroyed)
{
    TestErrorMessage(MethodErrc::kSkeletonAlreadyDestroyed, "Command failed since skeleton was already destroyed.");
}

TEST_F(MethodErrorMessageForFixture, MessageForUnexpectedMessage)
{
    TestErrorMessage(MethodErrc::kUnexpectedMessage, "Message with an unexpected type was received.");
}

TEST_F(MethodErrorMessageForFixture, MessageForUnexpectedMessageSize)
{
    TestErrorMessage(MethodErrc::kUnexpectedMessageSize, "Message with an unexpected size was received.");
}

TEST_F(MethodErrorMessageForFixture, MessageForMessagePassingError)
{
    TestErrorMessage(MethodErrc::kMessagePassingError, "Message passing failed with an error.");
}

TEST_F(MethodErrorMessageForFixture, MessageForNotSubscribed)
{
    TestErrorMessage(MethodErrc::kNotSubscribed, "Method has not been successfully subscribed.");
}

TEST_F(MethodErrorMessageForFixture, MessageForNotOffered)
{
    TestErrorMessage(MethodErrc::kNotOffered, "Method has not been fully offered.");
}

TEST_F(MethodErrorMessageForFixture, MessageForUnknownProxy)
{
    TestErrorMessage(MethodErrc::kUnknownProxy, "Proxy is not allowed to access method.");
}

using MethodErrorMessageForDeathTest = MethodErrorMessageForFixture;
TEST_F(MethodErrorMessageForDeathTest, MessageForkInvalidTerminates)
{
    // When calling MessageFor with the code kInvalid
    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = method_error_domain_dummy_.MessageFor(static_cast<score::result::ErrorCode>(MethodErrc::kInvalid)),
        ".*");
}

TEST_F(MethodErrorMessageForDeathTest, MessageForNumEnumElements)
{
    // When calling MessageFor with the code kNumEnumElements
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = method_error_domain_dummy_.MessageFor(
                     static_cast<score::result::ErrorCode>(MethodErrc::kNumEnumElements)),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
