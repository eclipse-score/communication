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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_NOTIFY_EVENT_HANDLER_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_NOTIFY_EVENT_HANDLER_MOCK_H

#include "score/mw/com/impl/bindings/lola/messaging/i_notify_event_handler.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola::test
{

class NotifyEventHandlerMock : public INotifyEventHandler
{
  public:
    MOCK_METHOD(void,
                RegisterMessageReceivedCallbacks,
                (const QualityType asil_level, message_passing::IReceiver& receiver),
                (noexcept, override));
    MOCK_METHOD(void, NotifyEvent, (const QualityType asil_level, const ElementFqId event_id), (noexcept, override));
    MOCK_METHOD(IMessagePassingService::HandlerRegistrationNoType,
                RegisterEventNotification,
                (const QualityType asil_level,
                 const ElementFqId event_id,
                 std::weak_ptr<ScopedEventReceiveHandler> callback,
                 const pid_t target_node_id),
                (noexcept, override));
    MOCK_METHOD(void,
                ReregisterEventNotification,
                (QualityType asil_level, ElementFqId event_id, pid_t target_node_id),
                (noexcept, override));
    MOCK_METHOD(void,
                UnregisterEventNotification,
                (const QualityType asil_level,
                 const ElementFqId event_id,
                 const IMessagePassingService::HandlerRegistrationNoType registration_no,
                 const pid_t target_node_id),
                (override));
    MOCK_METHOD(void,
                NotifyOutdatedNodeId,
                (QualityType asil_level, pid_t outdated_node_id, pid_t target_node_id),
                (noexcept, override));
};

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_NOTIFY_EVENT_HANDLER_MOCK_H
