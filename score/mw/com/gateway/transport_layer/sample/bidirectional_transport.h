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

    /// \brief Initializes the send and receive sockets and starts the related threads.
    /// Blocks until the first connection is established.
    score::ResultBlank Setup() override;
    void Shutdown() override;

    bool IsConnected() const override;

    /// \brief Send a request message for a maximum of retries. Expects to receive ACK response for this message.
    score::ResultBlank SendRequest(TransportMessage& message) override;
    /// \brief Send a notification message without expected ACK response.
    score::ResultBlank SendNotification(TransportMessage& message) override;

    /// \brief Set a callback that will handle incoming messages. If not set incoming messages will be dropped.
    /// \param handler The callback to handle incoming messages.
    void SetMessageHandler(MessageHandler handler) override;

  private:
    struct PendingRequest
    {
        bool acknowledged{false};
    };

    /// \brief Send the given message and wait for the ACK response with the same sequence number.
    /// Returns error if unknown sequence number, timeout ocurres or disconnect happens.
    score::ResultBlank TrySendAndWaitForAck(TransportMessage& message, const std::uint32_t sequence);

    score::ResultBlank SetupSendSocket(score::cpp::stop_token stop_token);
    score::ResultBlank SetupListenSocket();

    /// \brief Handles in loop that sockets are setup and connected. Calls receiving function if connected.
    void ConnectionLoop(score::cpp::stop_token stop_token);
    /// \brief Listening for incoming connections. If successfully accepted, receive socket will be set up.
    bool WaitForConnection(score::cpp::stop_token stop_token);
    /// \brief Until stopped or disconnect happens, listens for incoming messages and triggers deserialization of
    /// message.
    void ReceiveUntilDisconnect(score::cpp::stop_token stop_token);
    void CleanupSocketsForReconnection();
    /// \brief Loop that waits for messages in the dispatch queue and calls the message handler callback for those
    /// messages.
    void DispatchLoop(const score::cpp::stop_token& stop_token);

    void HandleIncomingMessage(std::unique_ptr<TransportMessage> message);
    /// \brief Updates the acknowledgment status of pending requests based on the received response message.
    void HandleResponse(std::unique_ptr<TransportMessage> response);

    /// \brief Sends ACK for the given sequence number.
    score::ResultBlank SendAck(const std::uint32_t sequence);

    std::uint32_t GetNextSequenceNumber();

    /// \brief Serializes the given message and sends it on the send socket. Returns error if sending fails or
    /// disconnect happens.
    score::ResultBlank SendMessageOnSocket(const TransportMessage& message);
    /// \brief Reads from receiving socket and serializes the incoming message.
    /// \return The deserialized message or error if deserialization fails or a disconnect happens.
    std::unique_ptr<TransportMessage> ReceiveMessageFromSocket();

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

    static constexpr int kMaxSendRetries = 5U;
    static constexpr std::size_t kMaxPayloadSize = 1024U;

    std::array<std::uint8_t, kMaxPayloadSize> send_buffer_{};
    std::array<std::uint8_t, kMaxPayloadSize> receive_buffer_{};

    // Intentionally keeping at the end to ensure that it's destroyed first during shutdown.
    score::concurrency::LongRunningThreadsContainer threads_;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_BIDIRECTIONAL_TRANSPORT_H_
