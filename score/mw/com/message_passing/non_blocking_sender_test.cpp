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
#include "score/mw/com/message_passing/non_blocking_sender.h"

#include "score/concurrency/executor_mock.h"
#include "score/os/errno.h"
#include "score/mw/com/message_passing/sender_mock.h"

#include <score/memory.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::com::message_passing
{
namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::Invoke;
using ::testing::Return;

constexpr MessageId SOME_MSG_ID{42};
constexpr pid_t SOME_PID{666};
constexpr ShortMessagePayload SOME_SHORT_MSG_PAYLOAD{99};
constexpr std::size_t QUEUE_SIZE{10};
constexpr std::size_t QUEUE_SIZE_TOO_LARGE{101};

class NonBlockingSenderFixture : public ::testing::Test
{
  public:
    void SetUp() override {}

    void TearDown() override {}

    void PrepareNonBlockingSender()
    {
        auto sender_mock = score::cpp::pmr::make_unique<SenderMock>(score::cpp::pmr::get_default_resource());
        // sender (mock) has to be passed as unique_ptr to UuT. We need to extract the raw pointer to be able to
        // control our mock.
        sender_mock_raw_ptr_ = sender_mock.get();
        unit_.emplace(std::move(sender_mock), QUEUE_SIZE, executorMock_);
    }

    void TryPrepareNonBlockingSenderQueueTooLarge()
    {
        auto sender_mock = score::cpp::pmr::make_unique<SenderMock>(score::cpp::pmr::get_default_resource());
        // sender (mock) has to be passed as unique_ptr to UuT. We need to extract the raw pointer to be able to
        // control our mock.
        sender_mock_raw_ptr_ = sender_mock.get();
        unit_.emplace(std::move(sender_mock), QUEUE_SIZE_TOO_LARGE, executorMock_);
    }

    static ShortMessage CreateShortMessage()
    {
        ShortMessage msg;
        msg.id = SOME_MSG_ID;
        msg.pid = SOME_PID;
        msg.payload = SOME_SHORT_MSG_PAYLOAD;
        return msg;
    }

    static MediumMessage CreateMediumMessage()
    {
        MediumMessage msg;
        msg.id = SOME_MSG_ID;
        msg.pid = SOME_PID;
        msg.payload = {'H', 'E', 'L', 'L', 'O', ' ', 'L', 'O'};
        return msg;
    }

    SenderMock* sender_mock_raw_ptr_{nullptr};
    concurrency::testing::ExecutorMock executorMock_{};

    std::optional<NonBlockingSender> unit_{};
    // Note: Order between unit_ and current_task_ is important here, to avoid deadlock! We store the current task and
    // its callable, which contains a promise here. unit_ on destruction might to a wait() (future) of a TaskResult
    // connected to current_task_! So on destruction of NonBlockingSenderFixture, we have to assure, that current_task_
    // gets destructed BEFORE unit_!
    score::cpp::pmr::unique_ptr<concurrency::Task> current_task_{};
};

enum class StopTokenState
{
    STOP_REQUESTED,
    NOT_STOP_REQUESTED
};
enum class MessageSizeType
{
    SHORT_MESSAGE,
    MEDIUM_MESSAGE
};
enum class WrappedSenderResult
{
    SEND_OK,
    SEND_FAILED
};

/// \brief Parameterized fixture based on our basic NonBlockingSenderFixture
class NonBlockingSenderParameterized
    : public NonBlockingSenderFixture,
      public testing::WithParamInterface<std::tuple<StopTokenState, MessageSizeType, WrappedSenderResult>>
{
};

TEST_F(NonBlockingSenderFixture, Creation)
{
    // we have created a NonBlockingSender instance and expect no errors.
    PrepareNonBlockingSender();
}

using NonBlockingSenderDeathTest = NonBlockingSenderFixture;
TEST_F(NonBlockingSenderDeathTest, CreationDeath)
{
    // we try to create a NonBlockingSender instance with too large queue and expect termination.
    EXPECT_DEATH(TryPrepareNonBlockingSenderQueueTooLarge(), "max_queue_size");
}

TEST_F(NonBlockingSenderFixture, NonBlockingGuarantee)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();
    // expect, that it is non-blocking
    EXPECT_TRUE(unit_.value().HasNonBlockingGuarantee());
}

/// \brief Parameterized test, to test the same sequence once with:
///        - a stopped stop_token and once with a active/not stopped stop_token.
///        - a short and a medium message
///        - with a send success and a send failure on the wrapped sender
TEST_P(NonBlockingSenderParameterized, SendMessage_EmptyQueue)
{
    MessageSizeType messageSizeType;
    StopTokenState stopTokenState;
    WrappedSenderResult wrappedSenderResult;
    std::tie(stopTokenState, messageSizeType, wrappedSenderResult) = GetParam();

    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a message on the unit.
    score::cpp::expected_blank<score::os::Error> result;
    if (messageSizeType == MessageSizeType::SHORT_MESSAGE)
    {
        result = unit_.value().Send(CreateShortMessage());
    }
    else
    {
        result = unit_.value().Send(CreateMediumMessage());
    }

    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // and expect, that Send() gets called on the wrapped sender or not depending on the stop_source state
    score::cpp::stop_source stop_source;
    int expected_send_call_count{1};
    if (stopTokenState == StopTokenState::STOP_REQUESTED)
    {
        stop_source.request_stop();
        expected_send_call_count = 0;
    }

    if (messageSizeType == MessageSizeType::SHORT_MESSAGE)
    {
        EXPECT_CALL(*sender_mock_raw_ptr_, Send(An<const message_passing::ShortMessage&>()))
            .Times(expected_send_call_count)
            .WillOnce(Return(wrappedSenderResult == WrappedSenderResult::SEND_OK
                                 ? score::cpp::expected_blank<score::os::Error>()
                                 : score::cpp::make_unexpected(score::os::Error::createFromErrno(EBUSY))));
    }
    else
    {
        EXPECT_CALL(*sender_mock_raw_ptr_, Send(An<const message_passing::MediumMessage&>()))
            .Times(expected_send_call_count)
            .WillOnce(Return(wrappedSenderResult == WrappedSenderResult::SEND_OK
                                 ? score::cpp::expected_blank<score::os::Error>()
                                 : score::cpp::make_unexpected(score::os::Error::createFromErrno(EBUSY))));
    }

    // when the posted task gets executed.
    (*current_task_)(stop_source.get_token());
}

const std::vector<std::tuple<StopTokenState, MessageSizeType, WrappedSenderResult>> send_message_param_variations{
    {StopTokenState::NOT_STOP_REQUESTED, MessageSizeType::SHORT_MESSAGE, WrappedSenderResult::SEND_OK},
    {StopTokenState::NOT_STOP_REQUESTED, MessageSizeType::MEDIUM_MESSAGE, WrappedSenderResult::SEND_OK},
    {StopTokenState::STOP_REQUESTED, MessageSizeType::SHORT_MESSAGE, WrappedSenderResult::SEND_OK},
    {StopTokenState::NOT_STOP_REQUESTED, MessageSizeType::SHORT_MESSAGE, WrappedSenderResult::SEND_FAILED},
};

/// \brief instantiate test SendMessage_EmptyQueue, with all parameter variations.
INSTANTIATE_TEST_SUITE_P(SendMessage_EmptyQueue,
                         NonBlockingSenderParameterized,
                         ::testing::ValuesIn(send_message_param_variations));

TEST_F(NonBlockingSenderFixture, SendShortMessage_NonEmptyQueue)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a short message on the unit.
    auto result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // but expect, that no further call to executor enqueue is done
    EXPECT_CALL(executorMock_, Enqueue(_)).Times(0);

    // when calling a 2nd time Send on a now non-empty queue.
    result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);
}

TEST_F(NonBlockingSenderFixture, SendMediumMessage_NonEmptyQueue)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a medium message on the unit.
    auto result = unit_.value().Send(CreateMediumMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // but expect, that no further call to executor enqueue is done
    EXPECT_CALL(executorMock_, Enqueue(_)).Times(0);

    // when calling a 2nd time Send on a now non-empty queue.
    result = unit_.value().Send(CreateMediumMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);
}

TEST_F(NonBlockingSenderFixture, SendShortMessage_FullQueue)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a short message on the unit.
    auto result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // but expect, that no further call to executor enqueue is done
    EXPECT_CALL(executorMock_, Enqueue(_)).Times(0);

    // when calling further QUEUE_SIZE-1 times Send.
    for (std::uint32_t i = 1; i < QUEUE_SIZE; i++)
    {
        // Expect, that shutdownRequested is called on the executor ...
        EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
        result = unit_.value().Send(CreateShortMessage());
        EXPECT_TRUE(result);
    }

    // when calling another Send() on the unit with a now FULL queue
    result = unit_.value().Send(CreateShortMessage());

    // expect an error (queue full - EAGAIN)
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(EAGAIN));
}

TEST_F(NonBlockingSenderFixture, SendShortMessage_MultipleFromQueue)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_))
        .Times(1)
        .WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
            current_task_ = std::move(task);
        }));

    // when we send a short message on the unit.
    auto result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // and when we send a 2nd short message on the unit (which gets queued)
    result = unit_.value().Send(CreateShortMessage());
    // expect, that 2nd Send was successful.
    EXPECT_TRUE(result);

    // and expect, that Send() gets called twice on the wrapped sender for the two queued Send calls
    score::cpp::stop_source stop_source;
    EXPECT_CALL(*sender_mock_raw_ptr_, Send(An<const ShortMessage&>())).Times(2).WillRepeatedly(Return(score::cpp::blank{}));

    // when the posted task gets executed.
    (*current_task_)(stop_source.get_token());
}

TEST_F(NonBlockingSenderFixture, SendShortMessage_EnqueueTwice)
{
    // Given a NonBlockingSender
    PrepareNonBlockingSender();

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // Expect, that enqueue is called on the executor as the sender queue is currently empty
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a short message on the unit
    auto result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);

    // but not when we send a 2nd short message on the unit (which gets queued, but not on the executor yet)
    EXPECT_CALL(executorMock_, Enqueue(_)).Times(0);
    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    result = unit_.value().Send(CreateShortMessage());
    // expect, that 2nd Send was successful.
    EXPECT_TRUE(result);

    // and expect, that Send() will be called on the wrapped sender for both Send calls
    EXPECT_CALL(*sender_mock_raw_ptr_, Send(An<const ShortMessage&>())).Times(2).WillRepeatedly(Return(score::cpp::blank{}));

    // when the posted task gets executed.
    score::cpp::stop_source stop_source;
    (*current_task_)(stop_source.get_token());

    // Expect, that shutdownRequested is called on the executor ...
    EXPECT_CALL(executorMock_, ShutdownRequested()).WillOnce(Return(false));
    // and expect, that enqueue is called on the executor again as the sender queue is now empty again
    EXPECT_CALL(executorMock_, Enqueue(_)).WillOnce(Invoke([this](score::cpp::pmr::unique_ptr<concurrency::Task> task) {
        current_task_ = std::move(task);
    }));

    // when we send a 3rd short message on the unit
    result = unit_.value().Send(CreateShortMessage());
    // expect, that Send was successful.
    EXPECT_TRUE(result);
}

}  // namespace
}  // namespace score::mw::com::message_passing
