#include "score/message_passing/client_send_queue.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/non_allocating_future/non_allocating_future.h"

#include <score/utility.hpp>

#include <cstring>
#include <utility>

namespace score::message_passing::detail
{

ClientSendQueue::ClientSendQueue(ClientConnectionLifetime& lifetime,
                                 score::cpp::pmr::memory_resource* memory_resource,
                                 const IClientFactory::ClientConfig& config,
                                 std::uint32_t max_send_size,
                                 SendActor send,
                                 ScheduleActor schedule_drain) noexcept
    : lifetime_{lifetime},
      config_{config},
      max_send_size_{max_send_size},
      send_{std::move(send)},
      schedule_drain_{std::move(schedule_drain)},
      reply_condition_{},
      storage_{static_cast<std::size_t>(config.max_queued_sends) + static_cast<std::size_t>(config.max_async_replies),
               score::cpp::pmr::polymorphic_allocator<>(memory_resource)},
      pool_{},
      queue_{},
      waiting_for_reply_{},
      reply_slot_owner_{nullptr}
{
    for (auto& command : storage_)
    {
        command.message.reserve(static_cast<std::size_t>(max_send_size_));
    }
    pool_.assign(storage_.begin(), storage_.end());
}

ClientSendQueue::~ClientSendQueue() noexcept
{
    pool_.clear();
}

bool ClientSendQueue::TryQueue(Message message, ReplyCallback callback) noexcept
{
    if (pool_.empty())
    {
        return false;
    }
    auto& command = pool_.front();
    pool_.pop_front();
    command.message.assign(message.begin(), message.end());
    command.callback = std::move(callback);
    queue_.push_back(command);
    ArmIfNeeded();
    return true;
}

ClientSendQueue::SendResult ClientSendQueue::Send(Message message) noexcept
{
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EMSGSIZE));
    }
    auto lock = lifetime_.Lock();
    ClientConnectionLifetime::Operation operation{lifetime_, lock};
    if (!operation)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    if (config_.truly_async || (config_.fully_ordered && waiting_for_reply_.has_value()))
    {
        if (!TryQueue(message, ReplyCallback{}))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOBUFS));
        }
        return {};
    }

    const ReplySlotToken marker{};
    if (config_.fully_ordered)
    {
        waiting_for_reply_ = ReplyCallback{};
        reply_slot_owner_ = &marker;
    }
    const auto result = SendTransport(operation, score::cpp::to_underlying(ClientToServer::SEND), message);
    if (reply_slot_owner_ == &marker)
    {
        waiting_for_reply_.reset();
        reply_slot_owner_ = nullptr;
        ArmIfNeeded();
    }
    return result;
}

ClientSendQueue::ReplyResult ClientSendQueue::SendWaitReply(Message message, ReplyBuffer reply) noexcept
{
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EMSGSIZE));
    }
    if (!lifetime_.IsAccepting())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }

    ReplyResult result{};
    const ReplySlotToken marker{};
    NonAllocatingFuture future(lifetime_.Mutex(), reply_condition_, result);
    ReplyCallback callback = [&reply, &future](ReplyResult message_expected) {
        if (!message_expected.has_value())
        {
            future.UpdateValueMarkReady(score::cpp::make_unexpected(message_expected.error()));
            return;
        }
        const auto reply_message = message_expected.value();
        if (reply_message.size() > reply.size())
        {
            future.UpdateValueMarkReady(score::cpp::make_unexpected(score::os::Error::createFromErrno(EMSGSIZE)));
            return;
        }
        // NOLINTBEGIN(bugprone-suspicious-stringview-data-usage) `reply`'s buffer size got checked above
        // NOLINTNEXTLINE(score-banned-function) copying byte buffer
        score::cpp::ignore = std::memcpy(reply.data(), reply_message.data(), reply_message.size());
        future.UpdateValueMarkReady(Message{reply.data(), reply_message.size()});
        // NOLINTEND(bugprone-suspicious-stringview-data-usage)
    };

    auto lock = lifetime_.Lock();
    ClientConnectionLifetime::Operation operation{lifetime_, lock};
    if (!operation)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    if (waiting_for_reply_.has_value())
    {
        // Suppress "AUTOSAR C++14 A5-1-4" rule finding: "A lambda expression object shall not outlive any of its
        // reference-captured objects.".
        // The reference-captured objects are not used after future.Wait().
        // coverity[autosar_cpp14_a5_1_4_violation]
        if (!TryQueue(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOBUFS));
        }
    }
    else
    {
        waiting_for_reply_ = std::move(callback);
        reply_slot_owner_ = &marker;
        const auto expected = SendTransport(operation, score::cpp::to_underlying(ClientToServer::REQUEST), message);
        if (!expected.has_value() && (reply_slot_owner_ == &marker))
        {
            waiting_for_reply_.reset();
            reply_slot_owner_ = nullptr;
            ArmIfNeeded();
            return score::cpp::make_unexpected(expected.error());
        }
    }
    lock.unlock();
    future.Wait();
    lock.lock();
    return result;
}

ClientSendQueue::SendResult ClientSendQueue::SendWithCallback(Message message, ReplyCallback callback) noexcept
{
    if (message.size() > max_send_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EMSGSIZE));
    }
    auto lock = lifetime_.Lock();
    ClientConnectionLifetime::Operation operation{lifetime_, lock};
    if (!operation)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
    }
    if (config_.truly_async || waiting_for_reply_.has_value())
    {
        if (!TryQueue(message, std::move(callback)))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOBUFS));
        }
        return {};
    }

    const ReplySlotToken marker{};
    waiting_for_reply_ = std::move(callback);
    reply_slot_owner_ = &marker;
    const auto expected = SendTransport(operation, score::cpp::to_underlying(ClientToServer::REQUEST), message);
    const bool owns_reply = reply_slot_owner_ == &marker;
    if (owns_reply)
    {
        reply_slot_owner_ = nullptr;
        if (!expected.has_value())
        {
            waiting_for_reply_.reset();
            ArmIfNeeded();
        }
    }
    if (!owns_reply)
    {
        return {};
    }
    return expected;
}

bool ClientSendQueue::HandleReply(Message message) noexcept
{
    auto lock = lifetime_.Lock();
    if (!waiting_for_reply_.has_value() || waiting_for_reply_->empty())
    {
        return false;
    }
    auto callback = std::move(*waiting_for_reply_);
    waiting_for_reply_.reset();
    reply_slot_owner_ = nullptr;
    DrainUnderLock(lock);
    lock.unlock();
    callback(message);
    return true;
}

void ClientSendQueue::ArmIfNeeded() noexcept
{
    if (queue_.empty() || !lifetime_.IsAccepting() || waiting_for_reply_.has_value())
    {
        return;
    }
    waiting_for_reply_ = ReplyCallback{};
    schedule_drain_();
}

void ClientSendQueue::Drain() noexcept
{
    auto lock = lifetime_.Lock();
    DrainUnderLock(lock);
}

void ClientSendQueue::DrainUnderLock(std::unique_lock<std::mutex>& lock) noexcept
{
    ClientConnectionLifetime::Operation operation{lifetime_, lock};
    if (!operation)
    {
        return;
    }
    while (!queue_.empty() && lifetime_.IsAccepting())
    {
        SendCommand& command = queue_.front();
        queue_.pop_front();
        const ReplySlotToken marker{};
        auto callback = std::move(command.callback);
        if (!callback.empty())
        {
            waiting_for_reply_ = std::move(callback);
            reply_slot_owner_ = &marker;
            const auto expected =
                SendTransport(operation, score::cpp::to_underlying(ClientToServer::REQUEST), command.message);
            pool_.push_front(command);
            if (expected.has_value())
            {
                if (reply_slot_owner_ == &marker)
                {
                    reply_slot_owner_ = nullptr;
                }
                break;
            }
            if (reply_slot_owner_ != &marker)
            {
                continue;
            }
            callback = std::move(*waiting_for_reply_);
            lock.unlock();
            if (!callback.empty())
            {
                callback(score::cpp::make_unexpected(expected.error()));
            }
            lock.lock();
            if (reply_slot_owner_ == &marker)
            {
                waiting_for_reply_.reset();
                reply_slot_owner_ = nullptr;
            }
        }
        else
        {
            waiting_for_reply_ = ReplyCallback{};
            reply_slot_owner_ = &marker;
            score::cpp::ignore =
                SendTransport(operation, score::cpp::to_underlying(ClientToServer::SEND), command.message);
            pool_.push_front(command);
            if (reply_slot_owner_ == &marker)
            {
                waiting_for_reply_.reset();
                reply_slot_owner_ = nullptr;
            }
        }
    }
}

ClientSendQueue::SendResult ClientSendQueue::SendTransport(ClientConnectionLifetime::Operation& operation,
                                                           std::uint8_t code,
                                                           Message message) noexcept
{
    return operation.Execute([this, code, message](std::int32_t descriptor) noexcept {
        return send_(descriptor, code, message);
    });
}

void ClientSendQueue::CancelPending() noexcept
{
    auto lock = lifetime_.Lock();
    if (waiting_for_reply_.has_value())
    {
        auto callback = std::move(*waiting_for_reply_);
        waiting_for_reply_.reset();
        reply_slot_owner_ = nullptr;
        lock.unlock();
        if (!callback.empty())
        {
            callback(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
        }
        lock.lock();
    }
    while (!queue_.empty())
    {
        SendCommand& command = queue_.front();
        auto callback = std::move(command.callback);
        queue_.pop_front();
        pool_.push_front(command);
        if (!callback.empty())
        {
            lock.unlock();
            callback(score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE)));
            lock.lock();
        }
    }
}

}  // namespace score::message_passing::detail
