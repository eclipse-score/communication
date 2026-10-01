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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SOMEIP_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SOMEIP_H

#include <cstdint>
#include <memory>
#include <message/MessageHeader.h>
#include <message/MessageConstant.h>
#include <message/MessagePayload.h>

namespace lgsomeip {

class MessageSOMEIP : public MessageHeader {
public:
    MessageSOMEIP();
    virtual ~MessageSOMEIP();

    virtual bool is_service_discovery() {
        return false;
    }
    virtual std::uint32_t get_length() const;

    virtual std::uint32_t serialize(std::uint8_t* data);
    std::uint32_t serialize_some_ip_header(std::uint8_t* data);
    virtual bool deserialize(std::uint8_t* data, std::uint32_t length);

    void set_payload(std::shared_ptr<MessagePayload> payload);
    void set_payload(std::uint8_t* data, std::uint32_t length);
    void set_instance_id(std::uint16_t instance_id);
    void set_is_valid_crc(bool valid_crc);

    std::shared_ptr<MessagePayload> get_payload_type();
    std::uint8_t* get_payload();
    std::uint16_t get_instance_id() const;
    bool get_is_valid_crc() const;

private:
    std::shared_ptr<MessagePayload> payload_;
    std::uint16_t instance_id_ = 0;
    bool valid_crc_{true};
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SOMEIP_H
