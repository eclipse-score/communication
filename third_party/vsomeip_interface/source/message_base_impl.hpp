// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_BASE_IMPL_HPP
#define VSOMEIP_MESSAGE_BASE_IMPL_HPP

#include <vsomeip/export.hpp>
#include <vsomeip/message.hpp>
#include <message/Message.h>

namespace vsomeip {

class message_base_impl : virtual public message_base {
public:
    VSOMEIP_EXPORT message_base_impl(std::shared_ptr<lgsomeip::Message> _lgsomeipMsg = nullptr);
    VSOMEIP_EXPORT virtual ~message_base_impl();

    VSOMEIP_EXPORT message_t get_message() const;
    VSOMEIP_EXPORT void set_message(message_t _message);

    VSOMEIP_EXPORT service_t get_service() const;
    VSOMEIP_EXPORT void set_service(service_t _service);

    VSOMEIP_EXPORT instance_t get_instance() const;
    VSOMEIP_EXPORT void set_instance(instance_t _instance);

    VSOMEIP_EXPORT method_t get_method() const;
    VSOMEIP_EXPORT void set_method(method_t _method);

    VSOMEIP_EXPORT request_t get_request() const;

    VSOMEIP_EXPORT client_t get_client() const;
    VSOMEIP_EXPORT void set_client(client_t _client);

    VSOMEIP_EXPORT session_t get_session() const;
    VSOMEIP_EXPORT void set_session(session_t _session);

    VSOMEIP_EXPORT protocol_version_t get_protocol_version() const;
    VSOMEIP_EXPORT void set_protocol_version(protocol_version_t _version);

    VSOMEIP_EXPORT interface_version_t get_interface_version() const;
    VSOMEIP_EXPORT void set_interface_version(interface_version_t _version);

    VSOMEIP_EXPORT message_type_e get_message_type() const;
    VSOMEIP_EXPORT void set_message_type(message_type_e _type);

    VSOMEIP_EXPORT return_code_e get_return_code() const;
    VSOMEIP_EXPORT void set_return_code(return_code_e _code);

    VSOMEIP_EXPORT bool is_reliable() const;
    VSOMEIP_EXPORT void set_reliable(bool _is_reliable);

    VSOMEIP_EXPORT virtual bool is_initial() const;
    VSOMEIP_EXPORT virtual void set_initial(bool _is_initial);

    VSOMEIP_EXPORT bool is_valid_crc() const;
    VSOMEIP_EXPORT void set_is_valid_crc(bool _is_valid_crc);

    // add to lgsomeip interface
    std::shared_ptr<lgsomeip::Message> get_lgsomeip_message();

protected: // members
    // message_header_impl header_;
    std::shared_ptr<lgsomeip::Message> mlgsomeipMsg = nullptr;
    bool is_reliable_;
    bool is_initial_;
    bool is_valid_crc_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
