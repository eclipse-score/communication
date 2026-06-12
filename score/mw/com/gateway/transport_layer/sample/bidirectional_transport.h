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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_BIDIRECTIONAL_TRANSPORT_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_BIDIRECTIONAL_TRANSPORT_H_

#include "score/concurrency/long_running_threads_container.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/hypervisor_socket_configuration.h"
#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/mw/com/gateway/transport_layer/sample/unique_socket.h"

#include <score/callback.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>

namespace score::mw::com::gateway
{

class BidirectionalTransport : public IBidirectionalTransport
{
    friend class BidirectionalTransportAttorney;  // For testing purposes only

  public:
    explicit BidirectionalTransport(HyperVisorSocketConfiguration socket_config) noexcept;
    ~BidirectionalTransport() override;

    BidirectionalTransport(const BidirectionalTransport&) = delete;
    BidirectionalTransport& operator=(const BidirectionalTransport&) = delete;
    BidirectionalTransport(BidirectionalTransport&&) = delete;
    BidirectionalTransport& operator=(BidirectionalTransport&&) = delete;

    score::ResultBlank Setup() override;
    void Shutdown() override;

    bool IsConnected() const override;

    score::ResultBlank SendRequest(TransportMessage& message) override;
    score::ResultBlank SendNotification(TransportMessage& message) override;

    void SetMessageHandler(MessageHandler handler) override;

  private:
    struct PendingRequest
    {
        bool acknowledged{false};
    };

    score::ResultBlank TrySendAndWaitForAck(TransportMessage& message, const std::uint32_t sequence);

    score::ResultBlank SetupSendSocket(score::cpp::stop_token stop_token);
    score::ResultBlank SetupListenSocket();

    void ConnectionLoop(score::cpp::stop_token stop_token);
    bool WaitForConnection(score::cpp::stop_token stop_token);
    void ReceiveUntilDisconnect(score::cpp::stop_token stop_token);
    void CleanupSocketsForReconnection();
    void DispatchLoop(score::cpp::stop_token stop_token);

    void HandleIncomingMessage(std::unique_ptr<TransportMessage> message);
    void HandleResponse(std::unique_ptr<TransportMessage> response);

    score::ResultBlank SendAck(const std::uint32_t sequence);

    std::uint32_t GetNextSequenceNumber();

    score::ResultBlank SendMessageOnSocket(const UniqueSocket& socket, const TransportMessage& message);
    std::unique_ptr<TransportMessage> ReceiveMessageFromSocket(const UniqueSocket& socket);

    HyperVisorSocketConfiguration socket_config_;

    UniqueSocket send_socket_;
    std::mutex send_mutex_;

    UniqueSocket listen_socket_;
    UniqueSocket receive_socket_;

    std::atomic<bool> is_connected_{false};

    MessageHandler message_handler_;
    bool has_message_handler_{false};

    // Dispatch queue: incoming non-ACK messages are pushed here by the receive loop and
    // processed by a dedicated dispatch thread. This decouples the receive loop from the
    // message handler, so the handler can call SendRequest() without blocking ACK reception.
    std::queue<std::unique_ptr<TransportMessage>> dispatch_queue_;
    std::mutex dispatch_mutex_;
    std::condition_variable dispatch_cv_;
    std::atomic<bool> dispatch_shutdown_{false};

    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::atomic<std::uint32_t> next_sequence_{1U};
    std::map<std::uint32_t, PendingRequest> pending_requests_;

    static constexpr int kMaxRetries = 5U;
    static constexpr std::size_t kMaxPayloadSize = 1024U;

    std::array<std::uint8_t, kMaxPayloadSize> send_buffer_{};
    std::array<std::uint8_t, kMaxPayloadSize> receive_buffer_{};

    // Intentionally keeping at the end to ensure that it's destroyed first during shutdown.
    score::concurrency::LongRunningThreadsContainer threads_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_BIDIRECTIONAL_TRANSPORT_H_
