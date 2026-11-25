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
#include "score/someip/routing_module/routing_bridge/routing_bridge.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

#include "score/filesystem/path.h"
#include "score/message_passing/server_factory.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <optional>
#include <string>

namespace
{

using score::someip::routing_module::RoutingBridge;

constexpr auto kLogContext{"SRTM"};

std::optional<RoutingBridge>& Bridge() noexcept
{
    static std::optional<RoutingBridge> bridge{};
    return bridge;
}

/// Path of the hosting binary or loaded module.
std::string ModulePath()
{
    Dl_info info{};
    if ((::dladdr(&Bridge(), &info) == 0) || (info.dli_fname == nullptr))
    {
        return {};
    }
    return info.dli_fname;
}

}  // namespace

extern "C" void someip_routing_prepare_logging(void)
{
    const std::string config_directory = score::someip::routing_module::ModuleConfigDirectory(ModulePath());
    // mw::log reads its configuration on the first log, by default relative to the hosting executable.
    // TODO: check the way on how the config paths are detected once this is deployed on an actual target.
    static_cast<void>(::setenv("MW_LOG_CONFIG_FILE", (config_directory + "/logging.json").c_str(), 0));
    score::mw::log::LogInfo(kLogContext) << "Initializing routing module logging";
}

extern "C" int someip_routing_start(void)
{
    const std::string config_directory = score::someip::routing_module::ModuleConfigDirectory(ModulePath());
    const std::string lola_config = config_directory + "/mw_com_config.json";
    // mw::com terminates the host process if its configuration cannot be read.
    if (::access(lola_config.c_str(), R_OK) != 0)
    {
        score::mw::log::LogError(kLogContext) << "LoLa configuration" << lola_config << "not readable";
        return ENOENT;
    }
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{lola_config}});

    static score::message_passing::ServerFactory server_factory{};
    auto& bridge = Bridge();
    bridge.emplace(server_factory);
    const auto started = bridge->Start();
    if (!started.has_value())
    {
        bridge.reset();
        const int error = started.error().GetOsDependentErrorCode();
        return (error != 0) ? error : EIO;
    }
    score::mw::log::LogInfo(kLogContext) << "Routing module started with LoLa configuration" << lola_config;
    return 0;
}

extern "C" void someip_routing_stop(void)
{
    Bridge().reset();
}

extern "C" void someip_routing_poll(void)
{
    Bridge()->Poll();
}

extern "C" void someip_routing_datagram(const unsigned short local_port,
                                        const unsigned char sender_ip[4],
                                        const unsigned short sender_port,
                                        const void* const packet,
                                        const unsigned int size)
{
    score::someip::routing_module::Endpoint sender{{}, sender_port};
    std::copy_n(sender_ip, sender.address.size(), sender.address.begin());
    Bridge()->OnUdpDatagram(local_port, sender, packet, size);
}
