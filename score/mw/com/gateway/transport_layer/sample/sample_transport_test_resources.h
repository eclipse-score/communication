#ifndef COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H
#define COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H

#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <memory>
#include <thread>

namespace score::mw::com::gateway
{

/// \brief Test attorney for BidirectionalTransport.
///
/// This class provides access to private properties of BidirectionalTransport for testing purposes.
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

    bool HasPendingRequest(std::uint32_t sequence) const
    {
        return transport_->pending_tracker_->HasPendingRequest(sequence);
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

    void EnqueueForDispatch(std::unique_ptr<TransportMessage> message)
    {
        {
            std::lock_guard<std::mutex> lock(transport_->dispatch_mutex_);
            transport_->dispatch_queue_.push(std::move(message));
        }
        transport_->dispatch_cv_.notify_one();
    }

    // Directly invokes the receive loop
    void RunReceiveUntilDisconnect()
    {
        score::cpp::stop_source stop_source;
        transport_->ReceiveUntilDisconnect(stop_source.get_token());
    }

  private:
    std::shared_ptr<BidirectionalTransport> transport_;
};

}  // namespace score::mw::com::gateway

#endif  // COMMUNICATION_SAMPLE_TRANSPORT_TEST_RESOURCES_H
