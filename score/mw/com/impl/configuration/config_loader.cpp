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
#include "score/mw/com/impl/configuration/config_loader.h"

#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/flatbuffer_config_loader.h"
#include "score/mw/log/logging.h"

#include <cstdlib>
#include <string>

namespace score::mw::com::impl::configuration
{

namespace
{
bool ShouldUseFlatBuffer(const std::string_view path) noexcept
{
    // Auto-detect based on file extension
    const std::string_view flatbuffer_extension = ".bin";
    if (path.length() > flatbuffer_extension.length())
    {
        std::string_view extension = path.substr(path.length() - flatbuffer_extension.length());
        if (extension == flatbuffer_extension)
        {
            return true;
        }
    }
    return false;
}
}  // namespace

Configuration Load(const std::string_view path) noexcept
{
    if (ShouldUseFlatBuffer(path))
    {
        // Warning is used to see usage in the default ipc bridge test setup
        score::mw::log::LogWarn("lola") << "Loading FlatBuffer configuration from: " << path;
        return FlatBufferConfigLoader::CreateConfiguration(path);
    }

    // Warning is used to see usage in the default ipc bridge test setup
    score::mw::log::LogWarn("lola") << "Loading JSON configuration from: " << path;
    return Parse(path);
}

}  // namespace score::mw::com::impl::configuration
