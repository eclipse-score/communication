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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(ServiceElementTypeTest, DefaultConstructedEnumValueIsInvalid)
{
    // Given a default constructed ServiceElementType
    ServiceElementType service_element_type{};

    // Then the value of the enum should be invalid
    EXPECT_EQ(service_element_type, ServiceElementType::INVALID);
}

TEST(ServiceElementTypeTest, OperatorStreamOutputsInvalidWhenTypeIsInvalid)
{
    // Given a ServiceElementType set to INVALID
    ServiceElementType service_element_type = ServiceElementType::INVALID;
    testing::internal::CaptureStdout();

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the formatted ServiceElementType
    EXPECT_THAT(output, ::testing::HasSubstr("INVALID"));
}

TEST(ServiceElementTypeTest, OperatorStreamOutputsEventWhenTypeIsEvent)
{
    // Given a ServiceElementType set to EVENT
    ServiceElementType service_element_type = ServiceElementType::EVENT;
    testing::internal::CaptureStdout();

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the formatted ServiceElementType
    EXPECT_THAT(output, ::testing::HasSubstr("EVENT"));
}

TEST(ServiceElementTypeTest, OperatorStreamOutputsFieldWhenTypeIsField)
{
    // Given a ServiceElementType set to FIELD
    ServiceElementType service_element_type = ServiceElementType::FIELD;
    testing::internal::CaptureStdout();

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the formatted ServiceElementType
    EXPECT_THAT(output, ::testing::HasSubstr("FIELD"));
}

TEST(ServiceElementTypeTest, OperatorStreamOutputsUnknownWhenTypeIsUnrecognized)
{
    // Given a ServiceElementType set to UNKNOWN
    ServiceElementType service_element_type = static_cast<ServiceElementType>(100U);
    testing::internal::CaptureStdout();

    // When streaming the ServiceElementType to a log
    score::mw::log::LogFatal("test") << service_element_type;
    std::string output = testing::internal::GetCapturedStdout();

    // Then the output should contain the formatted ServiceElementType
    EXPECT_THAT(output, ::testing::HasSubstr("UNKNOWN"));
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
