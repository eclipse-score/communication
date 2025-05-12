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

#include <score/string_view.hpp>

namespace score::mw::com::impl::configuration
{

Configuration Parse(const score::cpp::string_view path) noexcept;
Configuration Parse(score::json::Any json) noexcept;

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_PARSER_H
