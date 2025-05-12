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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_facade.h"

#include "score/os/errno_logging.h"
#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <utility>

score::mw::com::impl::lola::MessagePassingFacade::MessagePassingFacade(
    score::cpp::stop_source& stop_source,
    std::unique_ptr<INotifyEventHandler> notify_event_handler,
    IMessagePassingControl& msgpass_ctrl,
    const AsilSpecificCfg config_asil_qm,
    const score::cpp::optional<AsilSpecificCfg> config_asil_b) noexcept
    : score::mw::com::impl::lola::IMessagePassingService{},
      message_passing_ctrl_(msgpass_ctrl),
      asil_b_capability_{config_asil_b.has_value()},
      stop_source_{stop_source},
      notify_event_handler_{std::move(notify_event_handler)}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(notify_event_handler_ != nullptr);
    score::cpp::ignore = asil_b_capability_;

    // Suppress "AUTOSAR C++14 M12-1-1", The rule states: "An object’s dynamic type shall not be used from the body of
    // its constructor or destructor".
    // This is false positive. InitializeMessagePassingReceiver is not a virtual function.
    // coverity[autosar_cpp14_m12_1_1_violation]
    InitializeMessagePassingReceiver(
        QualityType::kASIL_QM, config_asil_qm.allowed_user_ids_, config_asil_qm.message_queue_rx_size_);
    if (asil_b_capability_)
    {
        InitializeMessagePassingReceiver(QualityType::kASIL_B,
                                         config_asil_b.value().allowed_user_ids_,
                                         config_asil_b.value().message_queue_rx_size_);
    }
}

score::mw::com::impl::lola::MessagePassingFacade::~MessagePassingFacade() noexcept
{
    // This function call will always return true.
    // As stop is requested only once.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(stop_source_.request_stop(), "unexpected return value");
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". Failing to create a thread will cause the program to terminate.
// coverity[autosar_cpp14_a3_1_1_violation]
// coverity[autosar_cpp14_a15_5_3_violation]
void score::mw::com::impl::lola::MessagePassingFacade::InitializeMessagePassingReceiver(
    const QualityType asil_level,
    const std::vector<uid_t>& allowed_user_ids,
    const std::int32_t min_num_messages) noexcept
{
    const auto receiverName =
        message_passing_ctrl_.CreateMessagePassingName(asil_level, message_passing_ctrl_.GetNodeIdentifier());

    auto& receiver = (asil_level == QualityType::kASIL_QM ? msg_receiver_qm_ : msg_receiver_asil_b_);
    const std::string thread_pool_name =
        (asil_level == QualityType::kASIL_QM) ? "mw::com MessageReceiver QM" : "mw::com MessageReceiver ASIL-B";
    // \todo Maybe we should make thread pool size configurable via configuration (deployment). Then we can decide how
    // many threads to spend over all and if we should have different number of threads for ASIL-B/QM receivers!
    // We currently restrict to two threads: Reading out hw_concurrency via std::thread::hardware_concurrency gives us
    // a very high core number, which would result in many threads, which costs too much resources and this also makes
    // no sense at all as we would need the number of cores pinned to our current process/task, which
    // std::thread::hardware_concurrency doesn't give us.
    // Currently using 2 threads for decoupled local event notification. Could be even minimized to 1, if needed.
    // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "If a function is declared to be noexcept,
    // noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
    // This might only throw std::system_error exception if the thread could not be started, if the thread couldn't
    // start the program will terminate.
    // coverity[autosar_cpp14_a15_4_2_violation]
    receiver.thread_pool_ = std::make_unique<score::concurrency::ThreadPool>(2U, thread_pool_name);
    // Suppress "AUTOSAR C++14 M8-5-2" rule finding. This rule declares: "Braces shall be used to indicate and match
    // the structure in the non-zero initialization of arrays and structures"
    // False positive as we here use initialization list.
    // coverity[autosar_cpp14_m8_5_2_violation]
    const score::mw::com::message_passing::ReceiverConfig receiver_config{min_num_messages};
    receiver.receiver_ = message_passing::ReceiverFactory::Create(
        receiverName, *receiver.thread_pool_, allowed_user_ids, receiver_config);

    notify_event_handler_->RegisterMessageReceivedCallbacks(asil_level, *receiver.receiver_);

    auto result = receiver.receiver_->StartListening();
    if (result.has_value() == false)
    {
        score::mw::log::LogFatal("lola")
            << "MessagePassingFacade: Failed to start listening on message_passing receiver with following error: "
            << result.error();
        std::terminate();
    }
}

void score::mw::com::impl::lola::MessagePassingFacade::NotifyEvent(const QualityType asil_level,
                                                                 const ElementFqId event_id) noexcept
{
    notify_event_handler_->NotifyEvent(asil_level, event_id);
}

score::mw::com::impl::lola::IMessagePassingService::HandlerRegistrationNoType
score::mw::com::impl::lola::MessagePassingFacade::RegisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    std::weak_ptr<ScopedEventReceiveHandler> callback,
    const pid_t target_node_id) noexcept
{
    return notify_event_handler_->RegisterEventNotification(asil_level, event_id, std::move(callback), target_node_id);
}

void score::mw::com::impl::lola::MessagePassingFacade::ReregisterEventNotification(QualityType asil_level,
                                                                                 ElementFqId event_id,
                                                                                 pid_t target_node_id) noexcept
{
    notify_event_handler_->ReregisterEventNotification(asil_level, event_id, target_node_id);
}

void score::mw::com::impl::lola::MessagePassingFacade::UnregisterEventNotification(
    const QualityType asil_level,
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    notify_event_handler_->UnregisterEventNotification(asil_level, event_id, registration_no, target_node_id);
}

void score::mw::com::impl::lola::MessagePassingFacade::NotifyOutdatedNodeId(QualityType asil_level,
                                                                          pid_t outdated_node_id,
                                                                          pid_t target_node_id) noexcept
{
    notify_event_handler_->NotifyOutdatedNodeId(asil_level, outdated_node_id, target_node_id);
}
