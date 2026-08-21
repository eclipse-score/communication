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
#include "score/mw/com/gateway/transport_layer/qemu/configuration/qemu_transport_config_parser.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <string_view>

namespace score::mw::com::gateway::qemu
{
namespace
{

using std::string_view_literals::operator""sv;

constexpr auto kIvshmemConfigurationKey = "ivshmem"sv;
constexpr auto kPreferredBarNumKey = "preferred-bar-num"sv;

}  // namespace

auto ParseQemuTransportConfig(const std::string_view path) noexcept -> QemuTransportConfiguration
{
    const score::json::JsonParser json_parser_obj;
    // NOLINTNEXTLINE(score-banned-function): AoU of score::json::JsonParser — caller must guarantee path integrity.
    auto json_result = json_parser_obj.FromFile(path);
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing qemu transport config file" << path << "failed with error:" << json_result.error().Message()
            << ": " << json_result.error().UserMessage() << " . Terminating.";
        std::terminate();
    }
    return ParseQemuTransportConfig(std::move(json_result).value());
}

auto ParseQemuTransportConfig(score::json::Any json) noexcept -> QemuTransportConfiguration
{
    auto top_level_object = json.As<score::json::Object>();
    if (!top_level_object.has_value())
    {
        ::score::mw::log::LogFatal("lola")
            << "Parsing qemu transport configuration failed: Expected top-level JSON object. Terminating.";
        std::terminate();
    }

    const auto& obj = top_level_object.value().get();

    std::uint32_t preferred_bar_num = ivshmem::kDefaultIvshmemBarNum;
    const auto ivshmem_entry = obj.find(kIvshmemConfigurationKey);
    if (ivshmem_entry != obj.cend())
    {
        const auto ivshmem_obj_result = ivshmem_entry->second.As<score::json::Object>();
        if (ivshmem_obj_result.has_value())
        {
            const auto& ivshmem_obj = ivshmem_obj_result.value().get();
            const auto preferred_bar_num_entry = ivshmem_obj.find(kPreferredBarNumKey);
            if (preferred_bar_num_entry != ivshmem_obj.cend())
            {
                const auto preferred_bar_num_result = preferred_bar_num_entry->second.As<std::uint32_t>();
                if (preferred_bar_num_result.has_value())
                {
                    preferred_bar_num = preferred_bar_num_result.value();
                }
            }
        }
    }

    return QemuTransportConfiguration{preferred_bar_num};
}

}  // namespace score::mw::com::gateway::qemu
