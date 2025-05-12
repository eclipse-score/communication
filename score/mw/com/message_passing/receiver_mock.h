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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_MOCK_H
#define SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_MOCK_H

#include "score/mw/com/message_passing/i_receiver.h"

#include "gmock/gmock.h"

namespace score::mw::com::message_passing
{

class ReceiverMock : public IReceiver
{
  public:
    MOCK_METHOD(void, Register, (const MessageId id, ShortMessageReceivedCallback callback), (noexcept, override));
    MOCK_METHOD(void, Register, (const MessageId id, MediumMessageReceivedCallback callback), (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, StartListening, (), (noexcept, override));
};

}  // namespace score::mw::com::message_passing
#endif  // SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_MOCK_H
