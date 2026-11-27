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

/**
 * @file io_dispatcher_error_test.cpp
 * @brief The error domain: the message each code maps to, and what MakeError() carries.
 */

#include "score/io_dispatcher/io_dispatcher_error.h"

#include <gtest/gtest.h>

#include <set>
#include <string_view>

using namespace io_dispatcher;

namespace
{

constexpr IoDispatcherErrc kEveryCode[]{IoDispatcherErrc::kApiError,
                                        IoDispatcherErrc::kInvalidHandle,
                                        IoDispatcherErrc::kResource,
                                        IoDispatcherErrc::kFatal};

}  // namespace

class IoDispatcherErrorTest : public ::testing::TestWithParam<IoDispatcherErrc>
{
};

TEST_P(IoDispatcherErrorTest, TheCodeCarriesANonEmptyMessage)
{
    const IoDispatcherErrorDomain domain{};
    EXPECT_FALSE(domain.MessageFor(static_cast<score::result::ErrorCode>(GetParam())).empty());
}

TEST_P(IoDispatcherErrorTest, MakeErrorCarriesTheCode)
{
    const auto error = MakeError(GetParam());
    EXPECT_EQ(error, GetParam());
}

TEST_P(IoDispatcherErrorTest, MakeErrorCarriesTheCallersMessage)
{
    constexpr std::string_view kMessage{"while arming the third source"};
    const auto error = MakeError(GetParam(), kMessage);
    EXPECT_EQ(error.UserMessage(), kMessage);
}

INSTANTIATE_TEST_SUITE_P(EveryIoDispatcherErrc, IoDispatcherErrorTest, ::testing::ValuesIn(kEveryCode));

TEST(IoDispatcherErrorDomainTest, NoTwoCodesShareAMessage)
{
    const IoDispatcherErrorDomain domain{};
    std::set<std::string_view> messages{};
    for (const IoDispatcherErrc code : kEveryCode)
    {
        messages.insert(domain.MessageFor(static_cast<score::result::ErrorCode>(code)));
    }

    EXPECT_EQ(messages.size(), std::size(kEveryCode)) << "two codes are indistinguishable in a log";
}

TEST(IoDispatcherErrorDomainTest, ACodeOutsideTheEnumerationGetsTheFallback)
{
    const IoDispatcherErrorDomain domain{};
    const auto fallback = domain.MessageFor(static_cast<score::result::ErrorCode>(0xFFFF));

    EXPECT_FALSE(fallback.empty());
    for (const IoDispatcherErrc code : kEveryCode)
    {
        EXPECT_NE(fallback, domain.MessageFor(static_cast<score::result::ErrorCode>(code)))
            << "an unknown code reads as a known one";
    }
}

TEST(IoDispatcherErrorDomainTest, MakeErrorLeavesTheMessageEmptyWhenTheCallerGivesNone)
{
    const auto error = MakeError(IoDispatcherErrc::kApiError);
    EXPECT_TRUE(error.UserMessage().empty()) << "a message nobody wrote was invented";
}
