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
#include "score/mw/com/message_passing/sender_factory.h"

#include "score/mw/com/message_passing/sender_mock.h"

#include "score/language/safecpp/string_view/zstring_view.h"

#include <gtest/gtest.h>

namespace score::mw::com::message_passing
{
namespace
{

using ::testing::An;
using ::testing::Return;

using safecpp::literals::operator""_zsv;
constexpr safecpp::zstring_view SOME_VALID_PATH = "foo"_zsv;

class SenderFactoryFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        message_passing::SenderFactory::InjectSenderMock(&sender_mock_);
    }
    void TearDown() override
    {
        message_passing::SenderFactory::InjectSenderMock(nullptr);
    }

    testing::StrictMock<message_passing::SenderMock> sender_mock_{};
    score::cpp::stop_source lifecycle_source_{};
};

TEST_F(SenderFactoryFixture, CheckSenderMock)
{
    auto unit = SenderFactory::Create(SOME_VALID_PATH, lifecycle_source_.get_token());

    ::testing::InSequence is;

    EXPECT_CALL(sender_mock_, Send(An<const message_passing::ShortMessage&>())).Times(1);
    EXPECT_CALL(sender_mock_, Send(An<const message_passing::MediumMessage&>())).Times(1);
    EXPECT_CALL(sender_mock_, HasNonBlockingGuarantee()).Times(1).WillOnce(Return(true));

    EXPECT_TRUE(unit->Send(ShortMessage{}));
    EXPECT_TRUE(unit->Send(MediumMessage{}));
    EXPECT_TRUE(unit->HasNonBlockingGuarantee());
}

}  // namespace
}  // namespace score::mw::com::message_passing
