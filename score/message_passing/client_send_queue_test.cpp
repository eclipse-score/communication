#include "score/message_passing/client_send_queue.h"

#include "score/message_passing/client_server_communication.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <future>
#include <utility>
#include <vector>

namespace score::message_passing::detail
{
namespace
{

using State = IClientConnection::State;
using StopReason = IClientConnection::StopReason;
using SendResult = ClientSendQueue::SendResult;
using Message = ClientSendQueue::Message;

constexpr std::int32_t kDescriptor{42};
constexpr std::uint32_t kMaxSendSize{8U};

class ClientSendQueueTest : public ::testing::Test
{
  protected:
    struct SentMessage
    {
        std::int32_t descriptor;
        std::uint8_t code;
        std::vector<std::uint8_t> bytes;
    };

    void SetUp() override
    {
        lifetime_.InstallDescriptor(kDescriptor);
    }

    ClientSendQueue MakeQueue(const IClientFactory::ClientConfig& config = {0, 2, false, false, false})
    {
        return ClientSendQueue(
            lifetime_,
            score::cpp::pmr::get_default_resource(),
            config,
            kMaxSendSize,
            [this](std::int32_t descriptor, std::uint8_t code, Message message) {
                sent_.push_back(SentMessage{descriptor, code, {message.begin(), message.end()}});
                if (!send_action_.empty())
                {
                    return send_action_(descriptor, code, message);
                }
                return SendResult{};
            },
            [this]() noexcept {
                ++schedule_count_;
            });
    }

    void StopAndCancel(ClientSendQueue& queue)
    {
        score::cpp::ignore = lifetime_.TrySetStopReason(StopReason::kUserRequested);
        queue.CancelPending();
        EXPECT_TRUE(lifetime_.CancellationFinished());
        score::cpp::ignore = lifetime_.RetireDescriptor();
    }

    std::atomic<State> state_{State::kReady};
    std::atomic<StopReason> reason_{StopReason::kNone};
    std::uint32_t retirement_count_{0};
    ClientConnectionLifetime lifetime_{state_, reason_, [this]() noexcept {
                                           ++retirement_count_;
                                       }};
    std::vector<SentMessage> sent_;
    std::uint32_t schedule_count_{0};
    ClientSendQueue::SendActor send_action_{};
};

TEST_F(ClientSendQueueTest, OversizedMessagesAreRejectedBeforeAdmission)
{
    // Given messages larger than the configured send limit
    auto queue = MakeQueue();
    std::array<std::uint8_t, kMaxSendSize + 1> message{};
    std::array<std::uint8_t, 1> reply{};

    // When each send API is called
    const auto send = queue.Send(message);
    const auto callback_send = queue.SendWithCallback(message, [](auto) {});
    const auto blocking_send = queue.SendWaitReply(message, reply);

    // Then each reports EMSGSIZE without transport or scheduling
    ASSERT_FALSE(send.has_value());
    ASSERT_FALSE(callback_send.has_value());
    ASSERT_FALSE(blocking_send.has_value());
    EXPECT_EQ(send.error().GetOsDependentErrorCode(), EMSGSIZE);
    EXPECT_EQ(callback_send.error().GetOsDependentErrorCode(), EMSGSIZE);
    EXPECT_EQ(blocking_send.error().GetOsDependentErrorCode(), EMSGSIZE);
    EXPECT_TRUE(sent_.empty());
    EXPECT_EQ(schedule_count_, 0U);
}

TEST_F(ClientSendQueueTest, SelectedStopReasonRejectsEverySendApi)
{
    // Given a ready state whose owner has selected stopping
    auto queue = MakeQueue();
    ASSERT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 1> reply{};

    // When each send API is called
    const auto send = queue.Send(message);
    const auto callback_send = queue.SendWithCallback(message, [](auto) {});
    const auto blocking_send = queue.SendWaitReply(message, reply);

    // Then each reports EINVAL and no work is admitted
    ASSERT_FALSE(send.has_value());
    ASSERT_FALSE(callback_send.has_value());
    ASSERT_FALSE(blocking_send.has_value());
    EXPECT_EQ(send.error().GetOsDependentErrorCode(), EINVAL);
    EXPECT_EQ(callback_send.error().GetOsDependentErrorCode(), EINVAL);
    EXPECT_EQ(blocking_send.error().GetOsDependentErrorCode(), EINVAL);
    EXPECT_TRUE(sent_.empty());
    EXPECT_EQ(schedule_count_, 0U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, DirectOneWaySendPassesDescriptorCodeAndBytesToActor)
{
    // Given a default queue with an accepting connection
    auto queue = MakeQueue();
    std::array<std::uint8_t, 2> message{3U, 4U};

    // When a one-way message is sent
    const auto result = queue.Send(message);

    // Then transport receives the live descriptor, SEND code, and unchanged bytes
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(sent_.size(), 1U);
    EXPECT_EQ(sent_[0].descriptor, kDescriptor);
    EXPECT_EQ(sent_[0].code, score::cpp::to_underlying(ClientToServer::SEND));
    EXPECT_EQ(sent_[0].bytes, (std::vector<std::uint8_t>{3U, 4U}));
    EXPECT_EQ(schedule_count_, 0U);
}

TEST_F(ClientSendQueueTest, AsyncQueueCopiesMessagesAndDrainsInFifoOrder)
{
    // Given a truly asynchronous queue with two preallocated slots
    auto queue = MakeQueue({0, 2, true, true, false});
    std::array<std::uint8_t, 1> first{1U};
    std::array<std::uint8_t, 1> second{2U};
    ASSERT_TRUE(queue.Send(first));
    ASSERT_TRUE(queue.Send(second));
    EXPECT_EQ(schedule_count_, 1U);
    const auto exhausted = queue.Send(first);
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(exhausted.error().GetOsDependentErrorCode(), ENOBUFS);
    first[0] = 9U;

    // When the queued command drains the copied messages
    queue.Drain();

    // Then FIFO payloads are unaffected by caller mutation and the pool is reusable
    ASSERT_EQ(sent_.size(), 2U);
    EXPECT_EQ(sent_[0].bytes, (std::vector<std::uint8_t>{1U}));
    EXPECT_EQ(sent_[1].bytes, (std::vector<std::uint8_t>{2U}));
    EXPECT_TRUE(queue.Send(first));
    EXPECT_TRUE(queue.Send(second));
    EXPECT_EQ(schedule_count_, 2U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, EmptyPoolRejectsAsyncAdmissionWithoutScheduling)
{
    // Given truly asynchronous delivery without any command storage
    auto queue = MakeQueue({0, 0, true, true, false});
    std::array<std::uint8_t, 1> message{1U};

    // When a send is submitted
    const auto result = queue.Send(message);

    // Then it reports ENOBUFS without arming or invoking transport
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), ENOBUFS);
    EXPECT_TRUE(sent_.empty());
    EXPECT_EQ(schedule_count_, 0U);
}

TEST_F(ClientSendQueueTest, OrderedSendWaitsForPendingReplyAndDrainsBeforeCallback)
{
    // Given a pending request followed by an ordered one-way message
    auto queue = MakeQueue({0, 2, true, false, false});
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 1> reply{7U};
    std::uint32_t callback_count{0};
    ASSERT_TRUE(queue.SendWithCallback(message, [this, &callback_count](auto result) {
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(sent_.size(), 2U);
        ++callback_count;
    }));
    ASSERT_TRUE(queue.Send(message));
    ASSERT_EQ(sent_.size(), 1U);
    EXPECT_EQ(sent_[0].code, score::cpp::to_underlying(ClientToServer::REQUEST));

    // When the request's reply is delivered
    EXPECT_TRUE(queue.HandleReply(reply));

    // Then the queued send precedes callback delivery and completion occurs once
    ASSERT_EQ(sent_.size(), 2U);
    EXPECT_EQ(sent_[1].code, score::cpp::to_underlying(ClientToServer::SEND));
    EXPECT_EQ(callback_count, 1U);
    EXPECT_FALSE(queue.HandleReply(reply));
    StopAndCancel(queue);
    EXPECT_EQ(callback_count, 1U);
}

TEST_F(ClientSendQueueTest, DirectTransportErrorRejectsCallbackAndReleasesReservation)
{
    // Given direct request transport which fails before callback completion
    auto queue = MakeQueue();
    std::array<std::uint8_t, 1> message{1U};
    std::uint32_t callback_count{0};
    send_action_ = [](auto, auto, auto) {
        return SendResult{score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))};
    };

    // When a callback request is sent
    const auto result = queue.SendWithCallback(message, [&callback_count](auto) {
        ++callback_count;
    });

    // Then the error is returned locally and the callback is not invoked
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);
    EXPECT_EQ(callback_count, 0U);
    send_action_ = {};
    EXPECT_TRUE(queue.SendWithCallback(message, [&callback_count](auto) {
        ++callback_count;
    }));
    EXPECT_EQ(schedule_count_, 0U);
    StopAndCancel(queue);
    EXPECT_EQ(callback_count, 1U);
}

TEST_F(ClientSendQueueTest, QueuedTransportFailureCompletesCallbackAndContinuesDrain)
{
    // Given an asynchronous request followed by a one-way send
    auto queue = MakeQueue({1, 1, true, true, false});
    std::array<std::uint8_t, 1> message{1U};
    std::uint32_t callback_count{0};
    ASSERT_TRUE(queue.SendWithCallback(message, [&callback_count](auto result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);
        ++callback_count;
    }));
    ASSERT_TRUE(queue.Send(message));
    send_action_ = [](auto, auto code, auto) {
        if (code == score::cpp::to_underlying(ClientToServer::REQUEST))
        {
            return SendResult{score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))};
        }
        return SendResult{};
    };

    // When the queue drains and request transport fails
    queue.Drain();

    // Then the callback receives one error and the next send still runs
    EXPECT_EQ(callback_count, 1U);
    ASSERT_EQ(sent_.size(), 2U);
    EXPECT_EQ(sent_[1].code, score::cpp::to_underlying(ClientToServer::SEND));
    StopAndCancel(queue);
    EXPECT_EQ(callback_count, 1U);
}

TEST_F(ClientSendQueueTest, ReplyWithoutRequestOrDuringEmptyMarkerIsRejected)
{
    // Given a queue with no request, then an armed one-way drain marker
    auto queue = MakeQueue({0, 2, true, true, false});
    std::array<std::uint8_t, 1> message{1U};
    EXPECT_FALSE(queue.HandleReply(message));
    ASSERT_TRUE(queue.Send(message));

    // When a reply arrives before the armed drain executes
    const bool accepted = queue.HandleReply(message);

    // Then the marker is not treated as a callback and transport remains pending
    EXPECT_FALSE(accepted);
    EXPECT_TRUE(sent_.empty());
    EXPECT_EQ(schedule_count_, 1U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, CancellationCallbacksCanReenterWithoutAdmittingNewWork)
{
    // Given a queued request whose cancellation callback tries sending again
    auto queue = MakeQueue({0, 2, true, true, false});
    std::array<std::uint8_t, 1> message{1U};
    std::uint32_t cancellation_count{0};
    ASSERT_TRUE(queue.SendWithCallback(message, [&queue, &cancellation_count](auto result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);
        const auto rejected = queue.Send({});
        EXPECT_FALSE(rejected.has_value());
        EXPECT_EQ(rejected.error().GetOsDependentErrorCode(), EINVAL);
        ++cancellation_count;
    }));
    ASSERT_TRUE(queue.Send(message));

    // When the owner stops admission and cancels the queue
    StopAndCancel(queue);

    // Then cancellation completes once without deadlocking or sending new work
    EXPECT_EQ(cancellation_count, 1U);
    EXPECT_TRUE(sent_.empty());
}

TEST_F(ClientSendQueueTest, BlockingRequestHandlesReplyBeforeTransportReturns)
{
    // Given direct transport which delivers its reply before returning
    auto queue = MakeQueue();
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 2> server_reply{5U, 6U};
    std::array<std::uint8_t, 4> reply{};
    send_action_ = [&queue, &server_reply](auto, auto, auto) {
        EXPECT_TRUE(queue.HandleReply(server_reply));
        return SendResult{};
    };

    // When the blocking request is issued
    const auto result = queue.SendWaitReply(message, reply);

    // Then the ready predicate returns copied data without an additional wait
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2U);
    EXPECT_EQ(result.value().data(), reply.data());
    EXPECT_EQ(reply[0], 5U);
    EXPECT_EQ(reply[1], 6U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, BlockingRequestReportsReplyBufferOverflow)
{
    // Given a server reply larger than the caller's reply buffer
    auto queue = MakeQueue();
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 2> server_reply{5U, 6U};
    std::array<std::uint8_t, 1> reply{};
    send_action_ = [&queue, &server_reply](auto, auto, auto) {
        EXPECT_TRUE(queue.HandleReply(server_reply));
        return SendResult{};
    };

    // When the blocking request receives that reply
    const auto result = queue.SendWaitReply(message, reply);

    // Then EMSGSIZE is returned without overwriting the caller's buffer
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EMSGSIZE);
    EXPECT_EQ(reply[0], 0U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, NativeStyleTransactionLeavesPendingReplyAndQueueUntouched)
{
    // Given an ordinary request with a one-way send queued behind its pending reply
    auto queue = MakeQueue({0, 2, true, false, false});
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 1> ordinary_reply{4U};
    std::array<std::uint8_t, 1> transaction_reply{9U};
    std::uint32_t callback_count{0};
    ASSERT_TRUE(queue.SendWithCallback(message, [&callback_count](auto result) {
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().front(), 4U);
        ++callback_count;
    }));
    ASSERT_TRUE(queue.Send(message));

    // When a native-style transaction uses lifetime admission without queue sequencing
    const auto result = lifetime_.ExecuteTransaction([&transaction_reply](auto) {
        return ClientSendQueue::ReplyResult{Message{transaction_reply}};
    });

    // Then its result does not consume the ordinary reply or drain queued sends
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().front(), 9U);
    EXPECT_EQ(sent_.size(), 1U);
    EXPECT_EQ(callback_count, 0U);
    EXPECT_EQ(schedule_count_, 0U);
    EXPECT_TRUE(queue.HandleReply(ordinary_reply));
    EXPECT_EQ(sent_.size(), 2U);
    EXPECT_EQ(callback_count, 1U);
    StopAndCancel(queue);
}

TEST_F(ClientSendQueueTest, StopDuringBlockingTransportCancelsFutureBeforeDescriptorRetirement)
{
    // Given a blocking request paused in transport behind explicit promises
    auto queue = MakeQueue();
    std::array<std::uint8_t, 1> message{1U};
    std::array<std::uint8_t, 1> reply{};
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future();
    send_action_ = [&entered, &released](auto, auto, auto) {
        entered.set_value();
        released.wait();
        return SendResult{score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))};
    };
    auto sending = std::async(std::launch::async, [&]() {
        return queue.SendWaitReply(message, reply);
    });
    entered.get_future().wait();

    // When the owner cancels the request before releasing its transport
    EXPECT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
    queue.CancelPending();
    EXPECT_FALSE(lifetime_.CancellationFinished());
    EXPECT_EQ(retirement_count_, 0U);
    release.set_value();
    const auto result = sending.get();

    // Then EPIPE completes the caller and only its final return permits retirement
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);
    EXPECT_EQ(retirement_count_, 1U);
    EXPECT_EQ(lifetime_.RetireDescriptor(), kDescriptor);
}

}  // namespace
}  // namespace score::message_passing::detail
