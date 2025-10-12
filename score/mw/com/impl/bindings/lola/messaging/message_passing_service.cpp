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

#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"

#include "score/message_passing/i_server_connection.h"
#include "score/os/errno_logging.h"
#include "score/mw/log/logging.h"

#include <sstream>

namespace
{

constexpr inline std::size_t kNumberOfLocalThreads = 2U;

constexpr auto kLocalThreadPoolName = "mw::com MessageReceiver";

}  // namespace

score::mw::com::impl::lola::MessagePassingService::MessagePassingService(
    const AsilSpecificCfg config_asil_qm,
    const score::cpp::optional<AsilSpecificCfg> config_asil_b) noexcept
    : score::mw::com::impl::lola::IMessagePassingService{},
      server_factory_{},
      client_factory_{},
      local_event_thread_pool_{kNumberOfLocalThreads, kLocalThreadPoolName},
      qm_{},
      asil_b_{}
{
    score::cpp::ignore = server_factory_.emplace();
    score::cpp::ignore = client_factory_.emplace(server_factory_->GetEngine());

    if (config_asil_b.has_value())
    {
        score::cpp::ignore = asil_b_.emplace(MessagePassingServiceInstance::ClientQualityType::kASIL_B,
                                      *config_asil_b,
                                      *server_factory_,
                                      *client_factory_,
                                      local_event_thread_pool_);
        score::cpp::ignore = qm_.emplace(MessagePassingServiceInstance::ClientQualityType::kASIL_QMfromB,
                                  config_asil_qm,
                                  *server_factory_,
                                  *client_factory_,
                                  local_event_thread_pool_);
    }
    else
    {
        score::cpp::ignore = qm_.emplace(MessagePassingServiceInstance::ClientQualityType::kASIL_QM,
                                  config_asil_qm,
                                  *server_factory_,
                                  *client_factory_,
                                  local_event_thread_pool_);
    }
}

// NOLINTNEXTLINE(modernize-use-equals-default) false positive: the destructor is not trivial
score::mw::com::impl::lola::MessagePassingService::~MessagePassingService() noexcept {}

void score::mw::com::impl::lola::MessagePassingService::NotifyEvent(const QualityType asil_level,
                                                                  const ElementFqId event_id) noexcept
{
    auto& instance = asil_level == QualityType::kASIL_QM ? qm_ : asil_b_;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance.has_value(), "Invalid asil level.");

    instance->NotifyEvent(event_id);
}

score::mw::com::impl::lola::IMessagePassingService::HandlerRegistrationNoType
score::mw::com::impl::lola::MessagePassingService::RegisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    std::weak_ptr<ScopedEventReceiveHandler> callback,
    const pid_t target_node_id) noexcept
{
    auto& instance = asil_level == QualityType::kASIL_QM ? qm_ : asil_b_;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance.has_value(), "Invalid asil level.");

    return instance->RegisterEventNotification(event_id, std::move(callback), target_node_id);
}

void score::mw::com::impl::lola::MessagePassingService::ReregisterEventNotification(const QualityType asil_level,
                                                                                  const ElementFqId event_id,
                                                                                  const pid_t target_node_id) noexcept
{
    auto& instance = asil_level == QualityType::kASIL_QM ? qm_ : asil_b_;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance.has_value(), "Invalid asil level.");

    instance->ReregisterEventNotification(event_id, target_node_id);
}

void score::mw::com::impl::lola::MessagePassingService::UnregisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    auto& instance = asil_level == QualityType::kASIL_QM ? qm_ : asil_b_;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance.has_value(), "Invalid asil level.");

    instance->UnregisterEventNotification(event_id, registration_no, target_node_id);
}

void score::mw::com::impl::lola::MessagePassingService::NotifyOutdatedNodeId(const QualityType asil_level,
                                                                           const pid_t outdated_node_id,
                                                                           const pid_t target_node_id) noexcept
{
    auto& instance = asil_level == QualityType::kASIL_QM ? qm_ : asil_b_;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance.has_value(), "Invalid asil level.");

    instance->NotifyOutdatedNodeId(outdated_node_id, target_node_id);
}
