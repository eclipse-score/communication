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
#include "score/mw/com/runtime_configuration.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::runtime
{
namespace
{

using ::testing::_;
using ::testing::Return;

constexpr auto kDefaultConfigurationPath = "./etc/mw_com_config.json";
constexpr auto kDummyConfigurationPath = "/my/configuration/path/mw_com_config.json";

using safecpp::literals::operator""_zsv;
constexpr auto kDummyApplicationNameZsv = "dummyname"_zsv;
constexpr auto kConfigurationPathCommandLineKeyZsv = "--service_instance_manifest"_zsv;
constexpr auto kDeprecatedConfigurationPathCommandLineKeyZsv = "-service_instance_manifest"_zsv;
constexpr auto kDummyConfigurationPathZsv = "/my/configuration/path/mw_com_config.json"_zsv;

TEST(RuntimeConfigurationStringConstructorTest, ConfigurationPathContainsStringPassedToConstructor)
{
    // When constructing a RuntimeConfiguration with a configuration path
    const RuntimeConfiguration runtime_configuration{kDefaultConfigurationPath};

    // Then the stored configuration path should be the same as the path provided to the constructor
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfNoPathPassedToConstructor)
{
    // When constructing a RuntimeConfiguration with no configuration path
    const RuntimeConfiguration runtime_configuration{};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsPathInCommandLineArgs)
{
    // Given command line arguments which contain a configuration path key and configuration path
    std::vector<safecpp::zstring_view> arguments = {
        kDummyApplicationNameZsv, kConfigurationPathCommandLineKeyZsv, kDummyConfigurationPathZsv};

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Then the stored configuration path should be the same as the path provided in the command line arguments
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDummyConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationZstringPathContainsPathInCommandLineArgs)
{
    // Given command line arguments which contain a configuration path key and configuration path
    std::vector<safecpp::zstring_view> arguments = {
        kDummyApplicationNameZsv, kConfigurationPathCommandLineKeyZsv, kDummyConfigurationPathZsv};

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Then the stored configuration path should be the same as the path provided in the command line arguments
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDummyConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, DeprecatedConfigurationCommandLineArg)
{
    // Given command line arguments which contain the deprecated argument
    std::vector<safecpp::zstring_view> arguments = {
        kDummyApplicationNameZsv, kDeprecatedConfigurationPathCommandLineKeyZsv, kDummyConfigurationPathZsv};

    // Given a log recorder mock
    score::mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);
    score::mw::log::SlotHandle handle{10};
    ON_CALL(recorder_mock, StartRecord(::testing::_, score::mw::log::LogLevel::kWarn))
        .WillByDefault(::testing::Return(handle));

    // Then a deprecation warning should be logged containing the key phrase that confirms this is a deprecation notice
    EXPECT_CALL(recorder_mock, StartRecord(std::string_view{"lola"}, score::mw::log::LogLevel::kWarn)).Times(1);
    // Catch-all for other LogStringView calls (the key names are also streamed as separate tokens);
    // registered first so the specific check below has higher LIFO priority.
    EXPECT_CALL(recorder_mock, LogStringView(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(recorder_mock, LogStringView(handle, std::string_view{"is deprecated, please use"}));

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Reset the global recorder to nullptr so that it no longer points to the local recorder_mock.
    score::mw::log::SetLogRecorder(nullptr);

    // Then the configuration path should still work for backward compatibility
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDummyConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfNoPathKeyInCommandLineArgs)
{
    // Given command line arguments which do not contain a configuration path key
    std::vector<safecpp::zstring_view> arguments = {kDummyApplicationNameZsv, kDummyConfigurationPathZsv};

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfNoPathOrKeyInCommandLineArgs)
{
    // Given command line arguments which do not contain a configuration path key or configuration path
    std::vector<safecpp::zstring_view> arguments = {kDummyApplicationNameZsv};

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfCommandLineArgumentsEmpty)
{
    // Given command line arguments which are empty
    std::vector<safecpp::zstring_view> arguments = {};

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{arguments};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorDeathTest, TerminatesIfCommandLineArgsContainPathKeyButNoPath)
{
    // Given command line arguments which contain a configuration path key but no configuration path
    std::vector<safecpp::zstring_view> arguments = {kDummyApplicationNameZsv, kConfigurationPathCommandLineKeyZsv};
    cpp::span<safecpp::zstring_view> command_line_arguments(arguments.data(), arguments.size());

    // When constructing a RuntimeConfiguration
    // Then the process terminates
    EXPECT_DEATH(RuntimeConfiguration{command_line_arguments}, ".*");
}

TEST(RuntimeConfigurationCommandLineConstructorDeathTest, TerminatesIfCommandLineArgsContainDeprecatedPathKeyButNoPath)
{
    // Given command line arguments which contain a configuration path key but no configuration path
    std::vector<safecpp::zstring_view> arguments = {kDummyApplicationNameZsv,
                                                    kDeprecatedConfigurationPathCommandLineKeyZsv};
    cpp::span<safecpp::zstring_view> command_line_arguments(arguments.data(), arguments.size());

    // When constructing a RuntimeConfiguration
    // Then the process terminates
    EXPECT_DEATH(RuntimeConfiguration{command_line_arguments}, ".*");
}

}  // namespace
}  // namespace score::mw::com::runtime
