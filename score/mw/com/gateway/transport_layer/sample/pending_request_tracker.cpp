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
#include "score/mw/com/gateway/transport_layer/sample/pending_request_tracker.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::gateway
{

std::uint32_t PendingRequestTracker::GetNextSequenceNumber()
{
    return next_sequence_.fetch_add(1U, std::memory_order_relaxed);
}

void PendingRequestTracker::RegisterPendingRequest(std::uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_requests_[sequence] = PendingRequest{};
}

void PendingRequestTracker::ErasePendingRequest(std::uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_requests_.erase(sequence);
}

void PendingRequestTracker::ResetAcknowledgement(std::uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(sequence);
    if (it != pending_requests_.end())
    {
        it->second.acknowledged = false;
    }
}

void PendingRequestTracker::Acknowledge(std::uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(sequence);
    if (it != pending_requests_.end())
    {
        it->second.acknowledged = true;
        pending_cv_.notify_all();
    }
}

score::ResultBlank PendingRequestTracker::WaitForAck(std::uint32_t sequence,
                                                     std::chrono::milliseconds timeout,
                                                     const std::atomic<bool>& is_connected)
{
    std::unique_lock<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(sequence);
    if (it == pending_requests_.end())
    {
        score::mw::log::LogError() << "PendingRequestTracker: pending request not found for sequence " << sequence;
        return score::MakeUnexpected(TransportErrorc::kSendFailure);
    }

    pending_cv_.wait_for(lock, timeout, [&it, &is_connected] {
        return it->second.acknowledged || !is_connected.load();
    });

    if (it->second.acknowledged)
    {
        score::mw::log::LogDebug() << "PendingRequestTracker: received ack for sequence " << sequence;
        return {};
    }

    if (!is_connected.load())
    {
        ::score::mw::log::LogError() << "PendingRequestTracker: connection lost while waiting for ack for sequence "
                                     << sequence;
        return score::MakeUnexpected(TransportErrorc::kNotConnected);
    }

    ::score::mw::log::LogError() << "PendingRequestTracker: timeout waiting for ack for sequence " << sequence;
    return score::MakeUnexpected(TransportErrorc::kTimeout,
                                 "timeout waiting for ack for sequence " + std::to_string(sequence));
}

void PendingRequestTracker::NotifyAll()
{
    pending_cv_.notify_all();
}

bool PendingRequestTracker::HasPendingRequest(std::uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_requests_.find(sequence) != pending_requests_.end();
}

}  // namespace score::mw::com::gateway
