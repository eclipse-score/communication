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

#include "score/mw/com/impl/bindings/lola/messaging/mw_log_logger.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

const score::mw::log::SlotHandle HANDLE{42};

class MwLogLoggerFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        score::mw::log::SetLogRecorder(&recorder_);
    }

    void TearDown() override
    {
        score::mw::log::SetLogRecorder(&score::mw::log::GetDefaultLogRecorder());
    }

    StrictMock<score::mw::log::RecorderMock> recorder_{};
};

TEST_F(MwLogLoggerFixture, InvalidSeverityCallsNothing)
{
    // given MwLogLogger and a strict mw::log::Recorder mock
    auto logger = GetMwLogLogger();

    using score::message_passing::LogSeverity;
    constexpr auto kLogLevels = score::cpp::to_underlying(LogSeverity::kVerbose) + 1U;
    constexpr auto kInvalidSeverity = static_cast<LogSeverity>(kLogLevels);

    // when calling the logger with invalid seveity value
    logger(kInvalidSeverity, score::message_passing::LogItems());

    // expect nothing is called
}

TEST_F(MwLogLoggerFixture, ValidSeveritiesMapAsExpected)
{
    // given MwLogLogger and a strict mw::log::Recorder mock
    auto logger = GetMwLogLogger();

    // and an array of mappings of valid severity levels to expected log level counterparts
    using score::message_passing::LogSeverity;
    using score::mw::log::LogLevel;
    constexpr auto kLogLevels = score::cpp::to_underlying(LogSeverity::kVerbose) + 1U;
    constexpr std::array<std::pair<LogSeverity, LogLevel>, kLogLevels> mapping{
        {{LogSeverity::kFatal, LogLevel::kFatal},
         {LogSeverity::kError, LogLevel::kError},
         {LogSeverity::kWarn, LogLevel::kWarn},
         // temporary replacement for Ticket-235378 debugging:
         {LogSeverity::kInfo, LogLevel::kWarn},
         {LogSeverity::kDebug, LogLevel::kWarn},
         {LogSeverity::kVerbose, LogLevel::kWarn}}};

    // expect the recorder receives the expected log levels in sequence
    InSequence is;
    for (const auto& pair : mapping)
    {
        EXPECT_CALL(recorder_, StartRecord("mp_2", pair.second)).WillOnce(Return(HANDLE));
        EXPECT_CALL(recorder_, StopRecord(_)).Times(1);
    }

    // when the respective severity levels are requested
    for (const auto& pair : mapping)
    {
        logger(pair.first, score::message_passing::LogItems());
    }
}

TEST_F(MwLogLoggerFixture, ValidLogItemTypesMapAsExpected)
{
    // given MwLogLogger and a strict mw::log::Recorder mock
    auto logger = GetMwLogLogger();

    // and an array of different valid LogItem types
    const std::array<score::message_passing::LogItem, 4> ItemTypes{
        {std::string_view{"test"}, std::int64_t{1}, std::uint64_t{2}, nullptr}};

    // expect the recorder receives the respective log requests in sequence
    InSequence is;
    EXPECT_CALL(recorder_, StartRecord(_, _)).WillOnce(Return(HANDLE));
    EXPECT_CALL(recorder_, LogStringView(_, "test")).Times(1);
    EXPECT_CALL(recorder_, LogInt64(_, 1L)).Times(1);
    EXPECT_CALL(recorder_, LogUint64(_, 2UL)).Times(1);
    EXPECT_CALL(recorder_, LogUint64(_, 0UL)).Times(1);  // no mockable LogHex64
    EXPECT_CALL(recorder_, StopRecord(_)).Times(1);

    // when loggging of this array is requested
    logger(score::message_passing::LogSeverity::kFatal, score::message_passing::LogItems(ItemTypes));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
