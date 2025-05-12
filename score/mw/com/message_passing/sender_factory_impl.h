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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_IMPL_H
#define SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_IMPL_H

#include "score/mw/com/message_passing/i_sender.h"
#include "score/mw/com/message_passing/sender_config.h"

#include <score/memory.hpp>
#include <score/stop_token.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace score::mw::com::message_passing
{

/// \brief A platform-specific implementation of ISender factory.
class SenderFactoryImpl final
{
  public:
    static score::cpp::pmr::unique_ptr<ISender> Create(
        const std::string_view identifier,
        const score::cpp::stop_token& token,
        const SenderConfig& sender_config = {},
        LoggingCallback logging_callback = &DefaultLoggingCallback,
        score::cpp::pmr::memory_resource* const memory_resource = score::cpp::pmr::get_default_resource());
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_IMPL_H
