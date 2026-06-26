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

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace score::mw::com::gateway
{

class PendingRequestTrackerFixture : public ::testing::Test
{
  protected:
    PendingRequestTracker tracker_;
};

TEST_F(PendingRequestTrackerFixture, GetNextSequenceNumberReturnsIncrementingValues)
{
    // Given a new tracker
    // When GetNextSequenceNumber is called multiple times
    const auto first = tracker_.GetNextSequenceNumber();
    const auto second = tracker_.GetNextSequenceNumber();
    const auto third = tracker_.GetNextSequenceNumber();

    // Then each call returns a monotonically increasing value
    EXPECT_EQ(first, 1U);
    EXPECT_EQ(second, 2U);
    EXPECT_EQ(third, 3U);
}

TEST_F(PendingRequestTrackerFixture, RegisterPendingRequestMakesItFindable)
{
    // Given a tracker with no pending requests
    EXPECT_FALSE(tracker_.HasPendingRequest(42U));

    // When a pending request is registered
    tracker_.RegisterPendingRequest(42U);

    // Then it can be found
    EXPECT_TRUE(tracker_.HasPendingRequest(42U));
}

TEST_F(PendingRequestTrackerFixture, ErasePendingRequestRemovesIt)
{
    // Given a tracker with a registered pending request
    tracker_.RegisterPendingRequest(10U);
    ASSERT_TRUE(tracker_.HasPendingRequest(10U));

    // When the pending request is erased
    tracker_.ErasePendingRequest(10U);

    // Then it is no longer findable
    EXPECT_FALSE(tracker_.HasPendingRequest(10U));
}

TEST_F(PendingRequestTrackerFixture, ErasePendingRequestForNonExistentSequenceIsNoOp)
{
    // Given a tracker with no pending requests
    ASSERT_FALSE(tracker_.HasPendingRequest(99U));
    // When erasing a non-existent sequence
    tracker_.ErasePendingRequest(99U);

    // Then no crash or error occurs
    EXPECT_FALSE(tracker_.HasPendingRequest(99U));
}

TEST_F(PendingRequestTrackerFixture, HasPendingRequestReturnsFalseForUnknownSequence)
{
    // Given a tracker with no pending requests
    // When checking for an unknown sequence
    // Then it returns false
    EXPECT_FALSE(tracker_.HasPendingRequest(1U));
}

TEST_F(PendingRequestTrackerFixture, ResetAcknowledgementResetsAckedFlag)
{
    // Given a pending request that has been acknowledged
    tracker_.RegisterPendingRequest(5U);
    tracker_.Acknowledge(5U);

    // When ResetAcknowledgement is called
    tracker_.ResetAcknowledgement(5U);

    // Then WaitForAck times out (flag is no longer set)
    std::atomic<bool> connected{true};
    const auto result = tracker_.WaitForAck(5U, std::chrono::milliseconds(10), connected);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}

TEST_F(PendingRequestTrackerFixture, ResetAcknowledgementForNonExistentSequenceIsNoOp)
{
    // Given a tracker with no pending requests
    // When ResetAcknowledgement is called for an unknown sequence
    tracker_.ResetAcknowledgement(99U);

    // Then no crash or error occurs (no-op)
    EXPECT_FALSE(tracker_.HasPendingRequest(99U));
}

TEST_F(PendingRequestTrackerFixture, AcknowledgeSetsFlagAndWakesWaitingThread)
{
    // Given a pending request being waited on in another thread
    tracker_.RegisterPendingRequest(7U);
    std::atomic<bool> connected{true};

    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kTimeout);
    std::thread waiting_thread([this, &result, &connected]() {
        result = tracker_.WaitForAck(7U, std::chrono::milliseconds(500), connected);
    });

    // When Acknowledge is called
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tracker_.Acknowledge(7U);

    waiting_thread.join();

    // Then WaitForAck returns success
    EXPECT_TRUE(result.has_value());
}

TEST_F(PendingRequestTrackerFixture, AcknowledgeForNonExistentSequenceIsNoOp)
{
    // Given a tracker with no pending requests
    // When Acknowledge is called for an unknown sequence
    tracker_.Acknowledge(99U);

    // Then no crash or error occurs and the request is still not found
    EXPECT_FALSE(tracker_.HasPendingRequest(99U));
}

TEST_F(PendingRequestTrackerFixture, WaitForAckReturnsSuccessWhenAlreadyAcknowledged)
{
    // Given a pending request that was acknowledged before waiting
    tracker_.RegisterPendingRequest(3U);
    tracker_.Acknowledge(3U);
    std::atomic<bool> connected{true};

    // When WaitForAck is called
    const auto result = tracker_.WaitForAck(3U, std::chrono::milliseconds(100), connected);

    // Then it returns success immediately
    EXPECT_TRUE(result.has_value());
}

TEST_F(PendingRequestTrackerFixture, WaitForAckReturnsSendFailureForUnknownSequence)
{
    // Given a tracker with no pending request for the given sequence
    std::atomic<bool> connected{true};

    // When WaitForAck is called with an unknown sequence
    const auto result = tracker_.WaitForAck(99U, std::chrono::milliseconds(50), connected);

    // Then it returns kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(PendingRequestTrackerFixture, WaitForAckReturnsNotConnectedWhenDisconnected)
{
    // Given a pending request and a disconnect occurs during waiting
    tracker_.RegisterPendingRequest(8U);
    std::atomic<bool> connected{true};

    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kTimeout);
    std::thread waiting_thread([this, &result, &connected]() {
        result = tracker_.WaitForAck(8U, std::chrono::milliseconds(500), connected);
    });

    // When the connection is lost
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    connected = false;
    tracker_.NotifyAll();

    waiting_thread.join();

    // Then WaitForAck returns kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(PendingRequestTrackerFixture, WaitForAckReturnsTimeoutWhenNoAckAndStillConnected)
{
    // Given a pending request that is never acknowledged
    tracker_.RegisterPendingRequest(9U);
    std::atomic<bool> connected{true};

    // When WaitForAck is called with a short timeout
    const auto result = tracker_.WaitForAck(9U, std::chrono::milliseconds(10), connected);

    // Then it returns kTimeout
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}

TEST_F(PendingRequestTrackerFixture, NotifyAllWakesUpWaitingThreads)
{
    // Given a pending request being waited on
    tracker_.RegisterPendingRequest(11U);
    std::atomic<bool> connected{true};

    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::thread waiting_thread([this, &result, &connected]() {
        result = tracker_.WaitForAck(11U, std::chrono::milliseconds(500), connected);
    });

    // When the connection is lost and NotifyAll is called to wake waiting threads
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    connected = false;
    tracker_.NotifyAll();

    waiting_thread.join();

    // Then the waiting_thread wakes up and returns kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

}  // namespace score::mw::com::gateway
