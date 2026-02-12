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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_INSTANCE_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_INSTANCE_MOCK_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service_instance.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola
{

class MessagePassingServiceInstanceMock : public IMessagePassingServiceInstance
{
  public:
    MOCK_METHOD(void, NotifyEvent, (const ElementFqId), (noexcept, override));

    MOCK_METHOD(IMessagePassingService::HandlerRegistrationNoType,
                RegisterEventNotification,
                (const ElementFqId, std::weak_ptr<ScopedEventReceiveHandler>, const pid_t),
                (noexcept, override));

    MOCK_METHOD(void, ReregisterEventNotification, (const ElementFqId, const pid_t), (noexcept, override));

    MOCK_METHOD(void,
                UnregisterEventNotification,
                (const ElementFqId, const IMessagePassingService::HandlerRegistrationNoType, const pid_t),
                (noexcept, override));

    MOCK_METHOD(ResultBlank,
                RegisterOnServiceMethodSubscribedHandler,
                (SkeletonInstanceIdentifier,
                 IMessagePassingService::ServiceMethodSubscribedHandler,
                 IMessagePassingService::AllowedConsumerUids),
                (override));

    MOCK_METHOD(ResultBlank,
                RegisterMethodCallHandler,
                (ProxyMethodInstanceIdentifier, IMessagePassingService::MethodCallHandler, uid_t),
                (override));

    MOCK_METHOD(void, NotifyOutdatedNodeId, (const pid_t, const pid_t), (noexcept, override));

    MOCK_METHOD(void,
                RegisterEventNotificationExistenceChangedCallback,
                (const ElementFqId, IMessagePassingService::HandlerStatusChangeCallback),
                (noexcept, override));

    MOCK_METHOD(void, UnregisterEventNotificationExistenceChangedCallback, (const ElementFqId), (noexcept, override));

    MOCK_METHOD(ResultBlank,
                SubscribeServiceMethod,
                (const SkeletonInstanceIdentifier&, const ProxyInstanceIdentifier&, pid_t),
                (override));

    MOCK_METHOD(ResultBlank, CallMethod, (const ProxyMethodInstanceIdentifier&, std::size_t, pid_t), (override));

    MOCK_METHOD(void, UnregisterOnServiceMethodSubscribedHandler, (SkeletonInstanceIdentifier), (override));

    MOCK_METHOD(void, UnregisterMethodCallHandler, (ProxyMethodInstanceIdentifier), (override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_INSTANCE_MOCK_H
