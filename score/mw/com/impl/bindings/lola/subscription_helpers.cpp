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
#include "score/mw/com/impl/bindings/lola/subscription_helpers.h"

#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/runtime.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <sstream>
#include <string>
#include <utility>

namespace score::mw::com::impl::lola
{

void EventReceiveHandlerManager::Register(std::weak_ptr<ScopedEventReceiveHandler> handler)
{
    Unregister();
    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    registration_number_ = lola_runtime.GetLolaMessaging().RegisterEventNotification(
        asil_level_, element_fq_id_, std::move(handler), event_source_pid_);
}

void EventReceiveHandlerManager::Reregister(
    std::optional<std::weak_ptr<ScopedEventReceiveHandler>> new_event_receiver_handler)
{
    if (new_event_receiver_handler.has_value())
    {
        Register(std::move(new_event_receiver_handler.value()));
    }
    else if (registration_number_.has_value())
    {
        auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
        lola_runtime.GetLolaMessaging().ReregisterEventNotification(asil_level_, element_fq_id_, event_source_pid_);
    }
}

void EventReceiveHandlerManager::Unregister() noexcept
{
    if (registration_number_.has_value())
    {
        auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
        lola_runtime.GetLolaMessaging().UnregisterEventNotification(
            asil_level_, element_fq_id_, registration_number_.value(), event_source_pid_);
        registration_number_ = score::cpp::nullopt;
    }
}

std::string CreateLoggingString(std::string&& string,
                                const ElementFqId& element_fq_id,
                                const SubscriptionStateMachineState current_state)
{
    std::stringstream s;
    // Suppress "AUTOSAR C++14 A18-9-2" rule finding: "Forwarding values to other functions shall be done via:
    // (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
    // reference".
    // Passing result of std::move() as a const reference argument, no move will actually happen.
    // coverity[autosar_cpp14_a18_9_2_violation]
    s << string << " " << element_fq_id.ToString() << MessageForSubscriptionState(current_state);
    return s.str();
}

}  // namespace score::mw::com::impl::lola
