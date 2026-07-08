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
#include "score/mw/com/impl/plumbing/binding_factory_error.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

class BindingFactoryErrorMessageForFixture : public ::testing::Test
{
  protected:
    void testErrorMessage(BindingFactoryErrorCode errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest =
            BindingFactoryErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    BindingFactoryErrorDomain BindingFactoryErrorDomainDummy{};
};

TEST_F(BindingFactoryErrorMessageForFixture, MessageForParentBindingIsNotLola)
{
    testErrorMessage(BindingFactoryErrorCode::kParentBindingIsNotLola, "Parent proxy binding is not a LoLa binding.");
}

TEST_F(BindingFactoryErrorMessageForFixture, MessageForUnsupportedBindingType)
{
    testErrorMessage(BindingFactoryErrorCode::kUnsupportedBindingType,
                     "Service type deployment contains an unsupported binding type.");
}

TEST_F(BindingFactoryErrorMessageForFixture, MessageForProxyCreationFailed)
{
    testErrorMessage(BindingFactoryErrorCode::kProxyCreationFailed, "Proxy binding creation failed.");
}

using BindingFactoryErrorMessageForDeathTest = BindingFactoryErrorMessageForFixture;
TEST_F(BindingFactoryErrorMessageForDeathTest, MessageForkInvalidTerminates)
{
    // When calling MessageFor with the code kInvalid
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = BindingFactoryErrorDomainDummy.MessageFor(
                     static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kInvalid)),
                 ".*");
}

TEST_F(BindingFactoryErrorMessageForDeathTest, MessageForNumEnumElements)
{
    // When calling MessageFor with the code kNumEnumElements
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = BindingFactoryErrorDomainDummy.MessageFor(
                     static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kNumEnumElements)),
                 ".*");
}

TEST_F(BindingFactoryErrorMessageForFixture, MessageForDefaultClause)
{
    auto one_past_the_last_lable = static_cast<std::uint32_t>(BindingFactoryErrorCode::kNumEnumElements) + 1;
    testErrorMessage(static_cast<BindingFactoryErrorCode>(one_past_the_last_lable), "unknown binding factory error");
}

}  // namespace
}  // namespace score::mw::com::impl
