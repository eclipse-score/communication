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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_IMPL_H
#define SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_IMPL_H

#include "score/concurrency/executor.h"
#include "score/mw/com/message_passing/i_receiver.h"
#include "score/mw/com/message_passing/receiver_config.h"

#include <score/span.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace score::mw::com::message_passing
{

/// \brief A platform-specific implementation of IReceiver factory.
class ReceiverFactoryImpl final
{
  public:
    static score::cpp::pmr::unique_ptr<IReceiver> Create(const std::string_view identifier,
                                                         concurrency::Executor& executor,
                                                         const score::cpp::span<const uid_t> allowed_uids,
                                                         const ReceiverConfig& receiver_config,
                                                         score::cpp::pmr::memory_resource* const memory_resource);
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_IMPL_H
