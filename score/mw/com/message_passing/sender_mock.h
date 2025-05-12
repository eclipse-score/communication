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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SENDER_MOCK_H
#define SCORE_MW_COM_MESSAGE_PASSING_SENDER_MOCK_H

#include "score/mw/com/message_passing/i_sender.h"

#include "gmock/gmock.h"

namespace score::mw::com::message_passing
{

class SenderMock : public ISender
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Send, (const ShortMessage&), (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Send, (const MediumMessage&), (noexcept, override));
    MOCK_METHOD(bool, HasNonBlockingGuarantee, (), (const, noexcept, override));
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SENDER_MOCK_H
