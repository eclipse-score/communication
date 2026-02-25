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

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <gmock/gmock.h>

#include <memory>

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
    MOCK_METHOD(void,
                RegisterEventNotificationExistenceChangedCallback,
                (QualityType, ElementFqId, HandlerStatusChangeCallback),
                (noexcept, override));
    MOCK_METHOD(void,
                UnregisterEventNotificationExistenceChangedCallback,
                (QualityType, ElementFqId),
                (noexcept, override));

    MOCK_METHOD(Result<MethodSubscriptionRegistrationGuard>,
                RegisterOnServiceMethodSubscribedHandler,
                (QualityType, SkeletonInstanceIdentifier, ServiceMethodSubscribedHandler, AllowedConsumerUids),
                (override));
    MOCK_METHOD(Result<MethodSubscriptionRegistrationGuard>,
                RegisterMethodCallHandler,
                (QualityType, ProxyMethodInstanceIdentifier, MethodCallHandler, uid_t),
                (override));
    MOCK_METHOD(ResultBlank,
                SubscribeServiceMethod,
                (QualityType, const SkeletonInstanceIdentifier&, const ProxyInstanceIdentifier&, pid_t),
                (override));
    MOCK_METHOD(ResultBlank,
                CallMethod,
                (QualityType, const ProxyMethodInstanceIdentifier&, std::size_t, pid_t),
                (override));

    MOCK_METHOD(void,
                UnregisterOnServiceMethodSubscribedHandler,
                (const QualityType asil_level, SkeletonInstanceIdentifier skeleton_instance_identifier),
                (override));
    MOCK_METHOD(void,
                UnregisterMethodCallHandler,
                (const QualityType asil_level, ProxyMethodInstanceIdentifier proxy_method_instance_identifier),
                (override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_MESSAGE_PASSING_SERVICE_MOCK_H
