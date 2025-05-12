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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_control.h"

#include "score/os/unistd.h"
#include "score/mw/com/message_passing/i_sender.h"
#include "score/mw/com/message_passing/non_blocking_sender.h"
#include "score/mw/com/message_passing/sender_factory.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{
constexpr auto mq_name_prefix = "/LoLa_";
constexpr auto mq_name_qm_postfix = "_QM";
constexpr auto mq_name_asil_b_postfix = "_ASIL_B";
}  // namespace

MessagePassingControl::MessagePassingControl(const bool asil_b_capability,
                                             const std::int32_t sender_queue_size) noexcept
    : score::mw::com::impl::lola::IMessagePassingControl{},
      asil_b_capability_{asil_b_capability},
      sender_queue_size_{sender_queue_size},
      node_identifier_{score::os::Unistd::instance().getpid()},
      senders_qm_{},
      senders_asil_{},
      stop_source_{},
      non_blocking_sender_thread_pool_{}
{
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
std::shared_ptr<message_passing::ISender> MessagePassingControl::GetMessagePassingSender(
    const score::mw::com::impl::QualityType asil_level,
    const pid_t target_node_id) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        (asil_level == QualityType::kASIL_QM) || ((asil_level == QualityType::kASIL_B) && asil_b_capability_),
        "Invalid asil level.");

    auto& senders = asil_level == QualityType::kASIL_QM ? senders_qm_ : senders_asil_;
    auto& mutex = asil_level == QualityType::kASIL_QM ? senders_qm_mutex_ : senders_asil_mutex_;

    std::lock_guard<std::mutex> lck(mutex);

    auto search = senders.find(target_node_id);
    if (search != senders.end())
    {
        return search->second;
    }

    // create the OS specific sender
    auto new_sender = CreateNewSender(asil_level, target_node_id);

    auto elem = senders.emplace(target_node_id, new_sender);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(elem.second, "MessagePassingControl::GetMessagePassingSender(): Failed to emplace Sender!");

    return elem.first->second;
}

// false-positive: one definition rule is not violated, function is defined in src file
// coverity[autosar_cpp14_a3_1_1_violation]
std::shared_ptr<message_passing::ISender> MessagePassingControl::CreateNewSender(const QualityType& asil_level,
                                                                                 const pid_t target_node_id)
{
    const std::string senderName = CreateMessagePassingName(asil_level, target_node_id);

    const message_passing::SenderConfig& sender_config = {};
    message_passing::LoggingCallback logging_callback = &message_passing::DefaultLoggingCallback;
    // AUTOSAR C++14 Declarations (AUTOSAR C++14 A7-1-2): Declaring memory_resource without constexpr
    // " The constexpr specifier shall be used for values that can be determined at compile time."
    // This is a false positive. memory_resource is neither known at compiletime nor a constant. We are using a constant
    // pointer to a non-constant value here.
    // coverity[autosar_cpp14_a7_1_2_violation]
    score::cpp::pmr::memory_resource* const memory_resource = score::cpp::pmr::get_default_resource();

    auto new_sender_unique_p = message_passing::SenderFactory::Create(
        senderName.data(), stop_source_.get_token(), sender_config, std::move(logging_callback), memory_resource);

    // In case we are ASIL-B ourselves, are sending towards an ASIL-QM receiver and the OS specific sender
    // doesn't warrant non-blocking sending in any case, we wrap the sender with wrapper, which gives the guarantee
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This a false-positive, all operands are parenthesized.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    if (asil_b_capability_ && (not new_sender_unique_p->HasNonBlockingGuarantee()) &&
        (asil_level == QualityType::kASIL_QM))
    {
        new_sender_unique_p =
            score::cpp::pmr::make_unique<message_passing::NonBlockingSender>(score::cpp::pmr::get_default_resource(),
                                                                      std::move(new_sender_unique_p),
                                                                      static_cast<std::size_t>(sender_queue_size_),
                                                                      GetNonBlockingSenderThreadPool());
    }

    auto deleter = new_sender_unique_p.get_deleter();
    // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function shall
    // have automatic storage duration".
    // The object will be emplaced into the senders map, so we need to allocate it in the heap.
    // coverity[autosar_cpp14_a18_5_8_violation]
    std::shared_ptr<message_passing::ISender> new_sender{
        new_sender_unique_p.release(), deleter, score::cpp::pmr::polymorphic_allocator(memory_resource)};

    return new_sender;
}

void MessagePassingControl::RemoveMessagePassingSender(const QualityType asil_level, pid_t target_node_id)
{
    auto& senders = asil_level == QualityType::kASIL_QM ? senders_qm_ : senders_asil_;
    auto& mutex = asil_level == QualityType::kASIL_QM ? senders_qm_mutex_ : senders_asil_mutex_;
    std::lock_guard<std::mutex> lck(mutex);

    auto search = senders.find(target_node_id);
    if (search != senders.end())
    {
        score::cpp::ignore = senders.erase(search);
    }
}

std::string MessagePassingControl::CreateMessagePassingName(const QualityType asil_level, const pid_t node_id) noexcept
{
    std::stringstream identifier;
    identifier << mq_name_prefix << node_id;
    if (asil_level == QualityType::kASIL_QM)
    {
        identifier << mq_name_qm_postfix;
    }
    else
    {
        identifier << mq_name_asil_b_postfix;
    }
    return identifier.str();
}

pid_t score::mw::com::impl::lola::MessagePassingControl::GetNodeIdentifier() const noexcept
{
    return node_identifier_;
}

concurrency::ThreadPool& MessagePassingControl::GetNonBlockingSenderThreadPool()
{
    if (non_blocking_sender_thread_pool_.has_value() == false)
    {
        // The non-blocking sender anyhow only applies one task at a time
        constexpr std::size_t thread_pool_size = 1U;
        return non_blocking_sender_thread_pool_.emplace(thread_pool_size);
    }
    else
    {
        return non_blocking_sender_thread_pool_.value();
    }
}

}  // namespace score::mw::com::impl::lola
