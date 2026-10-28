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

#include <message/MessageSOMEIP.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/formatLog.h>

namespace lgsomeip {

MessageSOMEIP::MessageSOMEIP() {
    payload_ = std::make_shared<MessagePayload>();
}

MessageSOMEIP::~MessageSOMEIP() {}

std::uint32_t MessageSOMEIP::get_length() const {
    // Total length of the SOME/IP packet =
    // the length of the SOME/IP header + the length of the payload
    return payload_->get_length() + SOMEIP_HEADER::SIZE;
}

bool MessageSOMEIP::deserialize(std::uint8_t* data, std::uint32_t length) {
    bool success = MessageHeader::deserialize(data, length);
    if (success) {
        set_payload(data + SOMEIP_HEADER::SIZE, length - SOMEIP_HEADER::SIZE);
    }
    return success;
}

std::uint32_t MessageSOMEIP::serialize(std::uint8_t* data) {
    // The value of the length field of a SOME/IP packet = the length of payload + 8 bytes,
    // since the length includes the Request ID, the Protocol Version, Interfacce Version,
    // the Message Type and the Return Code fields of the SOME/IP Header.
    MessageHeader::set_length(payload_->get_length() + 8);
    MessageHeader::serialize(data);

    std::uint8_t* payload_data = payload_->get_payload();
    for (std::uint32_t i = 0; i < payload_->get_length(); i++) {
        data[SOMEIP_HEADER::SIZE + i] = payload_data[i];
    }
    return payload_->get_length() + SOMEIP_HEADER::SIZE;
}

std::uint32_t MessageSOMEIP::serialize_some_ip_header(std::uint8_t* data) {
    // The value of the length field of a SOME/IP packet = the length of payload + 8 bytes,
    // since the length includes the Request ID, the Protocol Version, Interfacce Version,
    // the Message Type and the Return Code fields of the SOME/IP Header.
    MessageHeader::set_length(payload_->get_length() + 8);

    return MessageHeader::serialize(data);
}

void MessageSOMEIP::set_payload(std::shared_ptr<MessagePayload> payload) {
    if (payload != nullptr) {
        payload_ = payload;
        // The value of the length field of a SOME/IP packet = the length of payload + 8 bytes,
        // since the length includes the Request ID, the Protocol Version, Interfacce Version,
        // the Message Type and the Return Code fields of the SOME/IP Header.
        MessageHeader::set_length(payload_->get_length() + 8);
        print_byte_message("MessageSOMEIP::set_payload by MessagePayload", payload_->get_payload(),
                           payload_->get_length());
    }
}

void MessageSOMEIP::set_payload(std::uint8_t* data, std::uint32_t length) {
    payload_->set_payload(data, length);
}

std::shared_ptr<MessagePayload> MessageSOMEIP::get_payload_type() {
    return payload_;
}

std::uint8_t* MessageSOMEIP::get_payload() {
    return payload_->get_payload();
}

void MessageSOMEIP::set_instance_id(std::uint16_t instance_id) {
    instance_id_ = instance_id;
}

void MessageSOMEIP::set_is_valid_crc(bool valid_crc) {
    valid_crc_ = valid_crc;
}

bool MessageSOMEIP::get_is_valid_crc() const {
    return valid_crc_;
}

std::uint16_t MessageSOMEIP::get_instance_id() const {
    return instance_id_;
}

} // namespace lgsomeip
