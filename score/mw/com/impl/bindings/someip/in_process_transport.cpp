/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/someip/in_process_transport.h"

#include "score/mw/com/impl/com_error.h"

#include <algorithm>
#include <cstring>

namespace score::mw::com::impl::someip
{

ITransport::~ITransport() = default;

InProcessTransport::~InProcessTransport() = default;

InProcessTransport& InProcessTransport::instance() noexcept
{
    // Suppress "AUTOSAR C++14 A3-3-2", The rule states: "Static and thread-local objects shall be
    // constant-initialized". It cannot be made const since non-const methods are called on it.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static InProcessTransport transport{};
    return transport;
}

Result<void> InProcessTransport::OfferEvent(const ElementFqId& element_fq_id)
{
    const std::lock_guard<std::mutex> lock{mutex_};
    score::cpp::ignore = offered_events_[element_fq_id];
    return {};
}

void InProcessTransport::StopOfferEvent(const ElementFqId& element_fq_id)
{
    const std::lock_guard<std::mutex> lock{mutex_};
    score::cpp::ignore = offered_events_.erase(element_fq_id);
}

Result<void> InProcessTransport::SendEvent(const ElementFqId& element_fq_id, const void* data, const std::size_t size)
{
    if ((data == nullptr) && (size != 0U))
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "SendEvent called with a nullptr payload");
    }

    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return MakeUnexpected(ComErrc::kNotOffered, "SendEvent called for an event which is not offered");
    }

    auto& last_payload = offered_event_it->second.last_payload;
    last_payload.resize(size);
    if (size != 0U)
    {
        std::memcpy(last_payload.data(), data, size);
    }
    return {};
}

Result<void> InProcessTransport::NotifyEvent(const ElementFqId& element_fq_id)
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return MakeUnexpected(ComErrc::kNotOffered, "NotifyEvent called for an event which is not offered");
    }
    ++(offered_event_it->second.notification_count);
    return {};
}

bool InProcessTransport::HasSubscribers(const ElementFqId& element_fq_id) const
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return false;
    }
    return !offered_event_it->second.subscription_ids.empty();
}

Result<std::uint32_t> InProcessTransport::Subscribe(const ElementFqId& element_fq_id)
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return MakeUnexpected(ComErrc::kServiceNotOffered, "Subscribe called for an event which is not offered");
    }
    const auto subscription_id = next_subscription_id_;
    ++next_subscription_id_;
    offered_event_it->second.subscription_ids.push_back(subscription_id);
    return subscription_id;
}

void InProcessTransport::Unsubscribe(const ElementFqId& element_fq_id, const std::uint32_t subscription_id)
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return;
    }
    auto& subscription_ids = offered_event_it->second.subscription_ids;
    const auto subscription_it = std::find(subscription_ids.begin(), subscription_ids.end(), subscription_id);
    if (subscription_it != subscription_ids.end())
    {
        score::cpp::ignore = subscription_ids.erase(subscription_it);
    }
}

std::vector<std::byte> InProcessTransport::GetLastSentPayload(const ElementFqId& element_fq_id) const
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return {};
    }
    return offered_event_it->second.last_payload;
}

std::size_t InProcessTransport::GetNotificationCount(const ElementFqId& element_fq_id) const
{
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto offered_event_it = offered_events_.find(element_fq_id);
    if (offered_event_it == offered_events_.end())
    {
        return 0U;
    }
    return offered_event_it->second.notification_count;
}

void InProcessTransport::Reset()
{
    const std::lock_guard<std::mutex> lock{mutex_};
    offered_events_.clear();
    next_subscription_id_ = 0U;
}

}  // namespace score::mw::com::impl::someip
