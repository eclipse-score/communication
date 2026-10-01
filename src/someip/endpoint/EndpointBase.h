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

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINTBASE_H
#define LG_SOMEIP_ENDPOINT_ENDPOINTBASE_H

#include <cstdint>

#include <functional>
#include <map>
#include <memory>

#include <multiplex/Multiplexer.h>
#include <socket/Socket.h>

namespace lgsomeip {

class EndpointBase : public std::enable_shared_from_this<EndpointBase> {
public:
    const static std::uint32_t MAX_TRANSFER_PACKET_LENGTH =
        524288 + 1000; // To support 512Kbytes including header (about 1000bytes)

    EndpointBase(std::shared_ptr<lgsomeip::osabstraction::Socket> socket = nullptr);
    virtual ~EndpointBase();

    void set_socket(std::shared_ptr<lgsomeip::osabstraction::Socket> socket);
    std::shared_ptr<lgsomeip::osabstraction::Socket> get_socket() const;
    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> get_multiplexer() const;

    virtual void start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer = nullptr);
    virtual void stop_listen();

    virtual void send_message(const std::uint8_t* message, std::size_t size,
                              std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr);

    // Callback Table : must be implemented in child class
    virtual void on_message(std::uint8_t* message, std::size_t message_length) {}
    virtual void on_error() {}

    virtual std::shared_ptr<lgsomeip::osabstraction::Address> get_sender_address();

    // Operator Overloading
    friend bool operator==(const EndpointBase& lhs, const EndpointBase& rhs);

    void set_instance_id(std::uint16_t instance_id) {
        current_received_someip_packet_instance_id_ = instance_id;
    }
    std::uint16_t get_instance_id(void) {
        return current_received_someip_packet_instance_id_;
    }

    void increase_reference_count(void) {
        ++reference_count_;
    }
    void decrease_reference_count(void) {
        if (--reference_count_ < 0)
            reference_count_ = 0;
    }
    std::uint8_t get_reference_count(void) {
        return reference_count_;
    }

    void set_app_id(std::uint16_t app_id) {
        app_id_ = app_id;
    }
    std::uint16_t get_app_id() {
        return app_id_;
    }

    void set_magic_cookie_enabled(bool flag) {
        magic_cookie_enabled_ = flag;
    }
    inline bool get_magic_cookie_enabled() const {
        return magic_cookie_enabled_;
    }

protected:
    virtual void callback(bool close = false);

protected:
    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer_{nullptr};
    std::shared_ptr<lgsomeip::osabstraction::Socket> socket_{nullptr};

    std::shared_ptr<lgsomeip::osabstraction::Address> sender_address_{nullptr};

    uint8_t static_buffer_[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    uint8_t* udp_receive_buffer_ = static_buffer_;
    uint8_t* tcp_receive_buffer_ = static_buffer_;

    std::uint16_t current_received_someip_packet_instance_id_ = 0;

    std::int16_t reference_count_ = 0;

    std::uint16_t app_id_ = 0;

    bool magic_cookie_enabled_ = false;
};

using Endpoint = EndpointBase;

} // namespace lgsomeip

#endif // LG_SOMEIP_ENDPOINT_ENDPOINTBASE_H
