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

/// \brief Gateway executable, configurable via --mode sender|receiver.

#include "score/mw/com/gateway/gateway_application/gateway_application.h"

#include "score/mw/com/gateway/gateway_application/configuration/gateway_config_parser.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <score/span.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace
{

enum class GatewayMode
{
    kSender,
    kReceiver,
};

constexpr std::string_view kMwComConfigSender = "etc/mw_com_config_sender.json";
constexpr std::string_view kMwComConfigReceiver = "etc/mw_com_config_receiver.json";
constexpr std::string_view kGatewayConfigSender = "etc/mw_com_gateway_config_sender.json";
constexpr std::string_view kGatewayConfigReceiver = "etc/mw_com_gateway_config_receiver.json";

GatewayMode ParseMode(const int argc, const char** argv)
{
    const auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{"mode", "sender|receiver"}});
    const auto mode = score::mw::com::test::GetValue<std::string>(args, "mode");

    if (mode == "sender")
    {
        return GatewayMode::kSender;
    }
    if (mode == "receiver")
    {
        return GatewayMode::kReceiver;
    }
    std::cerr << "[Gateway] Unknown --mode value '" << mode << "', expected sender|receiver\n";
    std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int argc, const char** argv)
{
    const auto mode = ParseMode(argc, argv);

    const auto mw_com_config = (mode == GatewayMode::kSender) ? kMwComConfigSender : kMwComConfigReceiver;
    const auto gateway_config = (mode == GatewayMode::kSender) ? kGatewayConfigSender : kGatewayConfigReceiver;
    const auto mode_str = (mode == GatewayMode::kSender) ? "sender" : "receiver";

    // We need to explicitly initialize mw::com runtime, because we have a non-default config-name
    const std::string mw_com_config_str{mw_com_config};
    std::array<score::safecpp::zstring_view, 3> command_line_arguments_storage{
        "gateway", "--service_instance_manifest", mw_com_config_str};
    const score::cpp::span<score::safecpp::zstring_view> command_line_arguments{command_line_arguments_storage};
    score::mw::com::runtime::InitializeRuntime(command_line_arguments);

    score::cpp::stop_source stop_source;
    score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    const auto stop_token = stop_source.get_token();

    auto config = score::mw::com::gateway::ParseGatewayConfig(gateway_config);
    score::mw::com::gateway::GatewayApplication gateway_app(std::move(config));

    auto setup_result = gateway_app.Setup();
    if (!setup_result.has_value())
    {
        std::cerr << "[Gateway] Failed to setup transport\n";
        return EXIT_FAILURE;
    }
    std::cout << "[Gateway] Transport connected\n";

    auto start_result = gateway_app.Start();
    if (!start_result.has_value())
    {
        std::cerr << "[Gateway] Failed to start Gateway application\n";
        return EXIT_FAILURE;
    }
    std::cout << "[Gateway] Running in " << mode_str << " mode\n";

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(100ms);
    }

    std::cout << "[Gateway] Stopped\n";
    return EXIT_SUCCESS;
}
