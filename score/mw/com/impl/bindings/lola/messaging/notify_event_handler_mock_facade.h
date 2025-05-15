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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_NOTIFY_EVENT_HANDLER_MOCK_FACADE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_NOTIFY_EVENT_HANDLER_MOCK_FACADE_H

#include "score/mw/com/impl/bindings/lola/messaging/i_notify_event_handler.h"
#include "score/mw/com/impl/bindings/lola/messaging/notify_event_handler_mock.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola::test
{

/// \brief Facade class which dispatches to a mock object which is owned by the caller
///
/// Such a facade class is useful when a test needs to mock an object for which ownership must be passed to the
/// class under test e.g. via a unique_ptr. In this case, the test requires that the mock survives until the end of the
/// test, however, this cannot be guaranteed when handing ownership to the class under test. Therefore, the test can
/// create the mock object and provide a facade object to the class under test (which will dispatch any calls to the
/// mock object).
class NotifyEventHandlerMockFacade : public INotifyEventHandler
{
  public:
    NotifyEventHandlerMockFacade(NotifyEventHandlerMock& notify_event_handler_mock) : mock_{notify_event_handler_mock}
    {
    }

    void RegisterMessageReceivedCallbacks(const QualityType asil_level,
                                          message_passing::IReceiver& receiver) noexcept override
    {
        mock_.RegisterMessageReceivedCallbacks(asil_level, receiver);
    }

    void NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept override
    {
        mock_.NotifyEvent(asil_level, event_id);
    }

    IMessagePassingService::HandlerRegistrationNoType RegisterEventNotification(
        const QualityType asil_level,
        const ElementFqId event_id,
        std::weak_ptr<ScopedEventReceiveHandler> callback,
        const pid_t target_node_id) noexcept override
    {
        return mock_.RegisterEventNotification(asil_level, event_id, callback, target_node_id);
    }

    void ReregisterEventNotification(QualityType asil_level,
                                     ElementFqId event_id,
                                     pid_t target_node_id) noexcept override
    {
        mock_.ReregisterEventNotification(asil_level, event_id, target_node_id);
    }

    void UnregisterEventNotification(const QualityType asil_level,
                                     const ElementFqId event_id,
                                     const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                     const pid_t target_node_id) override
    {
        mock_.UnregisterEventNotification(asil_level, event_id, registration_no, target_node_id);
    }

    void NotifyOutdatedNodeId(QualityType asil_level, pid_t outdated_node_id, pid_t target_node_id) noexcept override
    {
        mock_.NotifyOutdatedNodeId(asil_level, outdated_node_id, target_node_id);
    }

  private:
    NotifyEventHandlerMock& mock_;
};

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_NOTIFY_EVENT_HANDLER_MOCK_FACADE_H
