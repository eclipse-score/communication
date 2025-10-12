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
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"

#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server.h"

namespace score
{
namespace message_passing
{

QnxDispatchServerFactory::QnxDispatchServerFactory(score::cpp::pmr::memory_resource* const resource) noexcept
    : QnxDispatchServerFactory{score::cpp::pmr::make_shared<QnxDispatchEngine>(resource, resource)}
{
}

QnxDispatchServerFactory::QnxDispatchServerFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept
    : IServerFactory{}, engine_{engine}
{
}

// NOLINTNEXTLINE(modernize-use-equals-default) false positive: the destructor is not trivial
QnxDispatchServerFactory::~QnxDispatchServerFactory() noexcept {}

score::cpp::pmr::unique_ptr<IServer> QnxDispatchServerFactory::Create(const ServiceProtocolConfig& protocol_config,
                                                               const ServerConfig& server_config) noexcept
{
    return score::cpp::pmr::make_unique<detail::QnxDispatchServer>(
        engine_->GetMemoryResource(), engine_, protocol_config, server_config);
}

}  // namespace message_passing
}  // namespace score
