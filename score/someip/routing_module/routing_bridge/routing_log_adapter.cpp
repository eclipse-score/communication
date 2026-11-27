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

#include "score/someip/routing_module/udp_transport/log.h"

#include "score/mw/log/logging.h"

#include <string_view>

extern "C" void someip_routing_log(const someip_log_severity severity, const char* const message)
{
    const std::string_view text{message};
    switch (severity)
    {
        case SOMEIP_LOG_ERROR:
            score::mw::log::LogError("SRTM") << text;
            break;
        case SOMEIP_LOG_WARNING:
            score::mw::log::LogWarn("SRTM") << text;
            break;
        case SOMEIP_LOG_INFO:
            score::mw::log::LogInfo("SRTM") << text;
            break;
        case SOMEIP_LOG_DEBUG:
        default:
            score::mw::log::LogDebug("SRTM") << text;
            break;
    }
}
