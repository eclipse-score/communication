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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_PENDING_REQUEST_TRACKER_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_PENDING_REQUEST_TRACKER_H_

#include "score/mw/com/gateway/transport_layer/sample/i_pending_request_tracker.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/result/result.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>

namespace score::mw::com::gateway
{

/// \brief Tracks the correlation between requests and their responses.
///
/// This class manages the lifecycle of pending requests that are waiting for an acknowledgement (ACK).
/// It provides the operations for registering, acknowledging, and waiting on pending requests.
class PendingRequestTracker : public IPendingRequestTracker
{
    friend class PendingRequestTrackerAttorney;   // For testing purposes only
    friend class BidirectionalTransportAttorney;  // For testing purposes only

  public:
    PendingRequestTracker() = default;
    ~PendingRequestTracker() = default;

    PendingRequestTracker(const PendingRequestTracker&) = delete;
    PendingRequestTracker& operator=(const PendingRequestTracker&) = delete;
    PendingRequestTracker(PendingRequestTracker&&) = delete;
    PendingRequestTracker& operator=(PendingRequestTracker&&) = delete;

    /// \brief Generates the next unique sequence number for a request.
    std::uint32_t GetNextSequenceNumber() override;

    /// \brief Registers a new pending request with the given sequence number.
    void RegisterPendingRequest(std::uint32_t sequence) override;

    /// \brief Removes a pending request entry for the given sequence number.
    void ErasePendingRequest(std::uint32_t sequence) override;

    /// \brief Resets the acknowledged flag for a pending request (used before retry).
    void ResetAcknowledgement(std::uint32_t sequence) override;

    /// \brief Acknowledges the pending request with the given sequence number.
    /// Called when an ACK response is received from the remote side.
    void Acknowledge(std::uint32_t sequence) override;

    /// \brief Waits for the pending request with the given sequence number to be acknowledged or
    /// for disconnection to occur.
    /// \param sequence The sequence number to wait for.
    /// \param timeout The maximum time to wait.
    /// \param is_connected A reference to an atomic bool indicating the connection state.
    /// \return Success if acknowledged, kNotConnected if disconnected, kTimeout if timed out,
    ///         kSendFailure if the sequence was not found.
    score::ResultBlank WaitForAck(std::uint32_t sequence,
                                  std::chrono::milliseconds timeout,
                                  const std::atomic<bool>& is_connected) override;

    /// \brief Wakes up all threads waiting on pending requests (e.g., on shutdown or disconnect).
    void NotifyAll() override;

    /// \brief Checks whether a pending request with the given sequence number exists.
    bool HasPendingRequest(std::uint32_t sequence) override;

  private:
    struct PendingRequest
    {
        bool acknowledged{false};
    };

    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::atomic<std::uint32_t> next_sequence_{1U};
    std::map<std::uint32_t, PendingRequest> pending_requests_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_PENDING_REQUEST_TRACKER_H_
