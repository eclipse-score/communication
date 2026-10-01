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

#include <cstring>
#include <memory>

#include <message/MessageBuilder.h>

namespace lgsomeip {

static const std::uint8_t kSdPreamble[4] = {0xff, 0xff, 0x81, 0x00};

bool MessageBuilder::is_sd_message(std::uint8_t* data, std::uint32_t length) {
    if (data == nullptr || length < SOMEIP_HEADER::SIZE) {
        return false;
    } else if (memcmp(kSdPreamble, data, 4) == 0) {
        return true;
    }

    return false;
}

std::shared_ptr<MessageSOMEIP> MessageBuilder::create_request_message(std::uint16_t service_id, std::uint16_t method_id,
                                                                      std::uint8_t major_version, bool no_response) {
    std::shared_ptr<MessageSOMEIP> message = MessageBuilder::create<SOMEIP>();
    if (message != nullptr) {
        std::uint32_t message_id = (service_id << 16) | ((std::uint32_t)method_id & 0xffff);
        std::uint8_t message_type = SOMEIP_MESSAGE_TYPE::REQUEST;
        if (no_response == true)
            message_type = SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN;

        message->set_message_id(message_id);
        message->set_interface_version(major_version);
        message->set_protocol_version(0x01);
        message->set_request_id(0);
        message->set_return_code(SOMEIP_RETURN_CODE::E_OK);
        message->set_message_type(message_type);
    }

    return message;
}

std::shared_ptr<MessageSOMEIP> MessageBuilder::create_response_message(MessageSOMEIP& message) {
    std::shared_ptr<MessageSOMEIP> response = nullptr;
    if (message.get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
        response = MessageBuilder::create<SOMEIP>();
        response->set_message_id(message.get_message_id());
        response->set_interface_version(message.get_interface_version());
        response->set_protocol_version(message.get_protocol_version());
        response->set_request_id(message.get_request_id());
        response->set_return_code(SOMEIP_RETURN_CODE::E_OK);
        response->set_message_type(SOMEIP_MESSAGE_TYPE::RESPONSE);
    } else {
        // TODO : PRINT Error Message
    }

    return response;
}

std::shared_ptr<MessageSOMEIP> MessageBuilder::create_notification_message(std::uint16_t service_id,
                                                                           std::uint16_t event_id) {
    std::shared_ptr<MessageSOMEIP> message = MessageBuilder::create<SOMEIP>();
    if (message != nullptr) {
        std::uint32_t message_id = (service_id << 16) | ((std::uint32_t)event_id & 0xffff);

        message->set_message_id(message_id);
        message->set_interface_version(0);
        message->set_protocol_version(0x01);
        message->set_request_id(0);
        message->set_return_code(SOMEIP_RETURN_CODE::E_OK);
        message->set_message_type(SOMEIP_MESSAGE_TYPE::NOTIFICATION);
    }

    return message;
}

} // namespace lgsomeip
