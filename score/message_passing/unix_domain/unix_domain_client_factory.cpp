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
#include "score/message_passing/unix_domain/unix_domain_client_factory.h"

#include "score/message_passing/client_connection.h"
#include "score/message_passing/unix_domain/unix_domain_engine.h"

namespace score
{
namespace message_passing
{

UnixDomainClientFactory::UnixDomainClientFactory(score::cpp::pmr::memory_resource* const resource) noexcept
    : UnixDomainClientFactory{score::cpp::pmr::make_shared<UnixDomainEngine>(resource, resource)}
{
}

UnixDomainClientFactory::UnixDomainClientFactory(const std::shared_ptr<UnixDomainEngine> engine) noexcept
    : engine_{engine}
{
}

UnixDomainClientFactory::~UnixDomainClientFactory() noexcept {}

score::cpp::pmr::unique_ptr<IClientConnection> UnixDomainClientFactory::Create(
    const ServiceProtocolConfig& protocol_config,
    const ClientConfig& client_config) noexcept
{
    return score::cpp::pmr::make_unique<detail::ClientConnection>(
        engine_->GetMemoryResource(), engine_, protocol_config, client_config);
}

}  // namespace message_passing
}  // namespace score
