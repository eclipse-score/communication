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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_factory.h"

#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"

#include "score/message_passing/i_server_connection.h"
#include "score/os/errno_logging.h"
#include "score/mw/log/logging.h"

#include <memory>
#include <optional>

namespace
{

constexpr inline std::size_t kNumberOfLocalThreads = 2U;

constexpr auto kLocalThreadPoolName = "mw::com MessageReceiver";

}  // namespace

namespace score::mw::com::impl::lola
{

// Suppress autosar_cpp14_a15_5_3_violation
// Rationale: Calling std::terminate() if any exceptions are thrown is expected as per safety requirements
// coverity[autosar_cpp14_a15_5_3_violation]
MessagePassingService::MessagePassingService(
    const AsilSpecificCfg& config_asil_qm,
    const std::optional<AsilSpecificCfg>& config_asil_b,
    // coverity[autosar_cpp14_a8_4_12_violation] Function only uses the object without affecting ownership
    const std::unique_ptr<IMessagePassingServiceInstanceFactory>& factory) noexcept
    : IMessagePassingService{},
      client_factory_{},
      // Suppress "AUTOSAR C++14 A15-4-2" rule findings. This rule states: "Throwing an exception in a
      // "noexcept" function." In this case it is ok, because the system anyways forces the process to
      // terminate if an exception is thrown.
      // coverity[autosar_cpp14_a15_4_2_violation]
      local_event_thread_pool_{kNumberOfLocalThreads, kLocalThreadPoolName},
      qm_{},
      asil_b_{}
{
    // Suppress "AUTOSAR C++14 A16-0-1" rule findings.
    // This is the standard way to determine if it runs on QNX or Unix
    // coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNX__
    score::message_passing::QnxDispatchServerFactory server_factory{client_factory_.GetEngine()};
    // coverity[autosar_cpp14_a16_0_1_violation]
#else
    score::message_passing::UnixDomainServerFactory server_factory{client_factory_.GetEngine()};
    // coverity[autosar_cpp14_a16_0_1_violation]
#endif

    const auto qm_client_quality_type =
        config_asil_b.has_value() ? ClientQualityType::kASIL_QMfromB : ClientQualityType::kASIL_QM;

    if (config_asil_b.has_value())
    {
        asil_b_ = factory->Create(
            ClientQualityType::kASIL_B, *config_asil_b, server_factory, client_factory_, local_event_thread_pool_);
    }

    qm_ = factory->Create(
        qm_client_quality_type, config_asil_qm, server_factory, client_factory_, local_event_thread_pool_);
}

void MessagePassingService::NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.NotifyEvent(event_id);
}

IMessagePassingService::HandlerRegistrationNoType MessagePassingService::RegisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    std::weak_ptr<ScopedEventReceiveHandler> callback,
    const pid_t target_node_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    return instance.RegisterEventNotification(event_id, std::move(callback), target_node_id);
}

void MessagePassingService::ReregisterEventNotification(const QualityType asil_level,
                                                        const ElementFqId event_id,
                                                        const pid_t target_node_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.ReregisterEventNotification(event_id, target_node_id);
}

void MessagePassingService::UnregisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.UnregisterEventNotification(event_id, registration_no, target_node_id);
}

void MessagePassingService::NotifyOutdatedNodeId(const QualityType asil_level,
                                                 const pid_t outdated_node_id,
                                                 const pid_t target_node_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.NotifyOutdatedNodeId(outdated_node_id, target_node_id);
}

IMessagePassingServiceInstance& MessagePassingService::GetMessagePassingServiceInstance(
    const QualityType asil_level) const
{
    // coverity[autosar_cpp14_m6_4_3_violation] kInvalid and default branches are the same
    switch (asil_level)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] return instead of break
        case QualityType::kASIL_QM:
            return *qm_;
        // coverity[autosar_cpp14_m6_4_5_violation] return instead of break
        case QualityType::kASIL_B:
            return *asil_b_;
        // coverity[autosar_cpp14_m6_4_5_violation] SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE will terminate this switch clause
        case QualityType::kInvalid:
        default:
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Invalid asil level");
    }
}

ResultBlank MessagePassingService::RegisterOnServiceMethodSubscribedHandler(
    SkeletonInstanceIdentifier /* skeleton_instance_identifier */,
    ServiceMethodSubscribedHandler /* subscribed_callback */)
{
    return {};
}

ResultBlank MessagePassingService::RegisterMethodCallHandler(ProxyInstanceIdentifier /* proxy_instance_identifier */,
                                                             MethodCallHandler /* method_call_callback */)
{
    return {};
}

void MessagePassingService::RegisterEventNotificationExistenceChangedCallback(
    const QualityType asil_level,
    const ElementFqId event_id,
    HandlerStatusChangeCallback callback) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.RegisterEventNotificationExistenceChangedCallback(event_id, std::move(callback));
}

void MessagePassingService::UnregisterEventNotificationExistenceChangedCallback(const QualityType asil_level,
                                                                                const ElementFqId event_id) noexcept
{
    auto& instance = GetMessagePassingServiceInstance(asil_level);

    instance.UnregisterEventNotificationExistenceChangedCallback(event_id);
}

ResultBlank MessagePassingService::SubscribeServiceMethod(
    const SkeletonInstanceIdentifier& /* skeleton_instance_identifier */)
{
    return {};
}

ResultBlank MessagePassingService::CallMethod(const ProxyInstanceIdentifier& /* proxy_instance_identifier */,
                                              std::size_t /* queue_position */)
{
    return {};
}

}  // namespace score::mw::com::impl::lola
