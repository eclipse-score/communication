/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_client_cache.h"

#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/i_client_factory.h"
#include "score/message_passing/service_protocol_config.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace score::mw::com::impl::lola
{

namespace
{
// TODO: avoid duplication with message_passing_service_instance.cpp
constexpr auto mq_name_prefix_mpcc("LoLa_2_");
constexpr auto mq_name_qm_postfix_mpcc("_QM");
constexpr auto mq_name_asil_b_postfix_mpcc("_ASIL_B");

constexpr std::uint32_t kMaxSendSize{9U};

constexpr std::uint32_t kStateTryAttempts{10U};
constexpr std::chrono::milliseconds kStateRetryDelay{50};

}  // namespace

MessagePassingClientCache::MessagePassingClientCache(const ClientQualityType asil_level,
                                                     score::message_passing::IClientFactory& client_factory) noexcept
    : asil_level_{asil_level}, client_factory_{client_factory}, clients_{}, mutex_{}
{
}

std::shared_ptr<score::message_passing::IClientConnection> MessagePassingClientCache::GetMessagePassingClient(
    const pid_t target_node_id) noexcept
{
    std::lock_guard<std::mutex> lck(mutex_);

    auto search = clients_.find(target_node_id);
    if (search != clients_.end())
    {
        return search->second;
    }

    // create the OS specific sender
    auto new_sender = CreateNewClient(target_node_id);

    auto elem = clients_.emplace(target_node_id, new_sender);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(elem.second,
                           "MessagePassingClientCache::GetMessagePassingClient(): Failed to emplace Client!");

    return elem.first->second;
}

std::shared_ptr<score::message_passing::IClientConnection> MessagePassingClientCache::CreateNewClient(
    const pid_t target_node_id) noexcept
{
    const std::string service_identifier = CreateMessagePassingName(asil_level_, target_node_id);

    const bool fully_async = asil_level_ == ClientQualityType::kASIL_QMfromB;
    const score::message_passing::ServiceProtocolConfig protocol_config{service_identifier, kMaxSendSize, 0U, 0U};
    const score::message_passing::IClientFactory::ClientConfig client_config{0U, 20U, false, fully_async, true};

    auto new_sender_unique_p = client_factory_.Create(protocol_config, client_config);

    auto deleter = new_sender_unique_p.get_deleter();
    // TODO: PMR
    // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function shall
    // have automatic storage duration".
    // The object will be emplaced into the senders map, so we need to allocate it in the heap.
    // coverity[autosar_cpp14_a18_5_8_violation]
    std::shared_ptr<score::message_passing::IClientConnection> new_sender{new_sender_unique_p.release(), deleter};

    new_sender->Start(score::message_passing::IClientConnection::StateCallback{},
                      score::message_passing::IClientConnection::NotifyCallback{});
    for (std::uint32_t try_attempt{0U}; try_attempt < kStateTryAttempts; ++try_attempt)
    {
        const auto state = new_sender->GetState();
        if (state == score::message_passing::IClientConnection::State::kReady)
        {
            return new_sender;
        }
        if (state != score::message_passing::IClientConnection::State::kStarting)
        {
            score::mw::log::LogError("lola")
                << "MessagePassingClientCache: Connection for " << service_identifier
                << " has failed to create, the reason is "
                << static_cast<std::uint32_t>(score::cpp::to_underlying(new_sender->GetStopReason()));
            return new_sender;
        }
        std::this_thread::sleep_for(kStateRetryDelay);
    }

    score::mw::log::LogError("lola") << "MessagePassingClientCache: Connection for " << service_identifier
                                   << " takes too long to create, might be not working";
    return new_sender;
}

void MessagePassingClientCache::RemoveMessagePassingClient(const pid_t target_node_id) noexcept
{
    std::lock_guard<std::mutex> lck(mutex_);

    auto search = clients_.find(target_node_id);
    if (search != clients_.end())
    {
        search->second->Stop();
        std::uint32_t try_attempt{0U};
        while (search->second->GetState() != score::message_passing::IClientConnection::State::kStopped)
        {
            ++try_attempt;
            if (try_attempt >= kStateTryAttempts)
            {
                score::mw::log::LogFatal("lola") << "MessagePassingClientCache: Cannot close connection to target "
                                               << target_node_id << " in reasonable time";
                std::terminate();
            }
            std::this_thread::sleep_for(kStateRetryDelay);
        }
        score::cpp::ignore = clients_.erase(search);
    }
}

std::string MessagePassingClientCache::CreateMessagePassingName(const ClientQualityType asil_level,
                                                                const pid_t node_id) noexcept
{
    std::stringstream identifier;
    identifier << mq_name_prefix_mpcc << node_id;
    if (asil_level != ClientQualityType::kASIL_B)
    {
        identifier << mq_name_qm_postfix_mpcc;
    }
    else
    {
        identifier << mq_name_asil_b_postfix_mpcc;
    }
    return identifier.str();
}

}  // namespace score::mw::com::impl::lola
