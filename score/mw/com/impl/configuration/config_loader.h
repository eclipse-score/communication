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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_LOADER_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_LOADER_H

#include "score/mw/com/impl/configuration/configuration.h"

#include <string_view>

namespace score::mw::com::impl::configuration
{

/**
 * @brief Load configuration from a file path.
 * 
 * This function automatically detects the file format (JSON or FlatBuffer binary)
 * and uses the appropriate parser. The selection is based on the file extension:
 * - ".json" for JSON format
 * - ".bin" for FlatBuffer binary format
 * 
 * @param path Path to the configuration file (.json or .bin)
 * @return Configuration object populated from the file
 */
Configuration Load(const std::string_view path) noexcept;

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_LOADER_H
