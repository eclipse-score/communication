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

#include "score/message_passing/log/log.h"

#include <score/utility.hpp>

#include <thread>

namespace score::message_passing
{
namespace
{

using namespace ::testing;

TEST(LogTest, EmptyLogger)
{
    LoggingCallback logger;
    LogFatal(logger, "Test");
    LogError(logger, "Test");
    LogWarn(logger, "Test");
    LogInfo(logger, "Test");
    LogDebug(logger, "Test");
    LogVerbose(logger, "Test");
}

TEST(LogTest, LoggerSeverity)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity severity, LogItems items) -> void {
        ASSERT_EQ(items.size(), 1);
        ASSERT_TRUE(std::holds_alternative<std::uint64_t>(items[0]));
        EXPECT_EQ(std::get<std::uint64_t>(items[0]), score::cpp::to_underlying(severity));
        ++counter;
    };

    LogFatal(logger, score::cpp::to_underlying(LogSeverity::kFatal));
    LogError(logger, score::cpp::to_underlying(LogSeverity::kError));
    LogWarn(logger, score::cpp::to_underlying(LogSeverity::kWarn));
    LogInfo(logger, score::cpp::to_underlying(LogSeverity::kInfo));
    LogDebug(logger, score::cpp::to_underlying(LogSeverity::kDebug));
    LogVerbose(logger, score::cpp::to_underlying(LogSeverity::kVerbose));

    EXPECT_EQ(counter, 6);
}

TEST(LogTest, UintLogger)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<std::uint64_t>(item));
            EXPECT_EQ(std::get<std::uint64_t>(item), 42UL);
            ++counter;
        }
    };

    const std::uint8_t u8{42U};
    const std::uint16_t u16{42U};
    const std::uint32_t u32{42U};
    const std::uint64_t u64{42UL};

    LogInfo(logger, u8, u16, u32, u64);

    EXPECT_EQ(counter, 4);
}

TEST(LogTest, IntLogger)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<std::int64_t>(item));
            EXPECT_EQ(std::get<std::int64_t>(item), 42L);
            ++counter;
        }
    };

    const std::int8_t i8{42U};
    const std::int16_t i16{42U};
    const std::int32_t i32{42U};
    const std::int64_t i64{42UL};

    LogInfo(logger, i8, i16, i32, i64);

    EXPECT_EQ(counter, 4);
}

TEST(LogTest, StringViewLogger)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<std::string_view>(item));
            EXPECT_EQ(std::get<std::string_view>(item), std::string_view{"Test"});
            ++counter;
        }
    };

    const char test_array[] = "Test";
    const char* const test_ptr{test_array};
    const std::string test_string{test_array};
    const std::string_view test_string_view{test_string};

    LogInfo(logger, test_array, test_ptr, test_string, test_string_view);

    EXPECT_EQ(counter, 4);
}

TEST(LogTest, EmptyStringViewLogger)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<std::string_view>(item));
            EXPECT_EQ(std::get<std::string_view>(item), std::string_view{});
            ++counter;
        }
    };

    const char test_array[] = "";
    const char* const test_ptr{test_array};
    const std::string test_string{};
    const std::string_view test_string_view{};

    LogInfo(logger, test_array, test_ptr, test_string, test_string_view);

    EXPECT_EQ(counter, 4);
}

TEST(LogTest, StringViewReferenceWrapperLogger)
{
    std::int32_t counter{0};
    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<std::string_view>(item));
            EXPECT_EQ(std::get<std::string_view>(item), std::string_view{"Test"});
            ++counter;
        }
    };

    const char test_array[] = "Test";
    const char* const test_ptr{test_array};
    const std::string test_string{test_array};
    const std::string_view test_string_view{test_string};

    LogInfo(logger, std::cref(test_array), std::ref(test_ptr), std::cref(test_string), std::ref(test_string_view));

    EXPECT_EQ(counter, 4);
}

TEST(LogTest, PointerLogger)
{
    std::int32_t counter{0};

    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<const void*>(item));
            EXPECT_EQ(std::get<const void*>(item), &counter);
            ++counter;
        }
    };

    LogInfo(logger, &counter, const_cast<const std::int32_t*>(&counter));

    EXPECT_EQ(counter, 2);
}

TEST(LogTest, NullptrLogger)
{
    std::int32_t counter{0};

    const LoggingCallback logger = [&counter](LogSeverity /*severity*/, LogItems items) -> void {
        for (auto item : items)
        {
            ASSERT_TRUE(std::holds_alternative<const void*>(item));
            EXPECT_EQ(std::get<const void*>(item), nullptr);
            ++counter;
        }
    };

    LogInfo(logger, nullptr);

    EXPECT_EQ(counter, 1);
}

}  // namespace
}  // namespace score::message_passing
