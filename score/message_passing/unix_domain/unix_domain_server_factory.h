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
#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H

#include "score/message_passing/i_server_factory.h"

namespace score
{
namespace message_passing
{

class UnixDomainEngine;

class UnixDomainServerFactory final : public IServerFactory
{
  public:
    explicit UnixDomainServerFactory(
        score::cpp::pmr::memory_resource* const resource = score::cpp::pmr::get_default_resource()) noexcept;
    explicit UnixDomainServerFactory(const std::shared_ptr<UnixDomainEngine> engine) noexcept;
    ~UnixDomainServerFactory() noexcept;

    score::cpp::pmr::unique_ptr<IServer> Create(const ServiceProtocolConfig& protocol_config,
                                         const ServerConfig& server_config) noexcept override;

    std::shared_ptr<UnixDomainEngine> GetEngine() const noexcept
    {
        return engine_;
    }

  private:
    const std::shared_ptr<UnixDomainEngine> engine_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H
