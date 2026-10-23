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
#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/configuration_flatbuffer_parsing_strategy.h"
#include "score/mw/com/impl/configuration/configuration_json_parsing_strategy.h"

namespace score::mw::com::impl::configuration
{

auto Parse(const std::string_view path) -> Configuration
{
    ConfigurationJsonParsingStrategy strategy{};
    return strategy.Parse(path);
}

auto Parse(score::json::Any json) -> Configuration
{
    ConfigurationJsonParsingStrategy strategy{};
    return strategy.Parse(std::move(json));
}

#if defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)

namespace
{

bool HasFlatbufferExtension(const std::string_view path)
{
    constexpr std::string_view kBfbsExtension{".bfbs"};
    constexpr std::string_view kBinExtension{".bin"};
    const auto ends_with = [path](const std::string_view suffix) noexcept {
        return (path.size() >= suffix.size()) && (path.substr(path.size() - suffix.size()) == suffix);
    };
    return ends_with(kBfbsExtension) || ends_with(kBinExtension);
}

}  // namespace

auto ParseWithStrategySelection(const std::string_view path) -> Configuration
{
    if (HasFlatbufferExtension(path))
    {
        ConfigurationFlatbufferParsingStrategy strategy{};
        return strategy.Parse(path);
    }
    return Parse(path);
}

#else

auto ParseWithStrategySelection(const std::string_view path) -> Configuration
{
    return Parse(path);
}

#endif

}  // namespace score::mw::com::impl::configuration
