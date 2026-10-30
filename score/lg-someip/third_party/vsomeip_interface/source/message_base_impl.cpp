// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "message_impl.hpp"
#include <iostream>

namespace vsomeip {

message_base_impl::message_base_impl(std::shared_ptr<lgsomeip::Message> _lgsomeipMsg)
    : is_reliable_(false), is_initial_(false), is_valid_crc_(true) {
    if (_lgsomeipMsg == nullptr) {
        mlgsomeipMsg = std::make_shared<lgsomeip::Message>();
    } else {
        mlgsomeipMsg = _lgsomeipMsg;
    }

    // header_.set_owner(this);
}

message_base_impl::~message_base_impl() {}

// header interface
message_t message_base_impl::get_message() const {
    // return VSOMEIP_WORDS_TO_LONG(header_.service_, header_.method_);

    return mlgsomeipMsg->get_message_id();
}

void message_base_impl::set_message(message_t _message) {
    // header_.service_ = VSOMEIP_LONG_WORD0(_message);
    // header_.method_ = VSOMEIP_LONG_WORD1(_message);

    mlgsomeipMsg->set_message_id(_message);
}

service_t message_base_impl::get_service() const {
    // return header_.service_;
    return static_cast<service_t>(mlgsomeipMsg->get_message_id() >> 16);
}

void message_base_impl::set_service(service_t _service) {
    // header_.service_ = _service;
    message_t msgid = mlgsomeipMsg->get_message_id();
    msgid = (static_cast<message_t>(_service) << 16) | (msgid & 0xffff);
    mlgsomeipMsg->set_message_id(msgid);
}

instance_t message_base_impl::get_instance() const {
    // return header_.instance_;
    // Message is not containing Instance ID
    return mlgsomeipMsg->get_instance_id();
}

void message_base_impl::set_instance(instance_t _instance) {
    mlgsomeipMsg->set_instance_id(_instance);
}

method_t message_base_impl::get_method() const {
    // return header_.method_;
    return static_cast<service_t>(mlgsomeipMsg->get_message_id() & 0xffff);
}

void message_base_impl::set_method(method_t _method) {
    // header_.method_ = _method;
    message_t msgid = mlgsomeipMsg->get_message_id();
    msgid = (msgid & 0xffff0000) | (static_cast<message_t>(_method) & 0x0000ffff);
    mlgsomeipMsg->set_message_id(msgid);
}

request_t message_base_impl::get_request() const {
    // return VSOMEIP_WORDS_TO_LONG(header_.client_, header_.session_);
    return mlgsomeipMsg->get_request_id();
}

client_t message_base_impl::get_client() const {
    // return header_.client_;
    return static_cast<client_t>(mlgsomeipMsg->get_request_id() >> 16);
}

void message_base_impl::set_client(client_t _client) {
    // header_.client_ = _client;
    request_t msgid = mlgsomeipMsg->get_request_id();
    msgid = (static_cast<request_t>(_client) << 16) | (msgid & 0xffff);
    mlgsomeipMsg->set_request_id(msgid);
}

session_t message_base_impl::get_session() const {
    // return header_.session_;
    return static_cast<session_t>(mlgsomeipMsg->get_request_id() & 0xffff);
}

void message_base_impl::set_session(session_t _session) {
    // header_.session_ = _session;
    request_t msgid = mlgsomeipMsg->get_request_id();
    msgid = (msgid & 0xffff0000) | (static_cast<request_t>(_session) & 0x0000ffff);
    mlgsomeipMsg->set_request_id(msgid);
}

protocol_version_t message_base_impl::get_protocol_version() const {
    // return header_.protocol_version_;
    return mlgsomeipMsg->get_protocol_version();
}

void message_base_impl::set_protocol_version(protocol_version_t _protocol_version) {
    // header_.protocol_version_ = _protocol_version;
    mlgsomeipMsg->set_protocol_version(_protocol_version);
}

interface_version_t message_base_impl::get_interface_version() const {
    // return header_.interface_version_;
    return mlgsomeipMsg->get_interface_version();
}

void message_base_impl::set_interface_version(interface_version_t _interface_version) {
    // header_.interface_version_ = _interface_version;
    mlgsomeipMsg->set_interface_version(_interface_version);
}

message_type_e message_base_impl::get_message_type() const {
    // return header_.type_;
    // return message_type_e::MT_UNKNOWN;
    message_type_e type = message_type_e(mlgsomeipMsg->get_message_type());
    return type;
}

void message_base_impl::set_message_type(message_type_e _type) {
    // header_.type_ = _type;
    uint8_t type = static_cast<typename std::underlying_type<message_type_e>::type>(_type);
    mlgsomeipMsg->set_message_type(type);
}

return_code_e message_base_impl::get_return_code() const {
    // return header_.code_;
    return_code_e type = return_code_e(mlgsomeipMsg->get_return_code());
    return type;
}

void message_base_impl::set_return_code(return_code_e _code) {
    // header_.code_ = _code;
    uint8_t code = static_cast<typename std::underlying_type<return_code_e>::type>(_code);
    mlgsomeipMsg->set_return_code(code);
}

bool message_base_impl::is_reliable() const {
    // return is_reliable_;

    // Message is not containing Reliable
    return true;
}

void message_base_impl::set_reliable(bool _is_reliable) {
    // is_reliable_ = _is_reliable;

    // Message is not containing Reliable
}

bool message_base_impl::is_initial() const {
    // return is_initial_;

    // Message is not containing initial
    return false;
}

void message_base_impl::set_initial(bool _is_initial) {
    // is_initial_ = _is_initial;

    // Message is not containing initial
}

bool message_base_impl::is_valid_crc() const {
    return mlgsomeipMsg->get_is_valid_crc();
}

void message_base_impl::set_is_valid_crc(bool _is_valid_crc) {
    mlgsomeipMsg->set_is_valid_crc(_is_valid_crc);
}

std::shared_ptr<lgsomeip::Message> message_base_impl::get_lgsomeip_message() {
    return mlgsomeipMsg;
}

} // namespace vsomeip
