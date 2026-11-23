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

#include "score/mw/com/doc/tutorial/chapter_14/gateway_application_runner.h"

#include "score/mw/com/gateway/gateway_application/configuration/gateway_config_parser.h"
#include "score/mw/com/gateway/gateway_application/gateway_application.h"
#include "score/mw/log/logging.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace score::mw::com::gateway
{

// [gateway-runner-shutdown-signal-start]
namespace
{
std::atomic<bool> g_running{true};
std::condition_variable g_shutdown_cv;
std::mutex g_shutdown_mutex;
}  // namespace

void SignalHandler(int /*signal*/) noexcept
{
    g_running.store(false, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}
// [gateway-runner-shutdown-signal-end]

void RunGatewayApplication(const std::string& config_path)
{
    // [gateway-runner-create-start]
    auto app_configuration = ParseGatewayConfig(config_path);
    GatewayApplication gateway_app{std::move(app_configuration)};
    // [gateway-runner-create-end]

    // [gateway-runner-setup-and-start-start]
    const auto setup_result = gateway_app.Setup();
    if (!setup_result.has_value())
    {
        score::mw::log::LogError() << "Gateway application setup failed: " << setup_result.error();
        return;
    }

    const auto start_result = gateway_app.Start();
    if (!start_result.has_value())
    {
        score::mw::log::LogError() << "Gateway application failed to start: " << start_result.error();
        return;
    }
    // [gateway-runner-setup-and-start-end]

    // [gateway-runner-wait-start]
    score::mw::log::LogInfo() << "Gateway application started. Press Ctrl+C to stop.";

    std::unique_lock<std::mutex> lock{g_shutdown_mutex};
    while (g_running.load(std::memory_order_relaxed))
    {
        g_shutdown_cv.wait(lock);
    }

    score::mw::log::LogInfo() << "Gateway application shutting down.";
    // [gateway-runner-wait-end]
}

}  // namespace score::mw::com::gateway
