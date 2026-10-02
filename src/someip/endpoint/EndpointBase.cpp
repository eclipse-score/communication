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

#include <functional>
#include <typeinfo>

#include <endpoint/EndpointBase.h>
#include <message/MessageConstant.h>
#include <utils/byteorder/bytestream.h>

#include <utils/log/logger.h>

namespace lgsomeip {

EndpointBase::EndpointBase(std::shared_ptr<lgsomeip::osabstraction::Socket> socket) {
    set_socket(socket);
}

EndpointBase::~EndpointBase() {
    stop_listen();
}

void EndpointBase::set_socket(std::shared_ptr<lgsomeip::osabstraction::Socket> socket) {
    if (socket != nullptr) {
        socket_ = socket;
        if (socket_->get_reliable() == false) {
            auto type = socket_->get_address_type();

            if (type == lgsomeip::osabstraction::AddressType::IPv6) {
                sender_address_ = std::make_shared<lgsomeip::osabstraction::IP6Address>();
            } else if (type == lgsomeip::osabstraction::AddressType::IPv4) {
                sender_address_ = std::make_shared<lgsomeip::osabstraction::IP4Address>();
            }
        }
    }
}

std::shared_ptr<lgsomeip::osabstraction::Socket> EndpointBase::get_socket() const {
    return socket_;
}

std::shared_ptr<lgsomeip::osabstraction::Multiplexer> EndpointBase::get_multiplexer() const {
    return multiplexer_;
}

void EndpointBase::start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer) {
    if (multiplexer != nullptr) {
        multiplexer_ = multiplexer;
        auto callback = std::bind(&EndpointBase::callback, this, std::placeholders::_1);
        multiplexer_->set(socket_->get_socket_fd(), callback);
    } else {
        // TODO(lg-someip): Move listening to a dedicated thread when no multiplexer is available.
    }
}

void EndpointBase::stop_listen() {
    if (multiplexer_ != nullptr) {
        multiplexer_->unset(socket_->get_socket_fd());
        multiplexer_ = nullptr;
    } else {
        // TODO(lg-someip): Stop the dedicated listener thread when no multiplexer is available.
    }
}

void EndpointBase::send_message(const std::uint8_t* message, std::size_t size,
                                std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    if (address == nullptr) {
        socket_->send(socket_->get_socket_fd(), reinterpret_cast<const void*>(message), size);
    } else {
        socket_->send(reinterpret_cast<const void*>(message), size, address);
    }
}

void EndpointBase::callback(bool close) {
    LGSOMEIP_LOG_ERROR << "EndpointBase::callback / callback not expected!";
}

std::shared_ptr<lgsomeip::osabstraction::Address> EndpointBase::get_sender_address() {
    return sender_address_;
}

bool operator==(const EndpointBase& lhs, const EndpointBase& rhs) {
    if (lhs.get_socket() == nullptr || rhs.get_socket() == nullptr)
        return false;

    if (lhs.get_socket()->get_socket_fd() == rhs.get_socket()->get_socket_fd())
        return true;

    return false;
}

} // namespace lgsomeip
