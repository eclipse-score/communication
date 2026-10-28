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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEHEADER_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEHEADER_H

#include <cstdint>

namespace lgsomeip {

// Message Header Module

//  SOMEIP Header Format
//  SOMEIP Header Format

//  SOMEIP Header : Message ID
//  SOMEIP Header : Message ID
//  SOMEIP Header : Message ID

//  SOMEIP Header : Length

//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID (Response Message)
//  SOMEIP Header : Request ID

//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID
//  SOMEIP Header : Request ID

//  SOMEIP Header : Protocol Version
//  SOMEIP Header : Protocol Version

//  SOMEIP Header : Interface Version

//  SOMEIP Header : Message Type
//  SOMEIP Header : Message Type
//  SOMEIP Header : Message Type

//  SOMEIP Header : Return Code

//  SOMEIP Header : Event ID
//  SOMEIP Header : Eventgroup ID
//  SOMEIP Header : Eventgroup ID

//  SOMEIP Header : Endianness
//  SOMEIP Header : Endianness

class MessageHeader {
public:
    MessageHeader();
    virtual ~MessageHeader();

    virtual std::uint32_t serialize(std::uint8_t* data);
    virtual bool deserialize(std::uint8_t* data, std::uint32_t len);

    bool is_service_discovery();

    std::uint32_t get_message_id() const;
    virtual std::uint32_t get_length() const;
    std::uint8_t get_message_type() const;
    std::uint8_t get_interface_version() const;
    std::uint8_t get_protocol_version() const;
    std::uint32_t get_request_id() const;
    std::uint8_t get_return_code() const;

    void set_message_id(std::uint32_t service_id);
    void set_length(std::uint32_t length);
    void set_message_type(std::uint8_t type);
    void set_interface_version(std::uint8_t ver);
    void set_protocol_version(std::uint8_t ver);
    void set_request_id(std::uint32_t request_id);
    void set_return_code(std::uint8_t ret_code);

protected:
    std::uint32_t message_id_{0};
    std::uint32_t length_{0};
    std::uint32_t request_id_{0};
    std::uint8_t protocol_version_{0x01};
    std::uint8_t interface_version_{0};
    std::uint8_t return_code_{0};
    std::uint8_t message_type_{0};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEHEADER_H
