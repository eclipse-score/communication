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

#include "Helper.h"

#include <unistd.h>

void create_msg(char* message, std::size_t max_length) {
    if (message == nullptr || max_length == 0) {
        return;
    }

    for (std::size_t index = 0; index + 1 < max_length; index++) {
        if (index % 2)
            message[index] = '1';
        else
            message[index] = '0';
    }
    message[max_length - 1] = 0;
}

std::uint16_t get_test_port(std::uint16_t offset) {
    constexpr std::uint16_t kTestPortBase = 20000;
    constexpr std::uint16_t kTestPortRange = 20000;
    return static_cast<std::uint16_t>(kTestPortBase + (static_cast<unsigned long>(getpid()) % kTestPortRange) + offset);
}

std::string get_test_socket_path(const char* name) {
    return std::string("/tmp/someip/") + name + "-" + std::to_string(getpid());
}
