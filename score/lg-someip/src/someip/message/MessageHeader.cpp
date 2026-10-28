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

#include <message/MessageConstant.h>
#include <message/MessageHeader.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/logger.h>

namespace lgsomeip {

MessageHeader::MessageHeader()
    : message_id_(0), length_(SOMEIP_HEADER::SIZE - 8), request_id_(0), return_code_(0), message_type_(0) {}

MessageHeader::~MessageHeader() {}

bool MessageHeader::is_service_discovery() {
    return (message_id_ == 0xFFFF8100);
}

bool MessageHeader::deserialize(std::uint8_t* data, std::uint32_t len) {
    if (data == nullptr || len < SOMEIP_HEADER::SIZE) {
        LGSOMEIP_LOG_ERROR << "MessageHeader::deserialize / length : " << len << " is wrong";
        return false;
    }

    get_byte_stream(&message_id_, data + SOMEIP_HEADER::POS::MESSAGEID);
    get_byte_stream(&length_, data + SOMEIP_HEADER::POS::LENGTH);
    get_byte_stream(&request_id_, data + SOMEIP_HEADER::POS::REQUESTID);

    get_byte_stream(&protocol_version_, data + SOMEIP_HEADER::POS::PROTOCOL);
    get_byte_stream(&interface_version_, data + SOMEIP_HEADER::POS::INTERFACE);
    get_byte_stream(&message_type_, data + SOMEIP_HEADER::POS::MESSAGETYPE);
    get_byte_stream(&return_code_, data + SOMEIP_HEADER::POS::RETURNCODE);

    return true;
}

std::uint32_t MessageHeader::serialize(std::uint8_t* data) {
    set_byte_stream(data + SOMEIP_HEADER::POS::MESSAGEID, &message_id_);
    set_byte_stream(data + SOMEIP_HEADER::POS::LENGTH, &length_);
    set_byte_stream(data + SOMEIP_HEADER::POS::REQUESTID, &request_id_);

    set_byte_stream(data + SOMEIP_HEADER::POS::PROTOCOL, &protocol_version_);
    set_byte_stream(data + SOMEIP_HEADER::POS::INTERFACE, &interface_version_);
    set_byte_stream(data + SOMEIP_HEADER::POS::MESSAGETYPE, &message_type_);
    set_byte_stream(data + SOMEIP_HEADER::POS::RETURNCODE, &return_code_);

    return SOMEIP_HEADER::SIZE;
}

std::uint8_t MessageHeader::get_interface_version() const {
    return interface_version_;
}

std::uint32_t MessageHeader::get_message_id() const {
    return message_id_;
}

std::uint32_t MessageHeader::get_length() const {
    return length_;
}

std::uint8_t MessageHeader::get_message_type() const {
    return message_type_;
}

std::uint8_t MessageHeader::get_protocol_version() const {
    return protocol_version_;
}

std::uint32_t MessageHeader::get_request_id() const {
    return request_id_;
}

std::uint8_t MessageHeader::get_return_code() const {
    return return_code_;
}

void MessageHeader::set_interface_version(std::uint8_t ver) {
    interface_version_ = ver;
}

void MessageHeader::set_message_id(std::uint32_t message_id) {
    message_id_ = message_id;
}

void MessageHeader::set_length(std::uint32_t length) {
    length_ = length;
}

void MessageHeader::set_message_type(std::uint8_t type) {
    message_type_ = type;
}

void MessageHeader::set_protocol_version(std::uint8_t ver) {
    protocol_version_ = ver;
}

void MessageHeader::set_request_id(std::uint32_t request_id) {
    request_id_ = request_id;
}

void MessageHeader::set_return_code(std::uint8_t ret_code) {
    return_code_ = ret_code;
}

} // namespace lgsomeip
