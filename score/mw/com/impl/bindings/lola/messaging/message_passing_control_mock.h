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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_PASSING_CONTROL_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_PASSING_CONTROL_MOCK_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_control.h"

#include "gmock/gmock.h"
#include <string>

namespace score::mw::com::impl::lola
{

class MessagePassingControlMock : public IMessagePassingControl
{
  public:
    MOCK_METHOD(std::shared_ptr<message_passing::ISender>,
                GetMessagePassingSender,
                (QualityType, pid_t),
                (noexcept, override));
    MOCK_METHOD(pid_t, GetNodeIdentifier, (), (const, noexcept, override));
    MOCK_METHOD(std::string, CreateMessagePassingName, (QualityType, pid_t), (noexcept, override));
    MOCK_METHOD(void, RemoveMessagePassingSender, (QualityType, pid_t), (override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_PASSING_CONTROL_MOCK_H
