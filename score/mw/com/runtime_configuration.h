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
#ifndef SCORE_MW_COM_RUNTIME_CONFIGURATION_H
#define SCORE_MW_COM_RUNTIME_CONFIGURATION_H

#include "score/filesystem/path.h"
#include "score/memory/string_literal.h"

#include <score/span.hpp>

#include <cstdint>
#include <optional>

namespace score::mw::com::runtime
{
/**
 * \api
 * \brief RuntimeConfiguration object which a user can construct and pass to the public InitializeRuntime call
 * \public
 */
class RuntimeConfiguration
{
  public:
    /**
     * \brief Default constructor which initialiases the stored configuration path with a default value.
     * \details It can be useful for debugging purposes for an application to want to explicitly initialize the runtime
     * using defaults to check configuration parsing.
     */
    RuntimeConfiguration();

    /**
     * \api
     * \brief Constructor which initialiases the stored configuration path from command line arguments.
     * \details The constructor parses the command line arguments for a specific key to extract the configuration
     *          path. If the key is not found, a default path is used.
     * \param argc The number of command line arguments.
     * \param argv The array of command line arguments.
     * \public
     */
    // NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
    RuntimeConfiguration(const std::int32_t argc, score::StringLiteral argv[]);

    /**
     * \api
     * \brief Constructor which initialiases the stored configuration path with the provided path.
     * \param configuration_path The configuration path to be stored.
     * \public
     */
    explicit RuntimeConfiguration(filesystem::Path configuration_path);

    /**
     * \api
     * \brief Returns the stored configuration path.
     * \return The stored configuration path.
     * \public
     */
    const filesystem::Path& GetConfigurationPath() const;

  private:
    static std::optional<filesystem::Path> ParseConfigurationPath(
        const score::cpp::span<const score::StringLiteral> command_line_args);

    filesystem::Path configuration_path_;
};

}  // namespace score::mw::com::runtime

#endif  // SCORE_MW_COM_RUNTIME_CONFIGURATION_H
