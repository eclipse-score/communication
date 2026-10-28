/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINTTCP_H
#define LG_SOMEIP_ENDPOINT_ENDPOINTTCP_H

#include <cstdint>
#include <poll.h>
#include <sys/socket.h>

#include <algorithm>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>

#include "endpoint/EndpointBase.h"
#include "message/MessageConstant.h"
#include "packetrouter/PacketRouterConstant.h"
#include "utils/byteorder/bytestream.h"
#include "utils/log/formatLog.h"
#include "utils/log/logger.h"

#include "socket/LocalAddress.h"
#include "socket/TCPClientSocket.h"
#include "socket/TCPServerSocket.h"

namespace lgsomeip {

// PacketRouter TCP endpoint types.
template <typename BASETYPE> class EndpointTCPServer;

// How long a reader waits for the rest of a message that has partly arrived before it returns to the multiplexer.
constexpr int kPartialMessageWaitMs = 1;

// EndpointTCPClient
template <typename BASETYPE> class EndpointTCPClient : public Endpoint {
public:
    EndpointTCPClient(BASETYPE* host) {
        host_ = host;
    }
    virtual ~EndpointTCPClient() {}

    // Reads the next SOME/IP message from the stream. A message that has only partly arrived stays in the
    // receive state, and the next callback continues where this one stopped. Each read asks for exactly the
    // bytes still missing from the current part, so the reader never consumes bytes of the following message.
    virtual void callback(bool close) override {
        if (close) {
            reset_receive_state();
            on_error();
            return;
        }

        bool received_any = false;
        bool waited_for_data = false;

        for (;;) {
            const std::size_t part_size = receive_part_size();
            const std::size_t missing_len = part_size - received_part_len_;

            std::int32_t received_len =
                socket_->receive(reinterpret_cast<char*>(receive_part_buffer()) + received_part_len_, missing_len);

            if (received_len < 0) {
                LGSOMEIP_LOG_ERROR << "EndpointTCPClient::callback / Return due to socket error";
                reset_receive_state();
                return;
            }

            if (received_len == 0) {
                // The rest of a message that has started usually follows within microseconds. Waiting briefly for
                // it saves a round trip through the multiplexer. If it does not come, the state is kept for the
                // next callback.
                if (!waited_for_data && is_message_in_progress() && wait_for_more_data()) {
                    waited_for_data = true;
                    continue;
                }

                // The socket is non-blocking, so zero means no data yet or end of stream.
                // A stream socket that is readable without data has been closed by the peer.
                if ((!received_any || waited_for_data) && is_peer_closed()) {
                    LGSOMEIP_LOG_INFO << "EndpointTCPClient::callback / closed by the peer";
                    reset_receive_state();
                    on_error();
                }
                return;
            }

            received_any = true;
            waited_for_data = false;
            received_part_len_ += static_cast<std::size_t>(received_len);

            // Keep reading until the part is complete. A read that returns no data ends this callback.
            if (received_part_len_ == part_size && !on_part_received()) {
                return;
            }
        }
    }

    // Drops a partly received message, for example when the connection is closed.
    void reset_receive_state() {
        receive_part_ = ReceivePart::kHeader;
        received_part_len_ = 0;
        packet_length_field_ = 0;
        receive_buffer_dynamic_.clear();
        receive_buffer_dynamic_.shrink_to_fit();
        tcp_receive_buffer_ = static_buffer_;
    }

    virtual void on_message(std::uint8_t* message, std::size_t message_length) override {
        host_->on_message(shared_from_this(), message, message_length);
    }

    virtual void on_error() override {
        if (shared_from_this() != nullptr) {
            host_->on_disconnect(shared_from_this());
        }
    }

    void set_server_endpoint(std::shared_ptr<Endpoint> endpoint) {
        server_endpoint_ = std::dynamic_pointer_cast<EndpointTCPServer<BASETYPE>>(endpoint);
    }

    std::shared_ptr<EndpointTCPServer<BASETYPE>> get_server_endpoint() {
        return server_endpoint_;
    }

    void send_magic_cookie(bool is_service_provider) {
        const std::uint8_t* cookie_payload = nullptr;

        if (is_service_provider) {
            cookie_payload = kServerMagicCookie;
        } else {
            cookie_payload = kClientMagicCookie;
        }

        send_message(cookie_payload, kLenMagicCookie);
    }

    bool is_magic_cookie(std::uint8_t* message, std::size_t message_length) const {
        bool res = false;
        std::uint32_t service_id_len = 2;

        // Check Service ID to see if it is 0xFFFF.
        // TCP socket is not supposed to receive SD message,
        // so it is enough to check service ID first.
        if ((message[0] == 0xff) && (message[1] == 0xff)) {
            if (memcmp(kServerMagicCookie + service_id_len, message + service_id_len,
                       kLenMagicCookie - service_id_len) == 0) {
                res = true;
            } else if (memcmp(kClientMagicCookie + service_id_len, message + service_id_len,
                              kLenMagicCookie - service_id_len) == 0) {
                res = true;
            }
        }

        return res;
    }

    inline std::chrono::system_clock::time_point const get_last_cookie_sent_time() {
        return last_cookie_sent_;
    }

    inline void set_last_cookie_sent_time(std::chrono::system_clock::time_point time) {
        last_cookie_sent_ = time;
    }

private:
    // The parts of a stream message, in the order in which they arrive.
    enum class ReceivePart {
        kHeader,     // Message ID and length field.
        kBody,       // Request ID up to the end of the message; its size is the length field.
        kInstanceId, // Instance ID that follows a message on internal domain sockets.
    };

    std::size_t receive_part_size() const {
        switch (receive_part_) {
        case ReceivePart::kHeader:
            return SOMEIP_HEADER::ACCUM_LEN::LENGTH;
        case ReceivePart::kBody:
            return packet_length_field_;
        default:
            return kLenInstanceId;
        }
    }

    std::uint8_t* receive_part_buffer() {
        switch (receive_part_) {
        case ReceivePart::kHeader:
            return tcp_receive_buffer_;
        case ReceivePart::kBody:
            return tcp_receive_buffer_ + SOMEIP_HEADER::ACCUM_LEN::LENGTH;
        default:
            return instance_id_buffer_;
        }
    }

    // Handles a completely received part. Returns true when the next part should be read now.
    bool on_part_received() {
        switch (receive_part_) {
        case ReceivePart::kHeader:
            return on_header_received();
        case ReceivePart::kBody:
            return on_body_received();
        default: {
            std::uint16_t instance_id;
            get_byte_stream(&instance_id, instance_id_buffer_);
            set_instance_id(instance_id);

            LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / this packet has instanceID = "
                               << MSGID_FORMAT4(instance_id);

            deliver_message();
            return false;
        }
        }
    }

    bool on_header_received() {
        // The length field covers the bytes from the request ID/client ID to the end of the SOME/IP message.
        get_byte_stream(&packet_length_field_, tcp_receive_buffer_ + SOMEIP_HEADER::POS::LENGTH);

        // The length field counts at least the 8 header bytes behind it (request ID to return code).
        constexpr std::uint32_t min_length_field = SOMEIP_HEADER::SIZE - SOMEIP_HEADER::ACCUM_LEN::LENGTH;

        if ((packet_length_field_ < min_length_field) || (packet_length_field_ > MAX_TRANSFER_PACKET_LENGTH)) {
            // TCP has no message boundaries, so the next message cannot be located once the length is wrong.
            // Close the connection instead of parsing the rest of the stream as headers.
            LGSOMEIP_LOG_WARN << "EndpointTCPClient::callback / the length field is invalid!! Length = "
                              << packet_length_field_ << "; closing the connection";

            print_byte_message("EndpointTCPClient::callback Header", tcp_receive_buffer_,
                               SOMEIP_HEADER::ACCUM_LEN::LENGTH);

            reset_receive_state();
            on_error();
            return false;
        }

        // Use dynamic allocation for messages that do not fit the static buffer.
        // Reserve additional bytes for the instance ID.
        if (packet_length_field_ + SOMEIP_HEADER::ACCUM_LEN::LENGTH + kLenInstanceId > sizeof(static_buffer_)) {
            receive_buffer_dynamic_.assign(tcp_receive_buffer_, tcp_receive_buffer_ + SOMEIP_HEADER::ACCUM_LEN::LENGTH);
            receive_buffer_dynamic_.resize(packet_length_field_ + SOMEIP_HEADER::ACCUM_LEN::LENGTH + kLenInstanceId);
            tcp_receive_buffer_ = receive_buffer_dynamic_.data();
        }

        LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / Endpoint TCP: " << packet_length_field_ << " will be read";

        receive_part_ = ReceivePart::kBody;
        received_part_len_ = 0;
        return true;
    }

    bool on_body_received() {
        const std::uint32_t total_message_len = SOMEIP_HEADER::ACCUM_LEN::LENGTH + packet_length_field_;

        // Internal domain sockets carry an additional instance ID.
        if (get_socket()->get_dst_address()->get_type() == AF_UNIX) {
            set_instance_id(0);

            // SOME/IP-SD packets do not carry the additional instance ID.
            if (!(tcp_receive_buffer_[0] == 0xff && tcp_receive_buffer_[1] == 0xff)) {
                receive_part_ = ReceivePart::kInstanceId;
                received_part_len_ = 0;
                return true;
            }
        } else if (is_magic_cookie(tcp_receive_buffer_, total_message_len)) {
            LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / Magic Cookie received from ["
                               << get_socket()->get_dst_address()->get_ip_address() << "]";

            reset_receive_state();
            return false;
        }

        deliver_message();
        return false;
    }

    // Hands the complete message to the host. The state is cleared first because the host may close this endpoint.
    void deliver_message() {
        std::uint8_t* message = tcp_receive_buffer_;
        const std::size_t message_length = SOMEIP_HEADER::ACCUM_LEN::LENGTH + packet_length_field_;
        std::vector<std::uint8_t> keep_alive = std::move(receive_buffer_dynamic_);

        reset_receive_state();

        on_message(message, message_length);
    }

    // The socket is non-blocking, so peeking one byte reports end of stream (0) without waiting.
    bool is_peer_closed() const {
        char byte;
        return ::recv(socket_->get_socket_fd(), &byte, 1, MSG_PEEK) == 0;
    }

    bool is_message_in_progress() const {
        return receive_part_ != ReceivePart::kHeader || received_part_len_ > 0;
    }

    // Waits up to kPartialMessageWaitMs until the socket is readable. Returns true if it is.
    bool wait_for_more_data() const {
        struct pollfd pfd;
        pfd.fd = socket_->get_socket_fd();
        pfd.events = POLLIN;
        pfd.revents = 0;
        return ::poll(&pfd, 1, kPartialMessageWaitMs) > 0;
    }

    ReceivePart receive_part_{ReceivePart::kHeader};
    std::size_t received_part_len_{0};
    std::uint32_t packet_length_field_{0};
    std::vector<std::uint8_t> receive_buffer_dynamic_;
    std::uint8_t instance_id_buffer_[kLenInstanceId] = {};

    std::chrono::system_clock::time_point last_cookie_sent_{std::chrono::system_clock::now()};
    BASETYPE* host_;
    std::shared_ptr<EndpointTCPServer<BASETYPE>> server_endpoint_{nullptr};
};

// EndpointTCPServer
template <typename BASETYPE> class EndpointTCPServer : public Endpoint {
public:
    EndpointTCPServer(BASETYPE* host) {
        host_ = host;
    }

    virtual ~EndpointTCPServer() {
        client_list_.clear();
    }

    virtual void callback(bool close) override {
        LGSOMEIP_LOG_DEBUG << "EndpointTCPServer::callback / SocketFD = " << this->socket_->get_socket_fd();

        if (close) {
            on_error();
            return;
        }

        if (typeid(*socket_) == typeid(lgsomeip::osabstraction::TCPServerSocket)) {
            on_connect();
        } else {
            LGSOMEIP_LOG_ERROR << "EndpointTCPServer::callback / Unexpected socket type";
        }
    }

    virtual void on_error() override {
        LGSOMEIP_LOG_ERROR << "EndpointTCPServer::callback / on_error";
    }

    void on_connect() {
        std::shared_ptr<lgsomeip::osabstraction::Socket> client_socket =
            std::static_pointer_cast<lgsomeip::osabstraction::TCPServerSocket>(get_socket())->accept();
        auto client_endpoint = std::make_shared<EndpointTCPClient<BASETYPE>>(host_);
        client_endpoint->set_socket(client_socket);
        client_endpoint->set_server_endpoint(shared_from_this());
        if (get_magic_cookie_enabled() == true) {
            client_endpoint->set_magic_cookie_enabled(true);
        }

        add_client_endpoint(client_endpoint->get_socket()->get_socket_fd());
        host_->on_connect(shared_from_this(), client_endpoint);
    }

    void add_client_endpoint(std::int32_t file_descriptor) {
        client_list_.push_back(file_descriptor);
    }

    void remove_client_endpoint(std::int32_t file_descriptor) {
        for (auto iter = client_list_.begin(); iter != client_list_.end(); ++iter) {
            if ((*iter) == file_descriptor) {
                client_list_.erase(iter);
                break;
            }
        }
    }

    std::vector<std::int32_t>& get_client_list() {
        return client_list_;
    }

private:
    BASETYPE* host_;
    std::vector<std::int32_t> client_list_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_ENDPOINT_ENDPOINTTCP_H
