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

// coverity[autosar_cpp14_a2_10_6_violation] false-positive: there is nothing with the same name
// coverity[autosar_cpp14_a3_3_1_violation] False positive: Constructor implementation for class declared in header
QnxDispatchServerFactory::QnxDispatchServerFactory(score::cpp::pmr::memory_resource* const resource) noexcept
    : QnxDispatchServerFactory{score::cpp::pmr::make_shared<QnxDispatchEngine>(resource, resource)}
{
}

QnxDispatchServerFactory::QnxDispatchServerFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept
    : IServerFactory{}, engine_{engine}
{
}

QnxDispatchServerFactory::~QnxDispatchServerFactory() noexcept = default;

// coverity[autosar_cpp14_a2_10_4_violation] false-positive: name is not reused; system-specific implementation
score::cpp::pmr::unique_ptr<IServer> QnxDispatchServerFactory::Create(const ServiceProtocolConfig& protocol_config,
                                                               const ServerConfig& server_config) noexcept
{
    return score::cpp::pmr::make_unique<detail::QnxDispatchServer>(
        engine_->GetMemoryResource(), engine_, protocol_config, server_config);
}

}  // namespace message_passing
}  // namespace score
