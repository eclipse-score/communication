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
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

class ServiceElementTypeTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        score::mw::log::SetLogRecorder(&recorder_mock_);
        ON_CALL(recorder_mock_, StartRecord(::testing::_, score::mw::log::LogLevel::kFatal))
            .WillByDefault(::testing::Return(handle_));
    }
    void TearDown() override
    {
        // Reset the global recorder to nullptr so that it no longer points to the local recorder_mock_.
        // Without this, the global recorder would hold a dangling pointer after recorder_mock_ is destroyed,
        // causing undefined behavior if any subsequent test triggers logging.
        score::mw::log::SetLogRecorder(nullptr);
    }
    score::mw::log::RecorderMock recorder_mock_{};
    const score::mw::log::SlotHandle handle_{10};
};

TEST_F(ServiceElementTypeTest, DefaultConstructedEnumValueIsInvalid)
{
    // Given a default constructed ServiceElementType
    ServiceElementType service_element_type{};

    // Then the value of the enum should be invalid
    EXPECT_EQ(service_element_type, ServiceElementType::INVALID);
}

TEST_F(ServiceElementTypeTest, OperatorStreamOutputsInvalidWhenTypeIsInvalid)
{
    // Given a ServiceElementType set to INVALID
    ServiceElementType service_element_type = ServiceElementType::INVALID;

    // Then the logged output should contain the formatted ServiceElementType
    EXPECT_CALL(recorder_mock_, LogStringView(handle_, std::string_view{"INVALID"}));

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
}

TEST_F(ServiceElementTypeTest, OperatorStreamOutputsEventWhenTypeIsEvent)
{
    // Given a ServiceElementType set to EVENT
    ServiceElementType service_element_type = ServiceElementType::EVENT;

    // Then the logged output should contain the formatted ServiceElementType
    EXPECT_CALL(recorder_mock_, LogStringView(handle_, std::string_view{"EVENT"}));

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
}

TEST_F(ServiceElementTypeTest, OperatorStreamOutputsFieldWhenTypeIsField)
{
    // Given a ServiceElementType set to FIELD
    ServiceElementType service_element_type = ServiceElementType::FIELD;

    // Then the logged output should contain the formatted ServiceElementType
    EXPECT_CALL(recorder_mock_, LogStringView(handle_, std::string_view{"FIELD"}));

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
}

TEST_F(ServiceElementTypeTest, OperatorStreamOutputsMethodWhenTypeIsMethod)
{
    // Given a ServiceElementType set to METHOD
    ServiceElementType service_element_type = ServiceElementType::METHOD;

    // Then the logged output should contain the formatted ServiceElementType
    EXPECT_CALL(recorder_mock_, LogStringView(handle_, std::string_view{"METHOD"}));

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
}

TEST_F(ServiceElementTypeTest, OperatorStreamOutputsUnknownWhenTypeIsUnrecognized)
{
    // Given a ServiceElementType set to UNKNOWN
    ServiceElementType service_element_type = static_cast<ServiceElementType>(100U);

    // Then the logged output should contain the formatted ServiceElementType
    EXPECT_CALL(recorder_mock_, LogStringView(handle_, std::string_view{"UNKNOWN"}));

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
}

}  // namespace
}  // namespace score::mw::com::impl
