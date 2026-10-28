// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "deserializer.hpp"
#include "payload_impl.hpp"
#include "serializer.hpp"

namespace vsomeip {

payload_impl::payload_impl() {
    mLSOMEIPPayload = std::make_shared<lgsomeip::Payload>();
}

payload_impl::payload_impl(const byte_t* _data, uint32_t _size) {
    mLSOMEIPPayload = std::make_shared<lgsomeip::Payload>();
    mLSOMEIPPayload->set_payload(_data, _size);
}

payload_impl::payload_impl(const std::vector<byte_t>& _data) {
    mLSOMEIPPayload = std::make_shared<lgsomeip::Payload>();
    mLSOMEIPPayload->set_payload(_data);
}

payload_impl::payload_impl(const payload_impl& _payload) {
    mLSOMEIPPayload = std::make_shared<lgsomeip::Payload>();
    mLSOMEIPPayload->set_payload(*_payload.mLSOMEIPPayload);
}

payload_impl::~payload_impl() {}

bool payload_impl::operator==(const payload& _other) {
    bool is_equal(true);
    try {
        const payload_impl& other = dynamic_cast<const payload_impl&>(_other);
        is_equal = mLSOMEIPPayload == other.mLSOMEIPPayload;
    } catch (...) {
        is_equal = false;
    }
    return is_equal;
}

byte_t* payload_impl::get_data() {
    return mLSOMEIPPayload->get_payload();
}

const byte_t* payload_impl::get_data() const {
    return mLSOMEIPPayload->get_payload();
}

length_t payload_impl::get_length() const {
    return length_t(mLSOMEIPPayload->get_length());
}

void payload_impl::set_capacity(length_t _capacity) {
    mLSOMEIPPayload->set_capacity(_capacity);
}

void payload_impl::set_data(const byte_t* _data, const length_t _length) {
    mLSOMEIPPayload->set_payload(_data, _length);
}

void payload_impl::set_data(const std::vector<byte_t>& _data) {
    mLSOMEIPPayload->set_payload(_data);
}

void payload_impl::set_data(std::vector<byte_t>&& _data) {
    mLSOMEIPPayload->set_payload(std::move(_data));
}

bool payload_impl::serialize(serializer* _to) const {
    throw std::runtime_error("payload_impl::serialize / not implemented");
    return (0 != _to && _to->serialize(mLSOMEIPPayload->get_payload_vector()));
}

bool payload_impl::deserialize(deserializer* _from) {
    throw std::runtime_error("payload_impl::deserialize / not implemented");
    return (0 != _from && _from->deserialize(mLSOMEIPPayload->get_payload_vector()));
}

// For LSOMEIP Porting Mathod;
payload_impl::payload_impl(const std::shared_ptr<lgsomeip::Payload>& _payload) : mLSOMEIPPayload(_payload) {}

std::shared_ptr<lgsomeip::Payload> payload_impl::get_lgsomeip_payload() {
    return mLSOMEIPPayload;
}

} // namespace vsomeip
