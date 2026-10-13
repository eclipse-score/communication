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

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINTUDP_H
#define LG_SOMEIP_ENDPOINT_ENDPOINTUDP_H

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

#include "socket/LocalAddress.h"
#include "socket/UDPSocket.h"

namespace lgsomeip {

// PacketRouter UDP endpoint types.
template <typename BASETYPE> class EndpointUDP : public Endpoint {
public:
    EndpointUDP(BASETYPE* host) {
        host_ = host;
    }

    virtual void on_message(std::uint8_t* message, std::size_t message_length) override {
        host_->on_message(shared_from_this(), message, message_length);
    }

    virtual void callback(bool close) override {
        LGSOMEIP_LOG_DEBUG << "EndpointUDP::callback / SocketFD = " << this->socket_->get_socket_fd();

        if (close) {
            on_error();
            return;
        }

        std::int32_t received_len = 0;

        // Receive into a buffer one byte larger than any valid datagram so truncation is detectable.
        received_len = socket_->receive((char*)udp_receive_buffer_, sizeof(udp_receive_buffer_), sender_address_);

        LGSOMEIP_LOG_DEBUG << "EndpointUDP::callback / received length = " << received_len;

        if (received_len <= 0) {
#if defined(ENABLE_TLS)
            if (socket_->is_secure_connection()) {
                LGSOMEIP_LOG_DEBUG << "EndpointUDP::callback / DTLS connection status changing";
                return;
            } else {
                on_error();
            }
#else  // ENABLE_TLS
            on_error();
#endif // ENABLE_TLS
        } else if (static_cast<std::size_t>(received_len) >= sizeof(udp_receive_buffer_)) {
            LGSOMEIP_LOG_WARN << "EndpointUDP::callback / discard datagram that fills the receive buffer";
        } else if (received_len < static_cast<std::int32_t>(SOMEIP_HEADER::SIZE)) {
            LGSOMEIP_LOG_WARN << "EndpointUDP::callback / discard truncated SOME/IP header";
        } else {
            const std::size_t total_recv_len = static_cast<std::size_t>(received_len);
            std::size_t index = 0;
            static constexpr std::size_t rest_hdr_len_from_length =
                SOMEIP_HEADER::SIZE - SOMEIP_HEADER::ACCUM_LEN::LENGTH;

            while (index < total_recv_len) {
                const std::size_t remaining = total_recv_len - index;
                if (remaining < SOMEIP_HEADER::SIZE) {
                    LGSOMEIP_LOG_WARN << "EndpointUDP::callback / discard trailing truncated SOME/IP header";
                    break;
                }

                std::uint32_t packet_length_field = 0;
                get_byte_stream(&packet_length_field, udp_receive_buffer_ + index + SOMEIP_HEADER::POS::LENGTH);
                if (packet_length_field < rest_hdr_len_from_length ||
                    packet_length_field > remaining - rest_hdr_len_from_length) {
                    LGSOMEIP_LOG_WARN << "EndpointUDP::callback / discard invalid SOME/IP length field";
                    break;
                }

                const std::size_t message_length =
                    static_cast<std::size_t>(packet_length_field) + rest_hdr_len_from_length;
                if (message_length - SOMEIP_HEADER::SIZE > SOMEIP_UDP::MAX_PAYLOAD_SIZE) {
                    LGSOMEIP_LOG_WARN << "EndpointUDP::callback / discard SOME/IP payload larger than "
                                      << SOMEIP_UDP::MAX_PAYLOAD_SIZE << " bytes";
                    break;
                }

                std::vector<std::uint8_t> message_buffer(message_length + 2);
                std::copy(udp_receive_buffer_ + index, udp_receive_buffer_ + index + message_length,
                          message_buffer.begin());
                on_message(message_buffer.data(), message_length);
                index += message_length;
            }
        }
    }

    virtual void on_error() override {
        LGSOMEIP_LOG_ERROR << "EndpointUDP::callback / on_error";
    }

private:
    BASETYPE* host_;
    std::uint8_t udp_receive_buffer_[SOMEIP_UDP::RECEIVE_BUFFER_SIZE];
};

} // namespace lgsomeip

#endif // LG_SOMEIP_ENDPOINT_ENDPOINTUDP_H
