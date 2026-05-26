#ifndef COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H
#define COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H

#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"

#include <chrono>
#include <memory>
#include <thread>

namespace score::mw::com::gateway
{

/// \brief Test attorney for BidirectionalTransport.
///
/// This class provides access to BidirectionalTransport for testing purposes.
class BidirectionalTransportAttorney
{
  public:
    explicit BidirectionalTransportAttorney(std::shared_ptr<BidirectionalTransport> transport) noexcept
        : transport_{transport}
    {
    }

    bool IsMessageHandlerSet() const noexcept
    {
        return transport_->has_message_handler_;
    }

    bool IsListenSocketValid() const noexcept
    {
        return transport_->listen_socket_.IsValid();
    }

    bool IsSendSocketValid() const noexcept
    {
        return transport_->send_socket_.IsValid();
    }

    bool IsReceiveSocketValid() const noexcept
    {
        return transport_->receive_socket_.IsValid();
    }

    bool HasPendingRequest(std::uint32_t sequence) const
    {
        std::lock_guard<std::mutex> lock(transport_->pending_mutex_);
        return transport_->pending_requests_.find(sequence) != transport_->pending_requests_.end();
    }

    bool WaitForPendingRequest(std::uint32_t sequence, std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (HasPendingRequest(sequence))
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return HasPendingRequest(sequence);
    }

    bool WaitForConnectedState(bool expected, std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (transport_->is_connected_.load() == expected)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return transport_->is_connected_.load() == expected;
    }

    void SetIsConnected(bool is_connected) noexcept
    {
        transport_->is_connected_ = is_connected;
    }

    void SetSendSocket(std::int32_t fd) noexcept
    {
        transport_->send_socket_.Reset(fd);
    }

    void SetReceiveSocket(std::int32_t fd) noexcept
    {
        transport_->receive_socket_.Reset(fd);
    }

    void SetListenSocket(std::int32_t fd) noexcept
    {
        transport_->listen_socket_.Reset(fd);
    }

    void SetRequestTimeoutMs(std::uint32_t timeout_ms) noexcept
    {
        transport_->socket_config_.request_timeout_ms_ = timeout_ms;
    }

    void SetDispatchShutdown(bool value) noexcept
    {
        transport_->dispatch_shutdown_ = value;
    }

    void EnqueueForDispatch(std::unique_ptr<TransportMessage> message)
    {
        {
            std::lock_guard<std::mutex> lock(transport_->dispatch_mutex_);
            transport_->dispatch_queue_.push(std::move(message));
        }
        transport_->dispatch_cv_.notify_one();
    }

    void LockPendingMutex()
    {
        transport_->pending_mutex_.lock();
    }

    void UnlockPendingMutex()
    {
        transport_->pending_mutex_.unlock();
    }

    void AcknowledgePendingRequest(std::uint32_t sequence)
    {
        std::lock_guard<std::mutex> lock(transport_->pending_mutex_);
        auto it = transport_->pending_requests_.find(sequence);
        if (it != transport_->pending_requests_.end())
        {
            it->second.acknowledged = true;
        }
        transport_->pending_cv_.notify_all();
    }

  private:
    std::shared_ptr<BidirectionalTransport> transport_;
};

}  // namespace score::mw::com::gateway

#endif  // COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H
