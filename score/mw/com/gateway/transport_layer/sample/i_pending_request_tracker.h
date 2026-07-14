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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_PENDING_REQUEST_TRACKER_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_PENDING_REQUEST_TRACKER_H_

#include "score/result/result.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

namespace score::mw::com::gateway
{

/// \brief Interface of class PendingRequestTracker, which is used for testing and mocking purposes only.
class IPendingRequestTracker
{
  public:
    virtual ~IPendingRequestTracker() = default;

    /// \brief Generates the next unique sequence number for a request.
    virtual std::uint32_t GetNextSequenceNumber() = 0;

    /// \brief Registers a new pending request with the given sequence number.
    virtual void RegisterPendingRequest(std::uint32_t sequence) = 0;

    /// \brief Removes a pending request entry for the given sequence number.
    virtual void ErasePendingRequest(std::uint32_t sequence) = 0;

    /// \brief Resets the acknowledged flag for a pending request (used before retry).
    virtual void ResetAcknowledgement(std::uint32_t sequence) = 0;

    /// \brief Acknowledges the pending request with the given sequence number.
    /// Called when an ACK response is received from the remote side.
    virtual void Acknowledge(std::uint32_t sequence) = 0;

    /// \brief Waits for the pending request with the given sequence number to be acknowledged or
    /// for disconnection to occur.
    /// \param sequence The sequence number to wait for.
    /// \param timeout The maximum time to wait.
    /// \param is_connected A reference to an atomic bool indicating the connection state.
    /// \return Success if acknowledged, kNotConnected if disconnected, kTimeout if timed out,
    ///         kSendFailure if the sequence was not found.
    virtual score::ResultBlank WaitForAck(std::uint32_t sequence,
                                          std::chrono::milliseconds timeout,
                                          const std::atomic<bool>& is_connected) = 0;

    /// \brief Wakes up all threads waiting on pending requests (e.g., on shutdown or disconnect).
    virtual void NotifyAll() = 0;

    /// \brief Checks whether a pending request with the given sequence number exists.
    virtual bool HasPendingRequest(std::uint32_t sequence) = 0;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_PENDING_REQUEST_TRACKER_H_
