#ifndef SCORE_LIB_MESSAGE_PASSING_CLIENT_SEND_QUEUE_H
#define SCORE_LIB_MESSAGE_PASSING_CLIENT_SEND_QUEUE_H

#include "score/containers/intrusive_list.h"
#include "score/message_passing/client_connection_lifetime.h"
#include "score/message_passing/i_client_factory.h"

#include <score/vector.hpp>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>

namespace score::message_passing::detail
{

class ClientSendQueue final
{
  public:
    using Message = score::cpp::span<const std::uint8_t>;
    using ReplyBuffer = score::cpp::span<std::uint8_t>;
    using ReplyResult = score::cpp::expected<Message, score::os::Error>;
    using SendResult = score::cpp::expected_blank<score::os::Error>;
    using ReplyCallback = IClientConnection::ReplyCallback;
    using SendActor = score::cpp::callback<SendResult(std::int32_t, std::uint8_t, Message)>;
    using ScheduleActor = score::cpp::callback<void()>;

    ClientSendQueue(ClientConnectionLifetime& lifetime,
                    score::cpp::pmr::memory_resource* memory_resource,
                    const IClientFactory::ClientConfig& config,
                    std::uint32_t max_send_size,
                    SendActor send,
                    ScheduleActor schedule_drain) noexcept;
    ~ClientSendQueue() noexcept;

    SendResult Send(Message message) noexcept;
    SendResult SendWithCallback(Message message, ReplyCallback callback) noexcept;
    ReplyResult SendWaitReply(Message message, ReplyBuffer reply) noexcept;
    bool HandleReply(Message message) noexcept;
    void Drain() noexcept;
    void CancelPending() noexcept;

  private:
    struct ReplySlotToken
    {
    };

    class SendCommand : public score::containers::intrusive_list_element<>
    {
      public:
        using allocator_type = score::cpp::pmr::polymorphic_allocator<SendCommand>;
        explicit SendCommand(const allocator_type& allocator)
            : score::containers::intrusive_list_element<>{}, message(allocator), callback{}
        {
        }

        score::cpp::pmr::vector<std::uint8_t> message;
        ReplyCallback callback;
    };

    bool TryQueue(Message message, ReplyCallback callback) noexcept;
    void ArmIfNeeded() noexcept;
    void DrainUnderLock(std::unique_lock<std::mutex>& lock) noexcept;
    SendResult SendTransport(ClientConnectionLifetime::Operation& operation,
                             std::uint8_t code,
                             Message message) noexcept;

    ClientConnectionLifetime& lifetime_;
    const IClientFactory::ClientConfig config_;
    const std::uint32_t max_send_size_;
    SendActor send_;
    ScheduleActor schedule_drain_;
    std::condition_variable reply_condition_;
    score::cpp::pmr::vector<SendCommand> storage_;
    score::containers::intrusive_list<SendCommand> pool_;
    score::containers::intrusive_list<SendCommand> queue_;
    std::optional<ReplyCallback> waiting_for_reply_;
    const ReplySlotToken* reply_slot_owner_;
};

}  // namespace score::message_passing::detail

#endif
