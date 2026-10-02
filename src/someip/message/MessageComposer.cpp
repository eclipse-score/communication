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

#include <message/MessageComposer.h>

#include <iostream>
#include <exception>

#include <utils/log/logger.h>

namespace lgsomeip {

// MessageComposer public methods.

std::shared_ptr<MessageSD> MessageComposer::add_entry(std::shared_ptr<MessageSD> message, SDEntry* entry,
                                                      SDOption* first_option, SDOption* second_option) {
    std::uint8_t option_index = 0;
    std::uint8_t first_option_count = static_cast<std::uint8_t>(entry->get_option1st_count());
    std::uint8_t first_option_index = 0;

    std::uint8_t second_option_count = 0;
    std::uint8_t second_option_index = 0;

    auto& options = message->options();

    try {
        if (first_option_count == 2) {
            std::uint8_t first_option_position = 0xff;
            std::uint8_t second_option_position = 0xff;
            for (option_index = 0; option_index < options.size(); option_index++) {
                if (options[option_index] == first_option[0]) {
                    first_option_position = option_index;
                } else if (options[option_index] == first_option[1]) {
                    second_option_position = option_index;
                }
            }

            if (first_option_position == 0xff && second_option_position == 0xff) {
                first_option_index = options.size();
                first_option_count = 2;
                options.push_back(first_option[0]);
                options.push_back(first_option[1]);
            } else if (first_option_position == 0xff) {
                first_option_index = options.size();
                first_option_count = 1;
                second_option_index = second_option_position;
                second_option_count = 1;
                options.push_back(first_option[0]);
            } else if (second_option_position == 0xff) {
                first_option_index = first_option_position;
                first_option_count = 1;
                second_option_index = options.size();
                second_option_count = 1;
                options.push_back(first_option[1]);
            } else if (first_option_position != 0xff && second_option_position != 0xff) {
                if (first_option_position + 1 == second_option_position) {
                    first_option_index = first_option_position;
                    first_option_count = 2;
                } else {
                    first_option_index = first_option_position;
                    first_option_count = 1;
                    second_option_index = second_option_position;
                    second_option_count = 1;
                }
            }
        } else if (first_option_count == 1) {
            std::uint8_t first_option_position = 0xff;
            for (option_index = 0; option_index < options.size(); option_index++) {
                if (options[option_index] == first_option[0]) {
                    first_option_position = option_index;
                }
            }

            if (first_option_position == 0xff) {
                first_option_index = options.size();
                first_option_count = 1;
                options.push_back(first_option[0]);
            } else {
                first_option_index = first_option_position;
                first_option_count = 1;
            }
        }
    } catch (const std::exception& e) {
        LGSOMEIP_LOG_ERROR << "MessageComposer::add_entry() gets exception " << e.what();
    }

    entry->set_option1st_index(first_option_index);
    entry->set_option1st_count(first_option_count);
    entry->set_option2nd_index(second_option_index);
    entry->set_option2nd_count(second_option_count);

    message->entries().push_back(*entry);

    return message;
}

} // namespace lgsomeip
