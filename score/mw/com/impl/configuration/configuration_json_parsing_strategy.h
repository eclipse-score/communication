/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_JSON_PARSING_STRATEGY_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_JSON_PARSING_STRATEGY_H

#include "score/mw/com/impl/configuration/i_configuration_parsing_strategy.h"

#include "score/json/json_parser.h"

#include <string_view>

namespace score::mw::com::impl::configuration
{

class ConfigurationJsonParsingStrategy final : public IConfigurationParsingStrategy
{
  public:
    Configuration Parse(std::string_view path) const override;
    Configuration Parse(score::json::Any json) const;
};

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_JSON_PARSING_STRATEGY_H
