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
#include "score/mw/com/gateway/gateway_application/configuration/gateway_config_parser.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <string_view>
#include <vector>

namespace score::mw::com::gateway
{
namespace
{

using std::string_view_literals::operator""sv;

constexpr auto kForwardedServicesKey = "forwarded-services"sv;
constexpr auto kReceivedServicesKey = "expected-received-services"sv;
constexpr auto kTransportLayerKey = "transport-layer"sv;
constexpr auto kTransportLayerIdKey = "id"sv;
constexpr auto kTransportConfigPathKey = "config-path"sv;

auto ParseForwardedServices(const score::json::Any& json) noexcept -> std::vector<std::string>
{
    std::vector<std::string> forwarded_services{};
    if (json.As<score::json::Object>().has_value())
    {
        const auto& forwarded_services_entry =
            json.As<score::json::Object>().value().get().find(kForwardedServicesKey.data());

        if (forwarded_services_entry != json.As<score::json::Object>().value().get().cend())
        {
            for (const auto& forwarded_service : forwarded_services_entry->second.As<score::json::List>().value().get())
            {
                forwarded_services.push_back(std::move(forwarded_service.As<std::string>().value().get()));
            }
        }
    }
    return forwarded_services;
}

auto ParseReceivedServices(const score::json::Any& json) noexcept -> std::vector<std::string>
{
    std::vector<std::string> received_services{};

    if (json.As<score::json::Object>().has_value())
    {
        const auto& received_services_entry =
            json.As<score::json::Object>().value().get().find(kReceivedServicesKey.data());

        if (received_services_entry != json.As<score::json::Object>().value().get().cend())
        {
            for (const auto& received_service : received_services_entry->second.As<score::json::List>().value().get())
            {
                received_services.push_back(std::move(received_service.As<std::string>().value().get()));
            }
        }
    }
    return received_services;
}

auto ParseTransportLayerRef(const score::json::Any& json) noexcept -> std::pair<std::string, std::string>
{
    std::string transport_layer_id{};
    std::string transport_config_path{};

    if (json.As<score::json::Object>().has_value())
    {
        const auto& transport_layer_entry =
            json.As<score::json::Object>().value().get().find(kTransportLayerKey.data());

        if (transport_layer_entry != json.As<score::json::Object>().value().get().cend())
        {
            const auto& tl_obj = transport_layer_entry->second.As<score::json::Object>();
            if (tl_obj.has_value())
            {
                const auto& id_entry = tl_obj.value().get().find(kTransportLayerIdKey.data());
                if (id_entry != tl_obj.value().get().cend())
                {
                    transport_layer_id = id_entry->second.As<std::string>().value().get();
                }

                const auto& path_entry = tl_obj.value().get().find(kTransportConfigPathKey.data());
                if (path_entry != tl_obj.value().get().cend())
                {
                    transport_config_path = path_entry->second.As<std::string>().value().get();
                }
            }
        }
    }
    return {std::move(transport_layer_id), std::move(transport_config_path)};
}

}  // namespace

auto ParseGatewayConfig(const std::string_view path) noexcept -> GatewayConfiguration
{
    const score::json::JsonParser json_parser_obj;
    // NOLINTNEXTLINE(score-banned-function): AoU of score::json::JsonParser — caller must guarantee path integrity.
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing gateway config file" << path << "failed with error:" << json_result.error().Message() << ": "
            << json_result.error().UserMessage() << " . Terminating.";
        std::terminate();
    }
    return ParseGatewayConfig(std::move(json_result).value());
}

auto ParseGatewayConfig(score::json::Any json) noexcept -> GatewayConfiguration
{
    auto top_level_object = json.As<score::json::Object>();
    if (!top_level_object.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing gateway configuration failed: Expected top-level JSON object. Terminating.";
        std::terminate();
    }

    auto [transport_layer_id, transport_config_path] = ParseTransportLayerRef(json);
    return GatewayConfiguration{
        ParseForwardedServices(json), ParseReceivedServices(json), transport_layer_id, transport_config_path};
}

}  // namespace score::mw::com::gateway
