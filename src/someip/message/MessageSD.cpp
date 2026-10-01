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

#include <vector>

#include <message/MessageSD.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/logger.h>

namespace lgsomeip {

MessageSD::MessageSD() : flags_(0), reboot_flag_(false) {
    set_message_id(0xFFFF8100);
    set_interface_version(0x01);
    set_message_type(SOMEIP_MESSAGE_TYPE::NOTIFICATION);
}

MessageSD::~MessageSD() {}

std::uint32_t MessageSD::get_length() const {
    std::uint32_t total_len = SOMEIP_SD_HEADER::SIZE;

    total_len += 4; // Length Field of SOME/IP-SD Entities
    total_len += entries_.size() * SOMEIP_SD_ENTRY::SIZE;

    total_len += 4; // Length Field of SOME/IP-SD Options
    for (auto& item : options_) {
        total_len += item.get_length();
    }
    return total_len + 8;
}

bool MessageSD::deserialize(std::uint8_t* data, std::uint32_t length) {
    if (data == nullptr || length < SOMEIP_HEADER::SIZE) {
        LGSOMEIP_LOG_ERROR << "MessageSD::deserialize / length : " << length << " is wrong";
        return false;
    }

    if (!MessageHeader::deserialize(data, length) || length_ < 8 || length_ > length - 8) {
        LGSOMEIP_LOG_ERROR << "MessageSD::deserialize / SOME/IP length field is wrong";
        return false;
    }

    std::uint8_t* message_data = data + SOMEIP_HEADER::SIZE;
    const std::uint32_t message_length = length_ - 8;
    if (message_length < SOMEIP_SD_HEADER::SIZE + 8) {
        LGSOMEIP_LOG_ERROR << "MessageSD::deserialize / length : " << length << " is wrong";
        return false;
    }

    get_byte_stream(&flags_, message_data + SOMEIP_SD_HEADER::POS::FLAGS);

    if ((flags_ & SOMEIP_REBOOT_FLAG) != 0) {
        reboot_flag_ = true;
    } else {
        reboot_flag_ = false;
    }

    std::uint32_t position = SOMEIP_SD_HEADER::SIZE;
    std::uint32_t entity_length = 0;

    entries_.clear();
    get_byte_stream(&entity_length, message_data + position);
    position += 4;

    if (entity_length > message_length - position - 4 || entity_length % SOMEIP_SD_ENTRY::SIZE != 0) {
        LGSOMEIP_LOG_ERROR << "MessageSD::deserialize / entity_length : " << entity_length << " is wrong";
        return false;
    }

    while (entity_length > 0) {
        SDEntry entry;
        if (!entry.deserialize(message_data + position, entity_length)) {
            return false;
        }
        entries_.push_back(entry);
        entity_length = entity_length - 16;
        position = position + 16;
    }

    if (position + 4 > message_length) {
        return false;
    }

    std::uint32_t option_length = 0;
    options_.clear();
    get_byte_stream(&option_length, message_data + position);
    position = position + 4;

    if (option_length != message_length - position) {
        LGSOMEIP_LOG_ERROR << "MessageSD::deserialize / option_length : " << option_length << " is wrong";
        return false;
    }

    while (option_length > 0) {
        SDOption option;
        const std::uint32_t serialized_length = option.deserialize(message_data + position, option_length);
        if (serialized_length == 0 || serialized_length > option_length) {
            return false;
        }
        options_.push_back(option);
        option_length = option_length - serialized_length;
        position = position + serialized_length;
    }

    return true;
}

std::uint32_t MessageSD::serialize(std::uint8_t* data) {
    std::uint32_t index;
    std::uint32_t position = 0;
    std::uint32_t payload_length = get_length();

    std::uint8_t* message_data = data + SOMEIP_HEADER::SIZE;

    for (index = 0; index < payload_length; index++)
        message_data[index] = 0;

    set_byte_stream(message_data + SOMEIP_SD_HEADER::POS::FLAGS, &flags_);
    position += SOMEIP_SD_HEADER::SIZE;

    std::uint32_t entity_count = entries_.size() * 16;
    set_byte_stream(message_data + position, &entity_count);
    position = position + 4;

    for (auto& item : entries_) {
        item.serialize(&message_data[position]);
        position += SOMEIP_SD_ENTRY::SIZE;
    }

    std::uint32_t serialized_length;
    std::uint32_t options_length = 0;
    position = position + 4;

    for (auto& item : options_) {
        serialized_length = item.serialize(&message_data[position + options_length]);
        options_length += serialized_length;
    }

    set_byte_stream(message_data + position - 4, &options_length);

    MessageHeader::set_length(position + options_length + 8);
    MessageHeader::serialize(data);

    return position + options_length + SOMEIP_HEADER::SIZE;
}

std::vector<SDEntry>& MessageSD::entries() {
    return entries_;
}

SDEntry& MessageSD::entry(int n) {
    return entries_[n];
}

std::vector<SDOption>& MessageSD::options() {
    return options_;
}

SDOption& MessageSD::option(int n) {
    return options_[n];
}

bool MessageSD::get_reboot_flag() {
    return reboot_flag_;
}

} // namespace lgsomeip
