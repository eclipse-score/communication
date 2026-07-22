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

#include "score/filesystem/path.h"
#include "score/mw/log/logging.h"
#include "score/string_manipulation/string_literal.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::com::runtime
{
namespace
{

constexpr auto kDefaultConfigurationPath = "./etc/mw_com_config.json";
constexpr auto kDeprecatedConfigurationPathCommandLineKey = std::string_view{"-service_instance_manifest"};
constexpr auto kConfigurationPathCommandLineKey = std::string_view{"--service_instance_manifest"};

}  // namespace

RuntimeConfiguration::RuntimeConfiguration() : RuntimeConfiguration{kDefaultConfigurationPath} {}

RuntimeConfiguration::RuntimeConfiguration(filesystem::Path configuration_path)
    : configuration_path_{std::move(configuration_path)}
{
}

// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
RuntimeConfiguration::RuntimeConfiguration(const std::int32_t argc, const char* argv[]) : configuration_path_{}
{
    // We convert the const char* arguments into safecpp::zstring_views to ensure that they are null-terminated and safe
    // to use.
    // This is a deprecated API for a reason! If the given argv[] is not null-terminated, it can lead to undefined
    // behavior. But this isn't introduced newly by this conversion!
    std::vector<safecpp::zstring_view> command_line_arguments{};
    for (std::int32_t arg_idx = 0U; arg_idx < argc; arg_idx++)
    {
        auto argument = std::string_view{argv[arg_idx]};
        command_line_arguments.push_back(safecpp::zstring_view{argument.data(), argument.size()});
    }

    auto configuration_path = ParseConfigurationPath(command_line_arguments);
    configuration_path_ =
        configuration_path.has_value() ? std::move(configuration_path).value() : kDefaultConfigurationPath;
}

RuntimeConfiguration::RuntimeConfiguration(cpp::span<safecpp::zstring_view> command_line_arguments)
{
    auto configuration_path = ParseConfigurationPath(command_line_arguments);
    configuration_path_ =
        configuration_path.has_value() ? std::move(configuration_path).value() : kDefaultConfigurationPath;
}

const filesystem::Path& RuntimeConfiguration::GetConfigurationPath() const&
{
    return configuration_path_;
}

std::optional<filesystem::Path> RuntimeConfiguration::ParseConfigurationPath(
    const score::cpp::span<safecpp::zstring_view> command_line_args)
{
    const auto num_args = command_line_args.size();

    std::optional<std::string> configuration_path{};
    for (std::uint32_t arg_idx = 0U; arg_idx < num_args; arg_idx++)
    {
        const std::string& command_line_argument_key{
            score::cpp::at(command_line_args, static_cast<std::ptrdiff_t>(arg_idx)).data()};
        if (command_line_argument_key == kConfigurationPathCommandLineKey)
        {
            const auto index_of_configuration_path = arg_idx + 1U;
            if (index_of_configuration_path >= num_args)
            {
                score::mw::log::LogFatal("lola")
                    << "Command line arguments contains key\"" << kConfigurationPathCommandLineKey
                    << "\" but no corresponding value. Terminating.";
                std::terminate();
            }
            return score::cpp::at(command_line_args, static_cast<std::ptrdiff_t>(index_of_configuration_path)).data();
        }
        if (command_line_argument_key == kDeprecatedConfigurationPathCommandLineKey)
        {
            score::mw::log::LogWarn("lola") << "Command line argument" << kDeprecatedConfigurationPathCommandLineKey
                                            << "is deprecated, please use" << kConfigurationPathCommandLineKey;
            const auto index_of_configuration_path = arg_idx + 1U;
            if (index_of_configuration_path >= num_args)
            {
                score::mw::log::LogFatal("lola")
                    << "Command line arguments contains key\"" << kDeprecatedConfigurationPathCommandLineKey
                    << "\" but no corresponding value. Terminating.";
                std::terminate();
            }
            return score::cpp::at(command_line_args, static_cast<std::ptrdiff_t>(index_of_configuration_path)).data();
        }
    }
    return configuration_path;
}

}  // namespace score::mw::com::runtime
