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
 *
 ********************************************************************************/

// Tests of the public lg-someip API (include/lgsomeip/LgsomeipApi.h). They use
// the public header only and need no routing daemon.

#include <lgsomeip/LgsomeipApi.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using namespace lgsomeip::api;

class LgsomeipApiTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config_path_ = std::string(LGSOMEIP_TEST_CONFIG_DIR) + "/someip_config.json";
    }

    std::shared_ptr<Application> create(const std::string& name)
    {
        return Runtime::instance().create_application(name, config_path_);
    }

    std::string config_path_;
};

TEST(LgsomeipApiConstants, wildcard_and_default_values)
{
    EXPECT_EQ(kDefaultMajorVersion, 0U);
    EXPECT_EQ(kDefaultMinorVersion, 0U);
    EXPECT_EQ(kAnyMajorVersion, 0xFFU);
    EXPECT_EQ(kAnyMinorVersion, 0xFFFFFFFFU);
    EXPECT_EQ(kAnyEvent, 0xFFFFU);
}

TEST(LgsomeipApiMessage, has_neutral_default_values)
{
    const Message message;

    EXPECT_EQ(message.service, 0U);
    EXPECT_EQ(message.instance, 0U);
    EXPECT_EQ(message.method, 0U);
    EXPECT_EQ(message.client, 0U);
    EXPECT_EQ(message.session, 0U);
    EXPECT_EQ(message.interface_version, kDefaultMajorVersion);
    EXPECT_EQ(message.type, MessageType::Request);
    EXPECT_EQ(message.return_code, ReturnCode::Ok);
    EXPECT_TRUE(message.payload.empty());
}

TEST(LgsomeipApiMessage, message_types_match_the_wire_values)
{
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::Request), 0x00U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::RequestNoReturn), 0x01U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::Notification), 0x02U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::TpRequest), 0x20U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::TpRequestNoReturn), 0x21U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::TpNotification), 0x22U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::Response), 0x80U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::Error), 0x81U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::TpResponse), 0xA0U);
    EXPECT_EQ(static_cast<std::uint8_t>(MessageType::TpError), 0xA1U);
}

TEST(LgsomeipApiMessage, return_codes_match_the_wire_values)
{
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::Ok), 0x00U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::NotOk), 0x01U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::UnknownService), 0x02U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::UnknownMethod), 0x03U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::NotReady), 0x04U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::NotReachable), 0x05U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::Timeout), 0x06U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::WrongProtocolVersion), 0x07U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::WrongInterfaceVersion), 0x08U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::MalformedMessage), 0x09U);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::WrongMessageType), 0x0AU);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::E2ERepeated), 0x0BU);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::E2EWrongSequence), 0x0CU);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::E2E), 0x0DU);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::E2ENotAvailable), 0x0EU);
    EXPECT_EQ(static_cast<std::uint8_t>(ReturnCode::E2ENoNewData), 0x0FU);
}

TEST_F(LgsomeipApiTest, runtime_instance_is_a_singleton)
{
    EXPECT_EQ(&Runtime::instance(), &Runtime::instance());
}

TEST_F(LgsomeipApiTest, create_application_returns_a_new_object_each_time)
{
    const auto first = create("response");
    const auto second = create("response");

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
}

TEST_F(LgsomeipApiTest, application_reports_the_requested_name)
{
    const auto application = create("request-1");

    ASSERT_NE(application, nullptr);
    EXPECT_EQ(application->name(), "request-1");
}

TEST_F(LgsomeipApiTest, client_id_comes_from_the_configuration)
{
    EXPECT_EQ(create("response")->client_id(), 0x0111U);
    EXPECT_EQ(create("request-1")->client_id(), 0x0121U);
    EXPECT_EQ(create("request-2")->client_id(), 0x0122U);
}

TEST_F(LgsomeipApiTest, client_id_is_assigned_when_the_name_is_not_configured)
{
    const auto application = create("not-in-the-configuration");

    ASSERT_NE(application, nullptr);
    EXPECT_NE(application->client_id(), 0U);
}

TEST_F(LgsomeipApiTest, start_before_init_throws_logic_error)
{
    const auto application = create("response");

    EXPECT_THROW(application->start(), std::logic_error);
}

TEST_F(LgsomeipApiTest, join_before_init_returns_immediately)
{
    const auto application = create("response");

    EXPECT_NO_THROW(application->join());
}

}  // namespace
