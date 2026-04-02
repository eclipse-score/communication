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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_factory.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"

std::unique_ptr<score::mw::com::impl::lola::IMessagePassingServiceInstance>
score::mw::com::impl::lola::MessagePassingServiceInstanceFactory::Create(
    ClientQualityType client_quality_type,
    AsilSpecificCfg config,
    score::message_passing::IServerFactory& server_factory,
    score::message_passing::IClientFactory& client_factory,
    score::concurrency::Executor& executor) const noexcept
{
    return std::make_unique<MessagePassingServiceInstance>(
        client_quality_type, std::move(config), server_factory, client_factory, executor);
}
