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

#include "score/mw/com/message_passing/sender_factory.h"
#include "score/mw/com/message_passing/sender_mock.h"

#include "score/os/mocklib/unistdmock.h"

#include <score/optional.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::Return;

constexpr pid_t OUR_PID = 4444;
constexpr pid_t REMOTE_PID = 5555;
constexpr pid_t REMOTE_PID_2 = 666;
constexpr std::int32_t ARBITRARY_SEND_QUEUE_SIZE = 42;

class MessagePassingControlFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        message_passing::SenderFactory::InjectSenderMock(&sender_mock_);
    }

    void TearDown() override {}

    void PrepareControl(bool also_activate_asil, std::int32_t send_queue_size)
    {
        score::os::MockGuard<score::os::UnistdMock> unistd_mock{};
        // expect GetNodeIdentifier() is called to determine default node_identifier
        EXPECT_CALL(*unistd_mock, getpid()).WillOnce(Return(OUR_PID));

        // when creating our MessagePassingControl
        unit_.emplace(also_activate_asil, send_queue_size);
    }

    score::cpp::optional<MessagePassingControl> unit_;
    message_passing::SenderMock sender_mock_{};
};

TEST_F(MessagePassingControlFixture, CreationQMOnly)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling GetNodeIdentifier()
    pid_t nodeId = unit_.value().GetNodeIdentifier();

    // expect that it equals our PID
    EXPECT_EQ(nodeId, OUR_PID);
}

/// \brief Test for heap allocation is needed to stimulate "deleting Dtor" for gcov func coverage.
TEST_F(MessagePassingControlFixture, CreationQMOnlyHeap)
{
    // we have a Control created for QM only
    score::os::MockGuard<score::os::UnistdMock> unistd_mock{};
    // expect GetNodeIdentifier() is called to determine default node_identifier
    EXPECT_CALL(*unistd_mock, getpid()).WillOnce(Return(OUR_PID));

    // when creating our MessagePassingControl
    auto unit_on_heap = std::make_unique<MessagePassingControl>(false, ARBITRARY_SEND_QUEUE_SIZE);
    EXPECT_TRUE(unit_on_heap);

    // when calling GetNodeIdentifier()
    pid_t nodeId = unit_on_heap->GetNodeIdentifier();

    // expect that it equals our PID
    EXPECT_EQ(nodeId, OUR_PID);
}

TEST_F(MessagePassingControlFixture, CreationQMAndAsil)
{
    // we have a Control created for QM and ASIL_B
    PrepareControl(true, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling GetNodeIdentifier()
    pid_t nodeId = unit_.value().GetNodeIdentifier();

    // expect that it equals our PID
    EXPECT_EQ(nodeId, OUR_PID);
}

TEST_F(MessagePassingControlFixture, GetMessagePassingSenderSameTwice)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling GetMessagePassingSender for a remote pid, we get a sender
    auto sender1 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
    ASSERT_NE(sender1, nullptr);
    // when calling GetMessagePassingSender for the same remote pid, we get a sender
    auto sender2 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
    ASSERT_NE(sender2, nullptr);

    // expect, that both returned senders are the same.
    EXPECT_EQ(sender1.get(), sender2.get());
}

TEST_F(MessagePassingControlFixture, GetMessagePassingSenderDifferentTwice)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling GetMessagePassingSender for a remote pid, we get a sender
    auto sender1 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
    ASSERT_NE(sender1, nullptr);
    // when calling GetMessagePassingSender for a different remote pid, we get a sender
    auto sender2 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID_2);
    ASSERT_NE(sender2, nullptr);

    // expect, that both returned senders are not the same.
    EXPECT_NE(sender1.get(), sender2.get());
}

TEST_F(MessagePassingControlFixture, CreateMessagePassingNameQM)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling CreateMessagePassingName for a remote pid
    auto name = unit_.value().CreateMessagePassingName(QualityType::kASIL_QM, REMOTE_PID);

    // expect, that both returned senders are not the same.
    EXPECT_EQ(name, "/LoLa_5555_QM");
}

TEST_F(MessagePassingControlFixture, CreateMessagePassingNameASIL)
{
    // we have a Control created for QM and ASIL
    PrepareControl(true, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling CreateMessagePassingName for a remote pid
    auto name = unit_.value().CreateMessagePassingName(QualityType::kASIL_B, REMOTE_PID);

    // expect, that both returned senders are not the same.
    EXPECT_EQ(name, "/LoLa_5555_ASIL_B");
}

TEST_F(MessagePassingControlFixture, GetMessagePassingSenderNonBlockingWrapper)
{
    // we have a Control created for QM and ASIL
    PrepareControl(true, ARBITRARY_SEND_QUEUE_SIZE);

    EXPECT_CALL(sender_mock_, HasNonBlockingGuarantee()).WillOnce(Return(false));

    // when calling GetMessagePassingSender for a remote pid, we get a sender
    auto sender = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
    ASSERT_NE(sender, nullptr);

    // expect, that the returned sender has non-blocking guarantee, since UuT should have wrapped it with a
    // NonBlockingSender in this case (ASIL-B process and a Sender towards a ASIL-QM process)
    EXPECT_TRUE(sender->HasNonBlockingGuarantee());
}

TEST_F(MessagePassingControlFixture, GetMessagePassingSenderConcurrency)
{
    // we have a Control created for QM and ASIL
    PrepareControl(true, ARBITRARY_SEND_QUEUE_SIZE);

    EXPECT_CALL(sender_mock_, HasNonBlockingGuarantee()).WillRepeatedly(Return(false));

    // and we create 10 threads, which call "concurrently" GetMessagePassingSender for different remote pids
    std::vector<std::thread> threads;
    pid_t remote_pid = REMOTE_PID;
    threads.reserve(10);
    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back(
            [this](pid_t pid) {
                auto sender = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, pid);
                ASSERT_NE(sender, nullptr);
                EXPECT_TRUE(sender->HasNonBlockingGuarantee());
            },
            remote_pid++);
    }

    // expect that no exceptions/errors occur, and we can join all threads.

    for (auto& thread : threads)
    {
        thread.join();
    }
}

TEST_F(MessagePassingControlFixture, RemoveSenderWhileStillHoldingIt)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling GetMessagePassingSender for a remote pid, we get a sender
    auto sender = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
    ASSERT_NE(sender, nullptr);

    // when calling RemoveMessagePassingSender() for the same PID, while still holding the sender previously
    // returned.
    unit_.value().RemoveMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);

    // and then when calling again GetMessagePassingSender() for the same PID, while still holding the sender_guard
    // previously returned.
    auto sender_2 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);

    // expect, that the returned optional has no value!
    EXPECT_NE(sender_2, nullptr);
    EXPECT_NE(sender_2.get(), sender.get());
}

TEST_F(MessagePassingControlFixture, RemoveSenderWhileNotHoldingIt)
{
    // we have a Control created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    {
        // when calling GetMessagePassingSender for a remote pid, we get a sender
        auto sender = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);
        ASSERT_NE(sender, nullptr);
    }

    // when calling RemoveMessagePassingSender() for the same PID, while NOT holding the sender previously
    // returned anymore
    unit_.value().RemoveMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);

    // and then when calling again GetMessagePassingSender() for the same PID
    auto sender_2 = unit_.value().GetMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);

    // expect, that the returned shared_ptr to the sender is not null!
    EXPECT_NE(sender_2, nullptr);
}

TEST_F(MessagePassingControlFixture, RemoveSenderThatWasNeverCreatedWillNotTerminate)
{
    // Given a MessagePassingControl created for QM only
    PrepareControl(false, ARBITRARY_SEND_QUEUE_SIZE);

    // when calling RemoveMessagePassingSender() for a PID for which a sender was never created
    unit_.value().RemoveMessagePassingSender(QualityType::kASIL_QM, REMOTE_PID);

    // Then the program will not terminate
}

}  // namespace
}  // namespace score::mw::com::impl::lola
