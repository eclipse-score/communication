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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_MANAGER_H
#define SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_MANAGER_H

#include "score/someip/router_control_protocol/router_control_protocol.h"
#include "score/someip/routing_module/routing_manager/i_routing_command_handler.h"

#include "score/message_passing/i_server.h"
#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/i_server_factory.h"
#include "score/os/errno.h"

#include <score/callback.hpp>
#include <score/circular_buffer.hpp>
#include <score/expected.hpp>
#include <score/memory.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace score::someip::routing_module
{

/// Receives routing commands from the SD daemon (the only accepted client) via score::message_passing and applies
/// them in arrival order to an IRoutingCommandHandler on the data path thread.
class RoutingManager final
{
  public:
    // TODO: size for the deployment's SD startup burst, or use a vector reserved at startup.
    static constexpr std::size_t kCommandQueueCapacity{256U};

    /// Called from message_passing threads whenever ProcessPendingCommands() has work; must not block.
    using WakeupCallback = score::cpp::callback<void()>;

    RoutingManager(score::message_passing::IServerFactory& server_factory,
                   IRoutingCommandHandler& handler,
                   WakeupCallback wakeup) noexcept;
    ~RoutingManager() noexcept;
    RoutingManager(const RoutingManager&) = delete;
    RoutingManager(RoutingManager&&) = delete;
    RoutingManager& operator=(const RoutingManager&) = delete;
    RoutingManager& operator=(RoutingManager&&) = delete;

    score::cpp::expected_blank<score::os::Error> Start() noexcept;
    void Stop() noexcept;

    /// Applies all queued routing commands; call on the data path thread before forwarding further samples.
    void ProcessPendingCommands() noexcept;

  private:
    /// Removing all routing state is queued like a command, so that it stays ordered with commands received
    /// before and after it.
    struct QueueEntry
    {
        enum class Kind : std::uint8_t
        {
            kCommand,
            kRemoveAll,
        };
        Kind kind;
        router_control_protocol::Message command;
    };

    score::cpp::expected<score::message_passing::UserData, score::os::Error> OnConnect(
        score::message_passing::IServerConnection& connection) noexcept;
    void OnDisconnect(score::message_passing::IServerConnection& connection) noexcept;
    score::cpp::expected_blank<score::os::Error> OnCommand(score::cpp::span<const std::uint8_t> data) noexcept;
    void Enqueue(const router_control_protocol::Message& command) noexcept;
    std::optional<QueueEntry> PopEntry() noexcept;
    void Dispatch(const router_control_protocol::Message& command) noexcept;

    score::message_passing::IServerFactory& server_factory_;
    IRoutingCommandHandler& handler_;
    WakeupCallback wakeup_;
    score::cpp::pmr::unique_ptr<score::message_passing::IServer> server_;

    // Accessed by message_passing callbacks only, which the library serializes.
    bool connected_{false};
    bool shutdown_announced_{false};

    std::mutex queue_mutex_;
    score::cpp::circular_buffer<QueueEntry, kCommandQueueCapacity> queue_;
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_MANAGER_H
