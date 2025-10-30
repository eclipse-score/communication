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
#include "score/mw/com/message_passing/sender_factory_impl.h"

#include "score/mw/com/message_passing/qnx/resmgr_sender_traits.h"
#include "score/mw/com/message_passing/sender.h"

score::cpp::pmr::unique_ptr<score::mw::com::message_passing::ISender>
score::mw::com::message_passing::SenderFactoryImpl::Create(const std::string_view identifier,
                                                           const score::cpp::stop_token& token,
                                                           const SenderConfig& sender_config,
                                                           LoggingCallback logging_callback,
                                                           score::cpp::pmr::memory_resource* const memory_resource)
{
    return score::cpp::pmr::make_unique<Sender<ResmgrSenderTraits>>(
        memory_resource, identifier, token, sender_config, std::move(logging_callback));
}
