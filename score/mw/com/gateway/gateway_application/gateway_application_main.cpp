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

#include "score/filesystem/path.h"
#include "score/mw/com/gateway/gateway_application/gateway_application_runner.h"
#include "score/mw/com/runtime.h"
#include "score/mw/log/logging.h"

#include <score/span.hpp>

#include <csignal>
#include <cstddef>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        score::mw::log::LogError() << "Usage: gateway_application_bin <mw_com_config-path> <gateway-config-path>";
        return 1;
    }

    std::signal(SIGINT, score::mw::com::gateway::SignalHandler);
    std::signal(SIGTERM, score::mw::com::gateway::SignalHandler);

    const score::cpp::span<char*> args{argv, static_cast<std::size_t>(argc)};
    const std::string mw_com_config_path = args[1];
    const std::string gateway_config_path = args[2];
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{mw_com_config_path}});

    score::mw::com::gateway::RunGatewayApplication(gateway_config_path);

    return 0;
}
