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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEBUILDER_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEBUILDER_H

#include <cstdint>

#include <memory>

#include <message/MessageSD.h>
#include <message/MessageSOMEIP.h>

namespace lgsomeip {

struct SOMEIP {
    typedef MessageSOMEIP type;
};

struct SOMEIPSD {
    typedef MessageSD type;
};

class MessageBuilder {
public:
    static bool is_sd_message(std::uint8_t* data, std::uint32_t length);

    //  Request/Response (4.2.2)
    template <typename MessageType> static std::shared_ptr<typename MessageType::type> create();

    //  Request / Fire & Forgot Communication(4.2.3)
    static std::shared_ptr<MessageSOMEIP> create_request_message(std::uint16_t service_id, std::uint16_t method_id,
                                                                 std::uint8_t major_version = 0xFF,
                                                                 bool no_response = false);

    static std::shared_ptr<MessageSOMEIP> create_response_message(MessageSOMEIP& message);

    //  Notification Event(4.2.4)
    static std::shared_ptr<MessageSOMEIP> create_notification_message(std::uint16_t service_id, std::uint16_t event_id);

    template <typename MessageType>
    static bool build_message(MessageType& message, std::uint8_t* data, std::uint32_t length);

    template <typename MessageType>
    static std::uint32_t build_byte_stream(std::uint8_t* data, std::uint32_t* length, MessageType& message);

    template <typename MessageType>
    static std::uint32_t build_byte_stream_some_ip_header(std::uint8_t* data, std::uint32_t* length,
                                                          MessageType& message);
};

template <typename MessageType> std::shared_ptr<typename MessageType::type> MessageBuilder::create() {
    return std::make_shared<typename MessageType::type>();
}

template <typename MessageType>
bool MessageBuilder::build_message(MessageType& message, std::uint8_t* data, std::uint32_t length) {
    return message.deserialize(data, length);
}

template <typename MessageType>
std::uint32_t MessageBuilder::build_byte_stream(std::uint8_t* data, std::uint32_t* length, MessageType& message) {
    *length = message.serialize(data);
    return *length;
}

template <typename MessageType>
std::uint32_t MessageBuilder::build_byte_stream_some_ip_header(std::uint8_t* data, std::uint32_t* length,
                                                               MessageType& message) {
    *length = message.serialize_some_ip_header(data);
    return *length;
}

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEBUILDER_H
