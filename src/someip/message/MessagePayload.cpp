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
#include <message/MessagePayload.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/formatLog.h>

namespace lgsomeip {

void MessagePayload::append(const std::uint8_t* data, std::uint32_t length) {
    if (length == 0) {
        return;
    }
    if (data == nullptr) {
        throw std::invalid_argument("MessagePayload::append requires data for a non-empty payload");
    }
    payload_.insert(payload_.end(), data, data + length);
}

void MessagePayload::append(const std::vector<std::uint8_t>& data) {
    payload_.insert(payload_.end(), data.begin(), data.end());
}

void MessagePayload::set_capacity(std::uint32_t size) {
    payload_.reserve(size);
}

void MessagePayload::set_payload(const std::uint8_t* data, std::uint32_t length) {
    if (length > 0 && data == nullptr) {
        throw std::invalid_argument("MessagePayload::set_payload requires data for a non-empty payload");
    }
    if (length == 0) {
        payload_.clear();
        print_byte_message("MessagePayload::set_payload by uint8_t* data type with length", nullptr, 0);
        return;
    }
    payload_.assign(data, data + length);
    print_byte_message("MessagePayload::set_payload by uint8_t* data type with length", payload_.data(),
                       payload_.size());
}

void MessagePayload::set_payload(const MessagePayload& payload) {
    payload_ = payload.payload_;
    print_byte_message("MessagePayload::set_payload by MessagePayload type", payload_.data(), payload_.size());
}

void MessagePayload::set_payload(const std::vector<std::uint8_t>& data) {
    payload_ = data;
    print_byte_message("MessagePayload::set_payload by vector data type", payload_.data(), payload_.size());
}

std::uint8_t* MessagePayload::get_payload() {
    return payload_.data();
}

const std::uint8_t* MessagePayload::get_payload() const {
    return payload_.data();
}

std::vector<std::uint8_t>& MessagePayload::get_payload_vector() {
    return payload_;
}

std::uint32_t MessagePayload::get_length() const {
    return payload_.size();
}

bool MessagePayload::operator==(const MessagePayload& rhs) {
    if (payload_.size() != rhs.payload_.size())
        return false;
    for (std::uint32_t i = 0; i < payload_.size(); i++) {
        if (payload_[i] != rhs.payload_[i])
            return false;
    }
    return true;
}

} // namespace lgsomeip
