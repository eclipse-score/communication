// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// #include <cstring>

#include <vsomeip/internal/serializable.hpp>

#include "serializer.hpp"
// #include "../../utility/include/byteorder.hpp"
// #include "../../logging/include/instance.hpp"

namespace vsomeip {

serializer::serializer(std::uint32_t _buffer_shrink_threshold)
    : data_(0), shrink_count_(0), buffer_shrink_threshold_(_buffer_shrink_threshold) {}

serializer::~serializer() {}

bool serializer::serialize(const serializable* _from) {
    // return (_from && _from->serialize(this));
    return true;
}

bool serializer::serialize(const uint8_t _value) {
    // data_.push_back(_value);
    return true;
}

bool serializer::serialize(const uint16_t _value) {
    // data_.push_back(VSOMEIP_WORD_BYTE1(_value));
    // data_.push_back(VSOMEIP_WORD_BYTE0(_value));
    return true;
}

bool serializer::serialize(const uint32_t _value, bool _omit_last_byte) {
    // if (!_omit_last_byte) {
    //     data_.push_back(VSOMEIP_LONG_BYTE3(_value));
    // }
    // data_.push_back(VSOMEIP_LONG_BYTE2(_value));
    // data_.push_back(VSOMEIP_LONG_BYTE1(_value));
    // data_.push_back(VSOMEIP_LONG_BYTE0(_value));
    return true;
}

bool serializer::serialize(const uint8_t* _data, uint32_t _length) {
    // try {
    //     data_.insert(data_.end(), _data, _data + _length);
    // } catch(const std::bad_alloc &e) {
    //     VSOMEIP_ERROR << "Couldn't allocate memory in serializer::serialize(*_data, length)" << e.what();
    //     return false;
    // }
    return true;
}

bool serializer::serialize(const std::vector<byte_t>& _data) {
    // try {
    //     data_.insert(data_.end(),_data.begin(), _data.end());
    // } catch(const std::bad_alloc &e) {
    //     VSOMEIP_ERROR << "Couldn't allocate memory in serializer::serialize(vector)" << e.what();
    //     return false;
    // }
    return true;
}

const byte_t* serializer::get_data() const {
    // return data_.data();
    return nullptr;
}

uint32_t serializer::get_capacity() const {
    // return static_cast<std::uint32_t>(data_.max_size());
    return 0;
}

uint32_t serializer::get_size() const {
    // return static_cast<std::uint32_t>(data_.size());
    return 0;
}

void serializer::set_data(byte_t* _data, uint32_t _capacity) {
    // data_.clear();
    // try {
    //     data_.insert(data_.end(), _data, _data + _capacity);
    // } catch(const std::bad_alloc &e) {
    //     VSOMEIP_ERROR << "Couldn't allocate memory in serializer::set_data" << e.what();
    // }
}

void serializer::reset() {
    // if (buffer_shrink_threshold_) {
    //     if (data_.size() < (data_.capacity() >> 1)) {
    //         shrink_count_++;
    //     } else {
    //         shrink_count_ = 0;
    //     }
    // }
    // data_.clear();
    // if (buffer_shrink_threshold_ && shrink_count_ > buffer_shrink_threshold_) {
    //     data_.shrink_to_fit();
    //     shrink_count_ = 0;
    // }
}

} // namespace vsomeip
