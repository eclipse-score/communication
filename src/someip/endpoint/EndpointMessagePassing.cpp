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

#include <utils/byteorder/bytestream.h>
#include <utils/log/logger.h>

#include <endpoint/EndpointMessagePassing.h>

namespace lgsomeip {

// EndpointMessagePassingServer.
EndpointMessagePassingServer::EndpointMessagePassingServer(EndPointMessagePassingListener* listener,
                                                           const std::string& name)
    : listener_(listener), message_passing_server_(name) {}

EndpointMessagePassingServer::~EndpointMessagePassingServer() {}

void EndpointMessagePassingServer::start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer) {
    message_passing_server_.run(this);
}

void EndpointMessagePassingServer::stop_listen() {
    message_passing_server_.stop();
}

void EndpointMessagePassingServer::send_message(const std::uint8_t* message, std::size_t size,
                                                std::shared_ptr<lgsomeip::osabstraction::Address> address) {}

void EndpointMessagePassingServer::on_connect(
    const lgsomeip::osabstraction::MessagePassingConnectionInformation& info) {
    LGSOMEIP_LOG_DEBUG << "EndpointMessagePassingServer::onConnect info=" << (int)info.scoid_;
    auto itor = receivers_.find(info.scoid_);
    if (itor == receivers_.end()) {
        std::shared_ptr<EndpointMessagePassingReceiver> endpoint_receiver =
            std::make_shared<EndpointMessagePassingReceiver>(listener_);

        endpoint_receiver->set_connection_information(&message_passing_server_, info);

        receivers_.insert({info.scoid_, endpoint_receiver});

        listener_->on_message_passing_connect(shared_from_this(), endpoint_receiver);
    }
}

void EndpointMessagePassingServer::on_disconnect(
    const lgsomeip::osabstraction::MessagePassingConnectionInformation& info) {
    LGSOMEIP_LOG_DEBUG << "EndpointMessagePassingServer::onDisconnect info=" << (int)info.scoid_;
    auto itor = receivers_.find(info.scoid_);
    if (itor != receivers_.end()) {
        message_passing_server_.set_data_listener(info, nullptr);

        listener_->on_message_passing_disconnect(itor->second);

        receivers_.erase(itor);
    }
}

std::shared_ptr<lgsomeip::osabstraction::Address> EndpointMessagePassingServer::get_sender_address() {
    return nullptr;
}

void EndpointMessagePassingServer::set_timer(const std::int32_t id, const std::uint32_t interval_milliseconds,
                                             const bool periodic) {
    message_passing_server_.set_timer(id, interval_milliseconds, periodic, this);
}

void EndpointMessagePassingServer::kill_timer(const std::int32_t id) {
    message_passing_server_.kill_timer(id);
}

void EndpointMessagePassingServer::on_timer(const std::int32_t id) {
    listener_->on_message_passing_timer(id);
}

// EndpointMessagePassingReceiver.
EndpointMessagePassingReceiver::EndpointMessagePassingReceiver(EndPointMessagePassingListener* listener)
    : listener_(listener), message_passing_server_(nullptr) {}

EndpointMessagePassingReceiver::~EndpointMessagePassingReceiver() {
    stop_listen();
}

void EndpointMessagePassingReceiver::start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer) {
    if (message_passing_server_ != nullptr) {
        message_passing_server_->set_data_listener(connection_information_, this);
    }
}

void EndpointMessagePassingReceiver::stop_listen() {
    if (message_passing_server_ != nullptr) {
        message_passing_server_->set_data_listener(connection_information_, nullptr);
    }
}

void EndpointMessagePassingReceiver::send_message(const std::uint8_t* message, std::size_t size,
                                                  std::shared_ptr<lgsomeip::osabstraction::Address> address) {}

void EndpointMessagePassingReceiver::on_message(std::uint8_t* data, std::size_t size) {
    std::uint32_t payload_length;
    std::uint16_t instance_id = 0;

    std::size_t size_need = 8;

    if (size_need < size) {
        get_byte_stream(&payload_length, data + 4);
        size_need += payload_length;

        if ((data[0] != 0xff) && (data[1] != 0xff)) {
            size_need += 2;

            if (size_need == size) {
                get_byte_stream(&instance_id, data + 8 + payload_length);
            }
        }

        if (size_need != size) {
            LGSOMEIP_LOG_WARN << "EndpointMessagePassingReceiver::on_message / " << "wrong size : " << size
                              << "(receivedLength) not equal : " << size_need << "(expectedLength)";

            return;
        }

        set_instance_id(instance_id);

        listener_->on_message_passing_message(shared_from_this(), data, 8 + payload_length);
    } else {
        LGSOMEIP_LOG_WARN << "EndpointMessagePassingReceiver::on_message / "
                          << "wrong size less than default header size(8)";
    }
}

void EndpointMessagePassingReceiver::on_error() {
    listener_->on_message_passing_disconnect(shared_from_this());
}

std::shared_ptr<lgsomeip::osabstraction::Address> EndpointMessagePassingReceiver::get_sender_address() {
    return nullptr;
}

void EndpointMessagePassingReceiver::set_connection_information(
    lgsomeip::osabstraction::MessagePassingServer* message_passing_server,
    const lgsomeip::osabstraction::MessagePassingConnectionInformation& info) {
    message_passing_server_ = message_passing_server;
    connection_information_ = info;
}

int EndpointMessagePassingReceiver::get_connection_id() {
    return connection_information_.scoid_;
}

// EndpointMessagePassingSender.
EndpointMessagePassingSender::EndpointMessagePassingSender(const std::string& server_name) {
    is_connected_ = message_passing_sender_.connect(server_name);
}

EndpointMessagePassingSender::~EndpointMessagePassingSender() {
    message_passing_sender_.disconnect();
}

bool EndpointMessagePassingSender::is_connected() {
    return is_connected_;
}

void EndpointMessagePassingSender::start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer) {}

void EndpointMessagePassingSender::stop_listen() {}

void EndpointMessagePassingSender::send_message(const std::uint8_t* message, std::size_t size,
                                                std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    message_passing_sender_.send(message, size);
}

std::shared_ptr<lgsomeip::osabstraction::Address> EndpointMessagePassingSender::get_sender_address() {
    return nullptr;
}

} // namespace lgsomeip
