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

#include <gtest/gtest.h>

#include <utility>

namespace score::mw::com::gateway::qemu
{
namespace
{

using score::json::operator""_json;

TEST(QemuTransportConfigParserTest, UsesDefaultIvshmemBarNumWhenMissing)
{
    // Given a config without an "ivshmem" section
    auto json = R"JSON(
    {
      "hypervisor-socket": {
        "remote-ip": "10.0.2.2",
        "local-port": 45001,
        "remote-port": 45002
      }
    }
    )JSON"_json;

    // When parsing the config
    const auto config = ParseQemuTransportConfig(std::move(json));

    // Then the parser falls back to the default preferred BAR number
    EXPECT_EQ(config.GetPreferredIvshmemBarNum(), ivshmem::kDefaultIvshmemBarNum);
}

TEST(QemuTransportConfigParserTest, ReadsPreferredIvshmemBarNum)
{
    // Given a config with an explicit "ivshmem.preferred-bar-num" value
    auto json = R"JSON(
    {
      "hypervisor-socket": {
        "remote-ip": "10.0.2.2",
        "local-port": 45001,
        "remote-port": 45002
      },
      "ivshmem": {
        "preferred-bar-num": 3
      }
    }
    )JSON"_json;

    // When parsing the config
    const auto config = ParseQemuTransportConfig(std::move(json));

    // Then the configured preferred BAR number is used
    EXPECT_EQ(config.GetPreferredIvshmemBarNum(), 3U);
}

TEST(QemuTransportConfigParserTest, UsesDefaultWhenPreferredBarNumIsNotInteger)
{
    // Given a config where preferred-bar-num is a non-integer value
    auto json = R"JSON(
    {
      "hypervisor-socket": {
        "remote-ip": "10.0.2.2",
        "local-port": 45001,
        "remote-port": 45002
      },
      "ivshmem": {
        "preferred-bar-num": "not-a-number"
      }
    }
    )JSON"_json;

    // When parsing the config
    const auto config = ParseQemuTransportConfig(std::move(json));

    // Then the parser silently falls back to the default bar num instead of crashing or throwing
    EXPECT_EQ(config.GetPreferredIvshmemBarNum(), ivshmem::kDefaultIvshmemBarNum);
}

}  // namespace
}  // namespace score::mw::com::gateway::qemu
