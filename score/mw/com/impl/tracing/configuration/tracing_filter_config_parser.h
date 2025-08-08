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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_PARSER_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_PARSER_H

#include "score/json/json_parser.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"

#include <string_view>

namespace score::mw::com::impl::tracing
{

/// \brief Parses a given trace-filter-configuration json file under the given path
/// \param path location, where the json file resides.
/// \return on success a valid tracing filter config will be returned
score::Result<TracingFilterConfig> Parse(const std::string_view path, const Configuration& configuration) noexcept;

/// \brief Parses a trace-filter-configuration json from the given json object.
/// \param json
/// \return on success a valid tracing filter config will be returned
score::Result<TracingFilterConfig> Parse(score::json::Any json, const Configuration& configuration) noexcept;

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_PARSER_H
