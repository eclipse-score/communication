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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "score/message_passing/log/log_on_timeout.h"

#include <thread>

namespace score::message_passing
{
namespace
{

using namespace ::testing;

TEST(LogOnTimeoutTest, ExpectNoLog)
{
    std::int32_t counter{0};
    const LoggingCallback callback = [&counter](LogSeverity /*severity*/, LogItems /*items*/) -> void {
        ++counter;
    };

    LogWarnOnTimeout guard(callback, std::chrono::milliseconds{1000000}, "Test");
    guard.release();

    EXPECT_EQ(counter, 0);
}

TEST(LogOnTimeoutTest, ExpectLog)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity severity, LogItems items) -> void {
        EXPECT_EQ(severity, LogSeverity::kWarn);
        ASSERT_EQ(items.size(), 4);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(items[1]));
        EXPECT_GE(std::get<std::int64_t>(items[1]), 1);
        ASSERT_TRUE(std::holds_alternative<std::string_view>(items[3]));
        EXPECT_EQ(std::get<std::string_view>(items[3]), std::string_view{"Test"});
        ++counter;
    };

    LogWarnOnTimeout guard(logger, std::chrono::milliseconds{1}, "Test");
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    guard.release();

    EXPECT_EQ(counter, 1);
}

}  // namespace
}  // namespace score::message_passing
