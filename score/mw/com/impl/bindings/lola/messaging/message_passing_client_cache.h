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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGCLIENTCACHE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGCLIENTCACHE_H

#include "score/message_passing/i_client_factory.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::lola
{

class MessagePassingClientCache
{
  public:
    enum class ClientQualityType : std::uint8_t
    {
        kASIL_QM,
        kASIL_QMfromB,
        kASIL_B,
    };

    MessagePassingClientCache(const ClientQualityType asil_level,
                              score::message_passing::IClientFactory& client_factory) noexcept;

    ~MessagePassingClientCache() noexcept = default;

    MessagePassingClientCache(MessagePassingClientCache&&) = delete;
    MessagePassingClientCache& operator=(MessagePassingClientCache&&) = delete;
    MessagePassingClientCache(const MessagePassingClientCache&) = delete;
    MessagePassingClientCache& operator=(const MessagePassingClientCache&) = delete;

    std::shared_ptr<score::message_passing::IClientConnection> GetMessagePassingClient(
        const pid_t target_node_id) noexcept;
    void RemoveMessagePassingClient(const pid_t target_node_id) noexcept;

    static std::string CreateMessagePassingName(const ClientQualityType asil_level, const pid_t node_id) noexcept;

  private:
    std::shared_ptr<score::message_passing::IClientConnection> CreateNewClient(const pid_t target_node_id) noexcept;

    const ClientQualityType asil_level_;
    score::message_passing::IClientFactory& client_factory_;
    // TODO: PMR
    std::unordered_map<pid_t, std::shared_ptr<score::message_passing::IClientConnection>> clients_;
    std::mutex mutex_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGCLIENTCACHE_H
