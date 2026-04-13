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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_SERVER_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_I_SERVER_FACTORY_H

#include "score/message_passing/i_server.h"
#include "score/message_passing/service_protocol_config.h"

#include <score/memory.hpp>

#include <cstdint>

namespace score
{
namespace message_passing
{

class IServerFactory
{
  public:
    struct ServerConfig
    {
        std::uint32_t max_queued_sends;       ///< Maximum number of Send messages by clients queued on server side.
                                              ///< shall be at least 1
        std::uint32_t pre_alloc_connections;  ///< Number of preallocated server connections.
                                              ///< 0 if there is no preallocation (fine for QM apps,
                                              ///< but bad for monotonic memory allocation)
        std::uint32_t max_queued_notifies;    ///< Maximum number of Notify messages per connection queued on server
                                              ///< side. 0 if there is no Notify messages, otherwise at least 1
    };

    virtual score::cpp::pmr::unique_ptr<IServer> Create(const ServiceProtocolConfig& protocol_config,
                                                        const ServerConfig& server_config) noexcept = 0;

  protected:
    ~IServerFactory() = default;

    IServerFactory() noexcept = default;
    IServerFactory(const IServerFactory&) = delete;
    IServerFactory(IServerFactory&&) = delete;
    IServerFactory& operator=(const IServerFactory&) = delete;
    IServerFactory& operator=(IServerFactory&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_SERVER_FACTORY_H
