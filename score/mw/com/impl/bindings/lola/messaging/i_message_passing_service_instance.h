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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICEINSTANCE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICEINSTANCE_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/client_quality_type.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include <sched.h>

namespace score::mw::com::impl::lola
{

class IMessagePassingServiceInstance
{
  public:
    IMessagePassingServiceInstance() noexcept = default;

    IMessagePassingServiceInstance(const IMessagePassingServiceInstance&) = delete;
    IMessagePassingServiceInstance(IMessagePassingServiceInstance&&) = delete;
    IMessagePassingServiceInstance& operator=(const IMessagePassingServiceInstance&) = delete;
    IMessagePassingServiceInstance& operator=(IMessagePassingServiceInstance&&) = delete;

    virtual ~IMessagePassingServiceInstance() noexcept = default;

    virtual void NotifyEvent(const ElementFqId event_id) noexcept = 0;

    virtual IMessagePassingService::HandlerRegistrationNoType RegisterEventNotification(
        const ElementFqId event_id,
        std::weak_ptr<ScopedEventReceiveHandler> callback,
        const pid_t target_node_id) noexcept = 0;

    virtual void ReregisterEventNotification(const ElementFqId event_id, const pid_t target_node_id) noexcept = 0;

    virtual void UnregisterEventNotification(const ElementFqId event_id,
                                             const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                             const pid_t target_node_id) noexcept = 0;

    virtual ResultBlank RegisterOnServiceMethodSubscribedHandler(
        const SkeletonInstanceIdentifier skeleton_instance_identifier,
        IMessagePassingService::ServiceMethodSubscribedHandler subscribed_callback,
        IMessagePassingService::AllowedConsumerUids allowed_proxy_uids) = 0;

    virtual ResultBlank RegisterMethodCallHandler(const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
                                                  IMessagePassingService::MethodCallHandler method_call_callback,
                                                  const uid_t allowed_proxy_uid) = 0;

    virtual void UnregisterOnServiceMethodSubscribedHandler(
        SkeletonInstanceIdentifier skeleton_instance_identifier) = 0;

    virtual void UnregisterMethodCallHandler(ProxyMethodInstanceIdentifier proxy_method_instance_identifier) = 0;

    virtual void NotifyOutdatedNodeId(const pid_t outdated_node_id, const pid_t target_node_id) noexcept = 0;

    virtual void RegisterEventNotificationExistenceChangedCallback(
        const ElementFqId event_id,
        IMessagePassingService::HandlerStatusChangeCallback callback) noexcept = 0;

    virtual void UnregisterEventNotificationExistenceChangedCallback(const ElementFqId event_id) noexcept = 0;

    virtual ResultBlank SubscribeServiceMethod(const SkeletonInstanceIdentifier& skeleton_instance_identifier,
                                               const ProxyInstanceIdentifier& proxy_instance_identifier,
                                               const pid_t target_node_id) = 0;

    virtual ResultBlank CallMethod(const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
                                   const std::size_t queue_position,
                                   const pid_t target_node_id) = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICEINSTANCE_H
