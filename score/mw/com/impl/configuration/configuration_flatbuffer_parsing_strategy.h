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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_FLATBUFFER_PARSING_STRATEGY_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_FLATBUFFER_PARSING_STRATEGY_H

#include "score/mw/com/impl/configuration/i_configuration_parsing_strategy.h"

#include <score/span.hpp>

#include <cstdint>
#include <string_view>

namespace score::mw::com::impl::configuration
{

/// \brief Parses FlatBuffer-encoded (see mw_com_config.fbs) mw::com configuration files into a Configuration object.
///
/// This class is only built when the (experimental) FlatBuffers configuration flag is enabled (--config=flatbuffers,
/// see //score/mw/com/flags:experimental_enable_flatbuffers_configuration). Without the flag, the
/// :configuration_flatbuffer_parsing_strategy target is incompatible, so dependents must only reference it under
/// if_flatbuffers_enabled() (see //score/mw/com/flags:flags.bzl) and guard their use of it with
/// SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED.
class ConfigurationFlatbufferParsingStrategy final : public IConfigurationParsingStrategy
{
  public:
    Configuration Parse(std::string_view path) const override;
    Configuration Parse(score::cpp::span<const std::uint8_t> buffer) const;
};

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_FLATBUFFER_PARSING_STRATEGY_H
