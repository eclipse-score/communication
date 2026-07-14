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

#ifndef SCORE_MW_COM_GATEWAY_MOCK_PENDING_REQUEST_TRACKER_H
#define SCORE_MW_COM_GATEWAY_MOCK_PENDING_REQUEST_TRACKER_H

#include "score/mw/com/gateway/transport_layer/sample/i_pending_request_tracker.h"

#include <gmock/gmock.h>

namespace score::mw::com::gateway
{

class MockPendingRequestTracker : public IPendingRequestTracker
{
  public:
    MOCK_METHOD(std::uint32_t, GetNextSequenceNumber, (), (override));
    MOCK_METHOD(void, RegisterPendingRequest, (std::uint32_t), (override));
    MOCK_METHOD(void, ErasePendingRequest, (std::uint32_t), (override));
    MOCK_METHOD(void, ResetAcknowledgement, (std::uint32_t), (override));
    MOCK_METHOD(void, Acknowledge, (std::uint32_t), (override));
    MOCK_METHOD((score::ResultBlank),
                WaitForAck,
                (std::uint32_t, std::chrono::milliseconds, const std::atomic<bool>&),
                (override));
    MOCK_METHOD(void, NotifyAll, (), (override));
    MOCK_METHOD(bool, HasPendingRequest, (std::uint32_t), (override));
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_MOCK_PENDING_REQUEST_TRACKER_H
