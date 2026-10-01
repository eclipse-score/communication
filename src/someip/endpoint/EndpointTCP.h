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

// -----------------------------------------------------------------------------
//  PacketRouter Endpoint TCP : Endpoint Types
// -----------------------------------------------------------------------------
template <typename BASETYPE> class EndpointTCPServer;

constexpr std::uint32_t kMaxNumRetry = 10;

// EndpointTCPClient
template <typename BASETYPE> class EndpointTCPClient : public Endpoint {
public:
    EndpointTCPClient(BASETYPE* host) {
        host_ = host;
    }
    virtual ~EndpointTCPClient() {}

    virtual void callback(bool close) override {
        if (close) {
            on_error();
            return;
        }

        std::uint32_t num_retry = 0;
        std::int32_t received_len = 0;
        std::uint32_t packet_length_field = 0;
        std::uint32_t rest_len = 0;
        std::vector<uint8_t> receive_buffer_dynamic;

        // Static allocatin for receiving the packet will be used to improve the performance
        // if the length of the packet is shorter than SOMEIP_UDP_MAX_PAYLOAD_SIZE (about 1400 bytes).
        tcp_receive_buffer_ = static_buffer_;

        std::uint32_t rest_header_len(SOMEIP_HEADER::ACCUM_LEN::LENGTH);
        do {
            // Receive SOMEIP Header (Message ID + Length field)
            received_len = socket_->receive((char*)tcp_receive_buffer_, rest_header_len);

            if (received_len > 0) {
                rest_header_len -= received_len;
                num_retry = 0;
            } else {
                if (is_packet_need_discard(received_len, num_retry)) {
                    return;
                } else {
                    // 100bytes(SOME/IP 46bytes) need 1usec to be sent in 100Mbps Ethernet.
                    // Regarding thread operation, 1usec x 1000 = 1ms
                    // Since 1ms is not enough to receive packets separated from SOME/IP lib (PacketRouterProxy),
                    // 1ms should be 10ms.
                    // In PacketRouterProxy, a SOME/IP packet is separated to SOME/IP header and payload.
                    ++num_retry;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        } while (rest_header_len > 0);

        // Length field shall contain the length in Byte starting from Request ID/Client ID
        // until the end of the SOME/IP message.
        get_byte_stream(&packet_length_field, tcp_receive_buffer_ + SOMEIP_HEADER::POS::LENGTH);
        rest_len = packet_length_field;

        if ((packet_length_field == 0) || (packet_length_field > MAX_TRANSFER_PACKET_LENGTH)) {
            // If the length field of the received SOME/IP packet is zero or larger than MAX_TRANSFER_PACKET_LENGTH, the
            // packet might crash. So, the rest of TCP receive buffer should be removed from the buffer to ignore the
            // packet and receive the next packet.
            LGSOMEIP_LOG_WARN << "EndpointTCPClient::callback / the length field is invalid!! Length = "
                              << packet_length_field;

            auto temp_buffer = std::make_unique<uint8_t[]>(MAX_TRANSFER_PACKET_LENGTH);

            received_len = socket_->receive((char*)temp_buffer.get(), MAX_TRANSFER_PACKET_LENGTH);

            print_byte_message("EndpointTCPClient::callback Header", tcp_receive_buffer_,
                               SOMEIP_HEADER::ACCUM_LEN::LENGTH);
            print_byte_message("EndpointTCPClient::callback Data", temp_buffer.get(), received_len);

            return;
        }

        // If the length of a packet is larger than SOMEIP_UDP_MAX_PAYLOAD_SIZE,
        // dynamic allocation for receiving the packet should be used.
        // The length of dynamic allocation will be packetLengthField + SOMEIP_HEADER::ACCUM_LEN::LENGTH + 2.
        // 2 bytes are for storing the instance ID.
        if (packet_length_field + SOMEIP_HEADER::ACCUM_LEN::LENGTH + 2 > SOMEIP_UDP_MAX_PAYLOAD_SIZE) {
            receive_buffer_dynamic.assign(tcp_receive_buffer_, tcp_receive_buffer_ + SOMEIP_HEADER::ACCUM_LEN::LENGTH);
            receive_buffer_dynamic.resize(packet_length_field + SOMEIP_HEADER::ACCUM_LEN::LENGTH + 2);
            tcp_receive_buffer_ = receive_buffer_dynamic.data();
        }

        LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / Endpoint TCP: " << rest_len << " will be read";

        num_retry = 0;
        do {
            // Receive SOMEIP Body (From Request ID to the end)
            received_len = socket_->receive(
                (char*)tcp_receive_buffer_ + SOMEIP_HEADER::POS::REQUESTID + packet_length_field - rest_len, rest_len);

            if (received_len > 0) {
                rest_len -= received_len;
                num_retry = 0;
                LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / Endpoint TCP: " << received_len << " read, "
                                   << rest_len << " remains";
            } else {
                if (is_packet_need_discard(received_len, num_retry)) {
                    return;
                } else {
                    // 100bytes(SOME/IP 46bytes) need 1usec to be sent in 100Mbps Ethernet.
                    // Regarding thread operation, 1usec x 1000 x 10 = 10ms
                    ++num_retry;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        } while (rest_len > 0);

        std::uint32_t total_message_len = SOMEIP_HEADER::ACCUM_LEN::LENGTH + packet_length_field;

        // Only if this is an internal socket (domain socket)
        if (get_socket()->get_dst_address()->get_type() == AF_UNIX) {
            set_instance_id(0);

            // Only if this is a SOME/IP packet not a SOME/IP-SD one
            if (!(tcp_receive_buffer_[0] == 0xff && tcp_receive_buffer_[1] == 0xff)) {
                std::uint8_t buf[2];
                std::uint16_t instance_id;

                // Extract Instance ID from SOME/IP packet
                std::uint8_t rest_len = 2;
                num_retry = 0;
                do {
                    received_len = socket_->receive((char*)buf, rest_len);

                    if (received_len > 0) {
                        rest_len -= received_len;
                        num_retry = 0;
                    } else {
                        if (is_packet_need_discard(received_len, num_retry)) {
                            return;
                        } else {
                            ++num_retry;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    }
                } while (rest_len > 0);

                get_byte_stream(&instance_id, buf);
                set_instance_id(instance_id);

                LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / this packet has instanceID = "
                                   << MSGID_FORMAT4(instance_id);
            }
        } else {
            if (is_magic_cookie(tcp_receive_buffer_, total_message_len)) {
                LGSOMEIP_LOG_DEBUG << "EndpointTCPClient::callback / Magic Cookie received from ["
                                   << get_socket()->get_dst_address()->get_ip_address() << "]";

                return;
            }
        }

        on_message(tcp_receive_buffer_, total_message_len);
    }

    inline bool is_packet_need_discard(std::int32_t read_len, std::uint8_t num_retry) {
        bool ret(false);

        //  1. len = 0 means ::read() reads 0 byte or EWOULDBLOCK occurs.
        //     If retry exceeds kMaxNumRetry, discard packet.
        //  2. len < 0 means ::read() returns with error. Discard packet immediately.
        if (read_len == 0) {
            if (num_retry > kMaxNumRetry) {
                LGSOMEIP_LOG_WARN << "EndpointTCPClient::callback / Stop retrying to receive";
                ret = true;
            }
        } else {
            LGSOMEIP_LOG_ERROR << "EndpointTCPClient::callback / Return due to socket error";
            ret = true;
        }

        return ret;
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
