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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_H
#define SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_H

#include "score/concurrency/executor.h"
#include "score/mw/com/message_passing/i_receiver.h"
#include "score/mw/com/message_passing/receiver_config.h"

#include <score/memory.hpp>
#include <score/span.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace score::mw::com::message_passing
{

/// \brief Factory, which creates instances of IReceiver.
/// \details Factory pattern serves two purposes here: Testability/mockability of receivers and alternative
///          implementations of IReceiver. We initially have a POSIX MQ based implementation of IReceiver, but specific
///          implementations e.g. for QNX based on specific IPC mechanisms are expected.
class ReceiverFactory final
{
  public:
    /// \brief Creates an implementation instance of IReceiver.
    /// \details This is the factory create method for IReceiver instances. There a specific implementation of the
    ///          platform or a mock instance (see InjectReceiverMock()) is returned.
    /// \todo Eventually extend the signature with some configuration parameter, from which factory can deduce, which
    ///       IReceiver impl. to create. Currently we only have POSIX MQ based IReceiver impl..
    /// \param identifier some identifier for the receiver being created. Depending on the chosen impl. this might be
    ///        used or not.
    /// \param executor An executor where the asynchronous blocking listening task can be scheduled
    /// \param allowed_user_ids user ids of processes/senders allowed to access/send to this receiver (if empty,
    ///        everyone has access). Can be ignored in implementations that don't support ACLs.
    /// \param receiver_config additional receiver configuration parameters
    /// \param memory_resource memory resource for allocating the memory required by the Receiver
    /// object \return a platform specific implementation of a IReceiver or a mock.
    static score::cpp::pmr::unique_ptr<IReceiver> Create(
        const std::string_view identifier,
        concurrency::Executor& executor,
        const score::cpp::span<const uid_t> allowed_user_ids,
        const ReceiverConfig& receiver_config = {},
        score::cpp::pmr::memory_resource* memory_resource = score::cpp::pmr::get_default_resource());

    /// \brief Inject pointer to a mock instance, which shall be returned by all Create() calls.
    /// \param mock
    static void InjectReceiverMock(IReceiver* const mock);

  private:
    static IReceiver* receiver_mock_;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_FACTORY_H
