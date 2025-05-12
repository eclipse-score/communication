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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_MOCK_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola
{

class MessagePassingServiceMock : public IMessagePassingService
{
  public:
    MOCK_METHOD(void, NotifyEvent, (QualityType, ElementFqId), (noexcept, override));
    MOCK_METHOD(HandlerRegistrationNoType,
                RegisterEventNotification,
                (QualityType, ElementFqId, std::weak_ptr<ScopedEventReceiveHandler>, pid_t),
                (noexcept, override));
    MOCK_METHOD(void, ReregisterEventNotification, (QualityType, ElementFqId, pid_t), (noexcept, override));
    MOCK_METHOD(void,
                UnregisterEventNotification,
                (QualityType, ElementFqId, HandlerRegistrationNoType, pid_t),
                (noexcept, override));
    MOCK_METHOD(void, NotifyOutdatedNodeId, (QualityType, pid_t, pid_t), (noexcept, override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_MOCK_H
