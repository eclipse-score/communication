// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "message_impl.hpp"
#include "payload_impl.hpp"

namespace vsomeip {

message_impl::message_impl(std::shared_ptr<lgsomeip::Message> _lgsomeipMsg) : message_base_impl(_lgsomeipMsg) {}

message_impl::~message_impl() {}

length_t message_impl::get_length() const {
    return mlgsomeipMsg->get_length();
}

std::shared_ptr<payload> message_impl::get_payload() const {
    return std::make_shared<payload_impl>(mlgsomeipMsg->get_payload_type());
}

void message_impl::set_payload(std::shared_ptr<payload> _payload) {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        mlgsomeipMsg->set_payload(payload->get_lgsomeip_payload());
    } catch (...) {
    }
}

bool message_impl::serialize(serializer* _to) const {
    throw std::runtime_error("message_impl::serialize / not implemented");
    return true;
}

bool message_impl::deserialize(deserializer* _from) {
    throw std::runtime_error("message_impl::deserialize / not implemented");
    return true;
}

} // namespace vsomeip
