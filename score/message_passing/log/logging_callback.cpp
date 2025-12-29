/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/message_passing/log/logging_callback.h"

#include <iostream>
#include <mutex>

namespace score::message_passing
{

LoggingCallback GetCerrLogger()
{
    return [](LogSeverity /*severity*/, LogItems items) -> void {
        static std::mutex cerr_mutex;
        std::lock_guard<std::mutex> guard(cerr_mutex);
        for (auto& item : items)
        {
            std::visit(
                [](auto&& arg) {
                    std::cerr << arg;
                },
                item);
        }
        // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call
        // the function or it shall be preceded by &". Passing std::endl to std::cout object with the stream
        // operator follows the idiomatic way that both features in conjunction were designed in the C++
        // standard.
        // coverity[autosar_cpp14_m8_4_4_violation : FALSE] Ticket-219101
        std::cerr << std::endl;
    };
}

}  // namespace score::message_passing
