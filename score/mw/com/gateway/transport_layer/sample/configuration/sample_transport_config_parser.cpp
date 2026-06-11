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
#include "score/mw/com/gateway/transport_layer/sample/configuration/sample_transport_config_parser.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"
#include "score/network/ipv4_address.h"

#include <score/assert.hpp>

#include <string_view>

namespace score::mw::com::gateway
{
namespace
{

using std::string_view_literals::operator""sv;

constexpr auto kHvSocketConfigurationKey = "hypervisor-socket"sv;
constexpr auto kRemoteIpKey = "remote-ip"sv;
constexpr auto kLocalPortKey = "local-port"sv;
constexpr auto kRemotePortKey = "remote-port"sv;
constexpr auto kRequestTimeoutMsKey = "request-timeout-ms"sv;

}  // namespace

auto ParseSampleTransportConfig(const std::string_view path) noexcept -> HyperVisorSocketConfiguration
{
    const score::json::JsonParser json_parser_obj;
    // NOLINTNEXTLINE(score-banned-function): AoU of score::json::JsonParser — caller must guarantee path integrity.
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing sample transport config file" << path << "failed with error:" << json_result.error().Message()
            << ": " << json_result.error().UserMessage() << " . Terminating.";
        std::terminate();
    }
    return ParseSampleTransportConfig(std::move(json_result).value());
}

auto ParseSampleTransportConfig(score::json::Any json) noexcept -> HyperVisorSocketConfiguration
{
    auto top_level_object = json.As<score::json::Object>();
    if (!top_level_object.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing sample transport configuration failed: Expected top-level JSON object. Terminating.";
        std::terminate();
    }

    const auto& obj = top_level_object.value().get();

    const auto socket_entry = obj.find(kHvSocketConfigurationKey.data());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(socket_entry != obj.cend(),
                                                "hypervisor-socket is mandatory in sample transport configuration.");

    const auto& socket_obj = socket_entry->second.As<score::json::Object>().value().get();

    const auto remote_ip = socket_obj.find(kRemoteIpKey.data());
    const auto local_port = socket_obj.find(kLocalPortKey.data());
    const auto remote_port = socket_obj.find(kRemotePortKey.data());

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(remote_ip != socket_obj.cend(),
                                                "remote-ip is mandatory in hypervisor-socket configuration.");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(local_port != socket_obj.cend(),
                                                "local-port is mandatory in hypervisor-socket configuration.");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(remote_port != socket_obj.cend(),
                                                "remote-port is mandatory in hypervisor-socket configuration.");

    HyperVisorSocketConfiguration config{};
    config.remote_ip_ = score::os::Ipv4Address(remote_ip->second.As<std::string>().value());
    config.local_port_ = local_port->second.As<std::uint16_t>().value();
    config.remote_port_ = remote_port->second.As<std::uint16_t>().value();

    const auto request_timeout = socket_obj.find(kRequestTimeoutMsKey.data());
    if (request_timeout != socket_obj.cend())
    {
        config.request_timeout_ms_ = request_timeout->second.As<std::uint32_t>().value();
    }

    return config;
}

}  // namespace score::mw::com::gateway
