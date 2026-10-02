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
#include <message/SDEntry.h>
#include <utils/byteorder/bytestream.h>
#include <utils/log/logger.h>

namespace lgsomeip {

SDEntry::SDEntry() {}

SDEntry::~SDEntry() {}

std::uint32_t SDEntry::get_length() {
    return SOMEIP_SD_ENTRY::SIZE;
}

bool SDEntry::deserialize(std::uint8_t* data, std::uint32_t length) {
    if (length < SOMEIP_SD_ENTRY::SIZE) {
        LGSOMEIP_LOG_ERROR << "SDEntry::deserialize / length : " << length << " is wrong";
        return false;
    }

    std::uint8_t option_count = 0;
    std::uint8_t type = 0;
    get_byte_stream(&type, data);

    switch (type) {
    case SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID:
        get_byte_stream(&type_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::TYPE);
        get_byte_stream(&first_option_index_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTION1);
        get_byte_stream(&second_option_index_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTION2);

        get_byte_stream(&option_count, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTIONNUM);
        first_option_count_ = option_count >> 4;
        second_option_count_ = option_count & 0x0f;

        get_byte_stream(&service_id_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::SOMEIP_SERVICE_ID);
        get_byte_stream(&instance_id_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::SOMEIP_INSTANCE_ID);
        get_byte_stream(&major_version_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::MAJOR);
        get_byte_stream(&ttl_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::TTL, 3);
        get_byte_stream(&minor_version_, data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::MINOR);

        break;

    case SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID:
        get_byte_stream(&type_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::TYPE);
        get_byte_stream(&first_option_index_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTION1);
        get_byte_stream(&second_option_index_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTION2);

        get_byte_stream(&option_count, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTIONNUM);
        first_option_count_ = option_count >> 4;
        second_option_count_ = option_count & 0x0f;

        get_byte_stream(&service_id_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::SOMEIP_SERVICE_ID);
        get_byte_stream(&instance_id_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::SOMEIP_INSTANCE_ID);
        get_byte_stream(&major_version_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::MAJOR);
        get_byte_stream(&ttl_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::TTL, 3);
        get_byte_stream(&minor_version_, data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::MINOR);

        break;

    case SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID:
        get_byte_stream(&type_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::TYPE);
        get_byte_stream(&first_option_index_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTION1);
        get_byte_stream(&second_option_index_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTION2);

        get_byte_stream(&option_count, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTIONNUM);
        first_option_count_ = option_count >> 4;
        second_option_count_ = option_count & 0x0f;

        get_byte_stream(&service_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::SOMEIP_SERVICE_ID);
        get_byte_stream(&instance_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::SOMEIP_INSTANCE_ID);
        get_byte_stream(&major_version_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::MAJOR);
        get_byte_stream(&ttl_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::TTL, 3);
        get_byte_stream(&flags_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::FLAGS);
        get_byte_stream(&event_group_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::EVENTGROUPID);

        break;

    case SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID:
        get_byte_stream(&type_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::TYPE);
        get_byte_stream(&first_option_index_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTION1);
        get_byte_stream(&second_option_index_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTION2);

        get_byte_stream(&option_count, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTIONNUM);
        first_option_count_ = option_count >> 4;
        second_option_count_ = option_count & 0x0f;

        get_byte_stream(&service_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::SOMEIP_SERVICE_ID);
        get_byte_stream(&instance_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::SOMEIP_INSTANCE_ID);
        get_byte_stream(&major_version_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::MAJOR);
        get_byte_stream(&ttl_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::TTL, 3);
        get_byte_stream(&flags_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::FLAGS);
        get_byte_stream(&event_group_id_, data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::EVENTGROUPID);

        break;

    // SOME/IP-SD Extension for Application Management
    case SOMEIP_SD_ENTRY::INTERNAL::TYPEID:
        get_byte_stream(&type_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::TYPE);
        get_byte_stream(&first_option_index_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTION1);
        get_byte_stream(&second_option_index_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTION2);

        get_byte_stream(&option_count, data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTIONNUM);
        first_option_count_ = option_count >> 4;
        second_option_count_ = option_count & 0x0f;

        get_byte_stream(&service_id_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::SOMEIP_SERVICE_ID);
        get_byte_stream(&instance_id_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::SOMEIP_INSTANCE_ID);
        get_byte_stream(&major_version_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::MAJOR);
        get_byte_stream(&ttl_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::TTL, 3);
        get_byte_stream(&minor_version_, data + SOMEIP_SD_ENTRY::INTERNAL::POS::MINOR);

        break;

    default:
        // TODO: make runtime exception!
        LGSOMEIP_LOG_ERROR << "SDEntry::deserialize / type : " << type << " is wrong";
        return false;
    }

    return true;
}

std::uint32_t SDEntry::serialize(std::uint8_t* data) {
    std::uint8_t option_count = 0;

    switch (type_) {
    case SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID:
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::TYPE, &type_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTION1, &first_option_index_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTION2, &second_option_index_);

        option_count = ((first_option_count_ << 4) & 0xf0) | (second_option_count_ & 0x0f);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::OPTIONNUM, &option_count);

        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::SOMEIP_SERVICE_ID, &service_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::SOMEIP_INSTANCE_ID, &instance_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::MAJOR, &major_version_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::TTL, &ttl_, 3);
        set_byte_stream(data + SOMEIP_SD_ENTRY::FINDSERVICE::POS::MINOR, &minor_version_);

        break;

    case SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID:
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::TYPE, &type_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTION1, &first_option_index_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTION2, &second_option_index_);

        option_count = ((first_option_count_ << 4) & 0xf0) | (second_option_count_ & 0x0f);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::OPTIONNUM, &option_count);

        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::SOMEIP_SERVICE_ID, &service_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::SOMEIP_INSTANCE_ID, &instance_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::MAJOR, &major_version_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::TTL, &ttl_, 3);
        set_byte_stream(data + SOMEIP_SD_ENTRY::OFFERSERVICE::POS::MINOR, &minor_version_);
        break;

    case SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID:
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::TYPE, &type_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTION1, &first_option_index_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTION2, &second_option_index_);

        option_count = ((first_option_count_ << 4) & 0xf0) | (second_option_count_ & 0x0f);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::OPTIONNUM, &option_count);

        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::SOMEIP_SERVICE_ID, &service_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::SOMEIP_INSTANCE_ID, &instance_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::MAJOR, &major_version_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::TTL, &ttl_, 3);

        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::FLAGS, &flags_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBE::POS::EVENTGROUPID, &event_group_id_);
        break;

    case SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID:
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::TYPE, &type_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTION1, &first_option_index_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTION2, &second_option_index_);

        option_count = ((first_option_count_ << 4) & 0xf0) | (second_option_count_ & 0x0f);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::OPTIONNUM, &option_count);

        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::SOMEIP_SERVICE_ID, &service_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::SOMEIP_INSTANCE_ID, &instance_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::MAJOR, &major_version_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::TTL, &ttl_, 3);

        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::FLAGS, &flags_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::SUBSCRIBEACK::POS::EVENTGROUPID, &event_group_id_);
        break;

    // SOME/IP-SD Extension for Application Management
    case SOMEIP_SD_ENTRY::INTERNAL::TYPEID:
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::TYPE, &type_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTION1, &first_option_index_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTION2, &second_option_index_);

        option_count = ((first_option_count_ << 4) & 0xf0) | (second_option_count_ & 0x0f);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::OPTIONNUM, &option_count);

        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::SOMEIP_SERVICE_ID, &service_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::SOMEIP_INSTANCE_ID, &instance_id_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::MAJOR, &major_version_);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::TTL, &ttl_, 3);
        set_byte_stream(data + SOMEIP_SD_ENTRY::INTERNAL::POS::MINOR, &minor_version_);

        break;

    default:
        // TODO: make runtime exception!
        break;
    }

    return SOMEIP_SD_ENTRY::SIZE;
}

std::uint8_t SDEntry::get_type() const {
    return type_;
}

std::uint8_t SDEntry::get_option1st_index() const {
    return first_option_index_;
}

std::uint8_t SDEntry::get_option1st_count() const {
    return first_option_count_;
}

std::uint8_t SDEntry::get_option2nd_index() const {
    return second_option_index_;
}

std::uint8_t SDEntry::get_option2nd_count() const {
    return second_option_count_;
}

std::uint16_t SDEntry::get_service_id() const {
    return service_id_;
}

std::uint16_t SDEntry::get_instance_id() const {
    return instance_id_;
}

std::uint8_t SDEntry::get_major_version() const {
    return major_version_;
}

std::uint32_t SDEntry::get_ttl() const {
    return ttl_;
}

void SDEntry::set_type(std::uint8_t type) {
    type_ = type;
}

void SDEntry::set_option1st_index(std::uint8_t first_option_index) {
    first_option_index_ = first_option_index;
}

void SDEntry::set_option1st_count(std::uint8_t first_option_count) {
    first_option_count_ = first_option_count;
}

void SDEntry::set_option2nd_index(std::uint8_t second_option_index) {
    second_option_index_ = second_option_index;
}

void SDEntry::set_option2nd_count(std::uint8_t second_option_count) {
    second_option_count_ = second_option_count;
}

void SDEntry::set_service_id(std::uint16_t service_id) {
    service_id_ = service_id;
}

void SDEntry::set_instance_id(std::uint16_t instance_id) {
    instance_id_ = instance_id;
}

void SDEntry::set_major_version(std::uint8_t major_version) {
    major_version_ = major_version;
}

void SDEntry::set_ttl(std::uint32_t ttl) {
    ttl_ = ttl;
}

// For FINDSERVICE and OFFERSERVICE
std::uint32_t SDEntry::get_minor_version() const {
    return minor_version_;
}

void SDEntry::set_minor_version(std::uint32_t minor_version) {
    minor_version_ = minor_version;
}

// For SUBSCRIBE and SUBSCRIBEACK
std::uint8_t SDEntry::get_flag() const {
    return flags_;
}

std::uint16_t SDEntry::get_event_group_id() const {
    return event_group_id_;
}

void SDEntry::set_flag(std::uint8_t flag) {
    flags_ = flag;
}

void SDEntry::set_event_group_id(std::uint16_t event_group_id) {
    event_group_id_ = event_group_id;
}

// For CHECK LENGTH OF ENTRY
bool SDEntry::get_wrong_length() const {
    return wrong_length_;
}

void SDEntry::set_wrong_length(bool wrong_length) {
    wrong_length_ = wrong_length;
}

} // namespace lgsomeip
