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

#include "score/memory/string_literal.h"

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

constexpr auto kDeprecatedConfigurationPathCommandLineKey = "-service_instance_manifest";
constexpr auto kConfigurationPathCommandLineKey = "--service_instance_manifest";
constexpr auto kDefaultConfigurationPath = "./etc/mw_com_config.json";

constexpr auto kDummyConfigurationPath = "/my/configuration/path/mw_com_config.json";
constexpr auto kDummyApplicationName = "dummyname";

std::pair<std::int32_t, score::StringLiteral*> GenerateCommandLineArgs(std::vector<score::StringLiteral>& arguments)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT(arguments.size() < std::numeric_limits<std::int32_t>::max());
    const auto number_of_args = static_cast<std::int32_t>(arguments.size());
    return std::make_pair(number_of_args, arguments.data());
}

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
    std::vector<score::StringLiteral> arguments = {
        kDummyApplicationName, kConfigurationPathCommandLineKey, kDummyConfigurationPath};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{argc, argv};

    // Then the stored configuration path should be the same as the path provided in the command line arguments
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDummyConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, DeprecatedConfigurationCommandLineArg)
{
    // Given command line arguments which contain the deprecated argument
    std::vector<score::StringLiteral> arguments = {
        kDummyApplicationName, kDeprecatedConfigurationPathCommandLineKey, kDummyConfigurationPath};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // Given a mocked stdout to capture logs
    testing::internal::CaptureStdout();

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{argc, argv};

    // Then the configuration path should still work for backward compatibility
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDummyConfigurationPath);

    // Then the output should contain the deprecation warning of the command line argument
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, ::testing::HasSubstr("is deprecated"));
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfNoPathKeyInCommandLineArgs)
{
    // Given command line arguments which do not contain a configuration path key
    std::vector<score::StringLiteral> arguments = {kDummyApplicationName, kDummyConfigurationPath};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{argc, argv};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfNoPathOrKeyInCommandLineArgs)
{
    // Given command line arguments which do not contain a configuration path key or configuration path
    std::vector<score::StringLiteral> arguments = {kDummyApplicationName};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{argc, argv};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorTest, ConfigurationPathContainsDefaultPathIfCommandLineArgumentsEmpty)
{
    // Given command line arguments which are empty
    std::vector<score::StringLiteral> arguments = {};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // When constructing a RuntimeConfiguration
    const RuntimeConfiguration runtime_configuration{argc, argv};

    // Then the stored configuration path should be the default configuration path
    const auto& stored_configuration_path = runtime_configuration.GetConfigurationPath();
    EXPECT_EQ(stored_configuration_path.Native(), kDefaultConfigurationPath);
}

TEST(RuntimeConfigurationCommandLineConstructorDeathTest, TerminatesIfCommandLineArgsContainPathKeyButNoPath)
{
    // Given command line arguments which contain a configuration path key but no configuration path
    std::vector<score::StringLiteral> arguments = {kDummyApplicationName, kConfigurationPathCommandLineKey};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // Given a mocked stdout to capture logs
    testing::internal::CaptureStdout();

    // When constructing a RuntimeConfiguration
    // Then the process terminates
    EXPECT_DEATH(RuntimeConfiguration(argc, argv), ".*");

    // Then the output should contain the termination reason
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, ::testing::HasSubstr("no corresponding value"));

}

TEST(RuntimeConfigurationCommandLineConstructorDeathTest, TerminatesIfCommandLineArgsContainDeprecatedPathKeyButNoPath)
{
    // Given command line arguments which contain a configuration path key but no configuration path
    std::vector<score::StringLiteral> arguments = {kDummyApplicationName, kDeprecatedConfigurationPathCommandLineKey};
    auto [argc, argv] = GenerateCommandLineArgs(arguments);

    // Given a mocked stdout to capture logs
    testing::internal::CaptureStdout();

    // When constructing a RuntimeConfiguration
    // Then the process terminates
    EXPECT_DEATH(RuntimeConfiguration(argc, argv), ".*");

    // Then the output should contain the termination reason
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, ::testing::HasSubstr("no corresponding value"));
}

}  // namespace
}  // namespace score::mw::com::runtime
