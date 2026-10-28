/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
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

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace {

class CoutCapture {
public:
    CoutCapture() : original_buffer_(std::cout.rdbuf(captured_.rdbuf())) {}

    ~CoutCapture() {
        std::cout.rdbuf(original_buffer_);
    }

    std::string str() const {
        return captured_.str();
    }

private:
    std::ostringstream captured_;
    std::streambuf* original_buffer_;
};

void set_log_level(lgsomeip::Logger& instance, const std::string& level) {
    CoutCapture capture;
    instance.init(level);
}

void emit_stream_log() {
    LGSOMEIP_LOG_INFO << "stream message " << 42;
}

void emit_location_log() {
    LGSOMEIP_LOG_INFO << "located message";
}

TEST(LoggerTest, AcceptsConfiguredLevelNames) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "debug");

    CoutCapture capture;
    LGSOMEIP_LOG_DEBUG << "debug message";

    EXPECT_NE(capture.str().find("[Debug]"), std::string::npos);
    EXPECT_NE(capture.str().find("debug message"), std::string::npos);
}

TEST(LoggerTest, OmitsSourceLocationFromDefaultOutput) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "info");

    CoutCapture capture;
    emit_location_log();

    EXPECT_EQ(capture.str().find("LoggerTest.cpp:"), std::string::npos);
    EXPECT_EQ(capture.str().find("emit_location_log"), std::string::npos);
    EXPECT_NE(capture.str().find("located message"), std::string::npos);
}

TEST(LoggerTest, StreamsMessageDirectly) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "info");

    CoutCapture capture;
    emit_stream_log();

    EXPECT_NE(capture.str().find("stream message 42"), std::string::npos);
}

TEST(LoggerTest, SkipsDisabledStreamExpression) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "info");

    int evaluated = 0;
    CoutCapture capture;
    LGSOMEIP_LOG_DEBUG << ++evaluated;

    EXPECT_EQ(evaluated, 0);
    EXPECT_TRUE(capture.str().empty());
}

TEST(LoggerTest, FiltersBeforeEvaluatingTheMessage) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "info");

    int evaluated = 0;
    CoutCapture capture;
    LGSOMEIP_LOG_DEBUG << (++evaluated);

    EXPECT_FALSE(evaluated);
    EXPECT_TRUE(capture.str().empty());
}

TEST(LoggerTest, WritesConfiguredFileSink) {
    auto& instance = lgsomeip::Logger::instance();
    const std::string path = "logger_file_test.log";
    std::remove(path.c_str());

    instance.init("info", false, true, path);
    LGSOMEIP_LOG_INFO << "file message";

    std::ifstream file(path);
    std::stringstream contents;
    contents << file.rdbuf();

    EXPECT_NE(contents.str().find("[Info]"), std::string::npos);
    EXPECT_NE(contents.str().find("file message"), std::string::npos);

    set_log_level(instance, "info");
    std::remove(path.c_str());
}

TEST(FormatLogTest, FormatsRelatedIdsWithConsistentBrackets) {
    EXPECT_EQ(lgsomeip::message_id_to_string(static_cast<std::uint8_t>(0xab), 2), "ab");
    EXPECT_EQ(lgsomeip::message_id_to_string(static_cast<std::uint8_t>(0xab), 4), "00ab");
    EXPECT_EQ(lgsomeip::format_service_instance_id(0x1234, 0x0002), "[1234:0002]");
    EXPECT_EQ(lgsomeip::format_service_instance_event_id(0x1234, 0x0002, 0x00ab), "[1234:0002.00ab]");
    EXPECT_EQ(lgsomeip::format_service_instance_interface_version(0x1234, 0x0002, 0x01, 0x00000009),
              "[1234:0002] InterfaceVersion[01.00000009]");
    EXPECT_EQ(lgsomeip::format_service_instance_interface_major_version(0x1234, 0x0002, 0x01),
              "[1234:0002] InterfaceMajorVersion[01]");
    EXPECT_EQ(lgsomeip::format_named_id("EventID", 0x00ab, 4), "EventID[00ab]");
}

TEST(FormatLogTest, CapsByteMessageDump) {
    auto& instance = lgsomeip::Logger::instance();
    set_log_level(instance, "verbose");

    std::array<std::uint8_t, lgsomeip::kMaxByteMessageDumpLength + 1> message{};
    CoutCapture capture;
    lgsomeip::print_byte_message("test", message.data(), message.size());

    EXPECT_NE(capture.str().find("byte dump truncated at 256 bytes"), std::string::npos);
    set_log_level(instance, "info");
}

} // namespace