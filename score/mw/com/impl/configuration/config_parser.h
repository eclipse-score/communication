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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_PARSER_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_PARSER_H

#include "score/json/json_parser.h"
#include "score/mw/com/impl/configuration/configuration.h"

#include <string_view>

namespace score::mw::com::impl::configuration
{

Configuration Parse(const std::string_view path);
Configuration Parse(score::json::Any json);

/// \brief Parses the configuration file at the given path with a parsing strategy selected by its file extension.
/// \details Without the (experimental) FlatBuffers configuration flag, this is identical to Parse(path), i.e. the file
///          is always parsed as JSON. With the flag enabled, files ending in ".bfbs" or ".bin" are parsed with
///          ConfigurationFlatbufferParsingStrategy, all others (in particular ".json") with
///          ConfigurationJsonParsingStrategy.
Configuration ParseWithStrategySelection(const std::string_view path);

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_PARSER_H
