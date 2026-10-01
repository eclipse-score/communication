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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SD_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SD_H

#include <cstdint>
#include <vector>

#include <message/MessageHeader.h>
#include <message/MessageConstant.h>
#include <message/SDEntry.h>
#include <message/SDOption.h>

#define SOMEIP_REBOOT_FLAG 0x80

namespace lgsomeip {

class MessageSD : public MessageHeader {
public:
    MessageSD();
    virtual ~MessageSD();

    virtual std::uint32_t get_length() const;
    virtual bool is_service_discovery() {
        return true;
    }

    virtual std::uint32_t serialize(std::uint8_t* data);
    virtual bool deserialize(std::uint8_t* data, std::uint32_t len);

    std::uint8_t get_flag() {
        return flags_;
    }
    void set_flag(std::uint8_t flag) {
        flags_ = flag;
    }

    std::vector<SDEntry>& entries();
    SDEntry& entry(int pos);

    std::vector<SDOption>& options();
    SDOption& option(int pos);

    bool get_reboot_flag();

public:
    std::vector<SDEntry> entries_;
    std::vector<SDOption> options_;

    std::uint8_t flags_;
    bool reboot_flag_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_SD_H
