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
#include "score/memory/string_literal.h"
#include "score/mw/log/logging.h"

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
constexpr auto kConfigurationPathCommandLineKey = "-service_instance_manifest";

}  // namespace

RuntimeConfiguration::RuntimeConfiguration() : RuntimeConfiguration{kDefaultConfigurationPath} {}

RuntimeConfiguration::RuntimeConfiguration(filesystem::Path configuration_path)
    : configuration_path_{std::move(configuration_path)}
{
}

// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
RuntimeConfiguration::RuntimeConfiguration(const std::int32_t argc, score::StringLiteral argv[]) : configuration_path_{}
{
    const score::cpp::span<const score::StringLiteral> command_line_arguments(argv, argc);
    auto configuration_path = ParseConfigurationPath(command_line_arguments);
    configuration_path_ =
        configuration_path.has_value() ? std::move(configuration_path).value() : kDefaultConfigurationPath;
}

const filesystem::Path& RuntimeConfiguration::GetConfigurationPath() const
{
    return configuration_path_;
}

std::optional<filesystem::Path> RuntimeConfiguration::ParseConfigurationPath(
    const score::cpp::span<const score::StringLiteral> command_line_args)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(command_line_args.size() >= 0);
    const auto num_args = static_cast<std::uint32_t>(command_line_args.size());

    std::optional<std::string> configuration_path{};
    for (std::uint32_t arg_idx = 0; arg_idx < num_args; arg_idx++)
    {
        const std::string& command_line_argument_key{score::cpp::at(command_line_args, static_cast<std::ptrdiff_t>(arg_idx))};
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
            return score::cpp::at(command_line_args, static_cast<std::ptrdiff_t>(index_of_configuration_path));
        }
    }
    return configuration_path;
}

}  // namespace score::mw::com::runtime
