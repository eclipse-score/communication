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
#include "score/someip/routing_module/routing_manager/routing_manager.h"

#include "score/mw/log/logging.h"

#include <cerrno>
#include <utility>
#include <variant>

namespace score::someip::routing_module
{
namespace
{

using routing_control_protocol::Message;
using routing_control_protocol::Opcode;

constexpr auto kLogContext{"SRTM"};
constexpr std::uintptr_t kAcceptedConnection{1U};

const score::message_passing::ServiceProtocolConfig kProtocolConfig{routing_control_protocol::kServiceIdentifier,
                                                                    routing_control_protocol::kMaxMessageSize,
                                                                    0U,
                                                                    0U};
// Two preallocated connections, so that a second client can be rejected without allocating.
const score::message_passing::IServerFactory::ServerConfig kServerConfig{16U, 2U, 0U};

ServiceInstance ServiceInstanceOf(const Message& command) noexcept
{
    return ServiceInstance{command.service, command.instance};
}

Endpoint RemoteEndpointOf(const Message& command) noexcept
{
    return Endpoint{command.remote_ip, command.remote_port};
}

}  // namespace

RoutingManager::RoutingManager(score::message_passing::IServerFactory& server_factory,
                               IRoutingCommandHandler& handler,
                               WakeupCallback wakeup) noexcept
    : server_factory_{server_factory}, handler_{handler}, wakeup_{std::move(wakeup)}
{
}

RoutingManager::~RoutingManager() noexcept
{
    Stop();
}

score::cpp::expected_blank<score::os::Error> RoutingManager::Start() noexcept
{
    server_ = server_factory_.Create(kProtocolConfig, kServerConfig);
    if (server_ == nullptr)
    {
        score::mw::log::LogError(kLogContext) << "Creating the routing command server failed";
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }
    auto result = server_->StartListening(
        [this](score::message_passing::IServerConnection& connection) {
            return OnConnect(connection);
        },
        [this](score::message_passing::IServerConnection& connection) {
            OnDisconnect(connection);
        },
        [this](score::message_passing::IServerConnection&, const score::cpp::span<const std::uint8_t> data) {
            return OnCommand(data);
        },
        [](score::message_passing::IServerConnection&,
           score::cpp::span<const std::uint8_t>) -> score::cpp::expected_blank<score::os::Error> {
            score::mw::log::LogWarn(kLogContext) << "Routing command with reply request rejected";
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOTSUP));
        });
    if (!result.has_value())
    {
        score::mw::log::LogError(kLogContext) << "Listening for routing commands failed:" << result.error().ToString();
        server_.reset();
    }
    return result;
}

void RoutingManager::Stop() noexcept
{
    if (server_ != nullptr)
    {
        server_->StopListening();
        server_.reset();
    }
}

void RoutingManager::ProcessPendingCommands() noexcept
{
    // TODO: Check in production whether processing needs a per-call command limit.
    for (auto entry = PopEntry(); entry.has_value(); entry = PopEntry())
    {
        if (entry.value().kind == QueueEntry::Kind::kRemoveAll)
        {
            handler_.RemoveAll();
        }
        else
        {
            Dispatch(entry.value().command);
        }
    }
}

score::cpp::expected<score::message_passing::UserData, score::os::Error> RoutingManager::OnConnect(
    score::message_passing::IServerConnection& connection) noexcept
{
    const auto& client = connection.GetClientIdentity();
    if (connected_)
    {
        score::mw::log::LogWarn(kLogContext)
            << "SD daemon already connected, rejected routing command client with pid" << client.pid;
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EBUSY));
    }
    connected_ = true;
    shutdown_announced_ = false;
    score::mw::log::LogWarn(kLogContext) << "SD daemon connected, pid" << client.pid << "uid" << client.uid;
    return score::message_passing::UserData{std::in_place_type<std::uintptr_t>, kAcceptedConnection};
}

void RoutingManager::OnDisconnect(score::message_passing::IServerConnection& connection) noexcept
{
    const auto* const marker = std::get_if<std::uintptr_t>(&connection.GetUserData());
    if ((marker == nullptr) || (*marker != kAcceptedConnection))
    {
        return;
    }
    if (shutdown_announced_)
    {
        score::mw::log::LogWarn(kLogContext) << "SD daemon disconnected after announcing its shutdown";
    }
    else
    {
        score::mw::log::LogWarn(kLogContext) << "SD daemon disconnected unexpectedly, removing all routing state";
    }
    connected_ = false;
    shutdown_announced_ = false;
    {
        const std::lock_guard<std::mutex> lock{queue_mutex_};
        queue_.clear();
        queue_.push_back(QueueEntry{QueueEntry::Kind::kRemoveAll, Message{}});
    }
    wakeup_();
}

score::cpp::expected_blank<score::os::Error> RoutingManager::OnCommand(
    const score::cpp::span<const std::uint8_t> data) noexcept
{
    const auto command = routing_control_protocol::Decode(data);
    if (!command.has_value())
    {
        score::mw::log::LogWarn(kLogContext)
            << "Invalid routing command discarded, error" << static_cast<std::uint32_t>(command.error());
        return {};
    }
    if (command.value().opcode == Opcode::kShutdown)
    {
        shutdown_announced_ = true;
    }
    Enqueue(command.value());
    return {};
}

void RoutingManager::Enqueue(const Message& command) noexcept
{
    {
        const std::lock_guard<std::mutex> lock{queue_mutex_};
        if (queue_.full())
        {
            score::mw::log::LogError(kLogContext)
                << "Routing command queue full, command discarded; opcode" << static_cast<std::uint32_t>(command.opcode)
                << "service" << command.service << "instance" << command.instance;
            return;
        }
        queue_.push_back(QueueEntry{QueueEntry::Kind::kCommand, command});
    }
    wakeup_();
}

std::optional<RoutingManager::QueueEntry> RoutingManager::PopEntry() noexcept
{
    const std::lock_guard<std::mutex> lock{queue_mutex_};
    if (queue_.empty())
    {
        return std::nullopt;
    }
    std::optional<QueueEntry> entry{queue_.front()};
    queue_.pop_front();
    return entry;
}

void RoutingManager::Dispatch(const Message& command) noexcept
{
    const ServiceInstance service_instance = ServiceInstanceOf(command);
    CommandResult result{CommandResult::kApplied};
    switch (command.opcode)
    {
        case Opcode::kAddRemoteService:
            result = handler_.AddRemoteService(command);
            break;
        case Opcode::kRemoveRemoteService:
            result = handler_.RemoveRemoteService(service_instance);
            break;
        case Opcode::kAddLocalSubscription:
            result = handler_.AddLocalSubscription(service_instance, command.event);
            break;
        case Opcode::kRemoveLocalSubscription:
            result = handler_.RemoveLocalSubscription(service_instance, command.event);
            break;
        case Opcode::kAddLocalService:
            result = handler_.AddLocalService(command);
            break;
        case Opcode::kRemoveLocalService:
            result = handler_.RemoveLocalService(service_instance);
            break;
        case Opcode::kAddRemoteSubscription:
            result = handler_.AddRemoteSubscription(service_instance, command.event, RemoteEndpointOf(command));
            break;
        case Opcode::kRemoveRemoteSubscription:
            result = handler_.RemoveRemoteSubscription(service_instance, command.event, RemoteEndpointOf(command));
            break;
        case Opcode::kShutdown:
            score::mw::log::LogWarn(kLogContext) << "SD daemon announced its shutdown, removing all routing state";
            handler_.RemoveAll();
            break;
        default:
            // Unreachable: Decode only returns defined opcodes.
            result = CommandResult::kRejected;
            break;
    }
    if (result == CommandResult::kRejected)
    {
        score::mw::log::LogWarn(kLogContext)
            << "Routing command rejected, opcode" << static_cast<std::uint32_t>(command.opcode) << "service"
            << command.service << "instance" << command.instance << "event" << command.event;
    }
}

}  // namespace score::someip::routing_module
