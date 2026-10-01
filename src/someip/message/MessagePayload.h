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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEPAYLOAD_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEPAYLOAD_H

#include <cstdint>
#include <vector>
#include <utils/byteorder/bytestream.h>
#include <iostream>
#include <exception/Exception.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace lgsomeip {

// Message Payload Module

class MessagePayload {
public:
    MessagePayload(){};
    virtual ~MessagePayload(){};

    template <typename Type> void append(Type data, bool to_big_order = true);
    void append(const std::uint8_t* data, std::uint32_t length);
    void append(const std::vector<std::uint8_t>& data);

    void set_capacity(std::uint32_t size);
    void set_payload(const MessagePayload& payload);
    void set_payload(const std::uint8_t* data, std::uint32_t length);
    void set_payload(const std::vector<std::uint8_t>& data);

    const std::uint8_t* get_payload() const;
    std::uint8_t* get_payload();
    std::vector<std::uint8_t>& get_payload_vector();

    std::uint32_t get_length() const;

    bool operator==(const MessagePayload& rhs);

private:
    std::vector<std::uint8_t> payload_;
};

template <typename Type> void MessagePayload::append(Type data, bool to_big_order) {
    std::uint16_t size = sizeof(Type);

    if (to_big_order == true) {
        print_byte_message("MessagePayload::append", reinterpret_cast<std::uint8_t*>(&data), size);
        Type reversed_data = translate_byte_order(data);
        append(static_cast<const std::uint8_t*>(reinterpret_cast<std::uint8_t*>(&reversed_data)), size);
    } else {
        append(static_cast<const std::uint8_t*>(reinterpret_cast<std::uint8_t*>(&data)), size);
    }
}

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGEPAYLOAD_H
