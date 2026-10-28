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

#ifndef LG_SOMEIP_UTILS_LOG_FORMATLOG_H
#define LG_SOMEIP_UTILS_LOG_FORMATLOG_H

#include <cstddef>
#include <cstdint>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <utils/log/logger.h>

namespace lgsomeip {

template <typename T> inline std::string message_id_to_string(T id, int length) {
    std::stringstream logstream;
    logstream << std::hex << std::setw(length) << std::setfill('0');
    logstream << static_cast<std::uint64_t>(id);
    logstream << std::dec;
    return logstream.str();
}

inline std::string format_named_id(const char* name, std::uint32_t id, int length) {
    return std::string(name) + "[" + message_id_to_string(id, length) + "]";
}

inline std::string format_service_instance_id(std::uint16_t service_id, std::uint16_t instance_id) {
    return "[" + message_id_to_string(service_id, 4) + ":" + message_id_to_string(instance_id, 4) + "]";
}

inline std::string format_service_instance_event_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                    std::uint16_t member_id) {
    return "[" + message_id_to_string(service_id, 4) + ":" + message_id_to_string(instance_id, 4) + "." +
           message_id_to_string(member_id, 4) + "]";
}

inline std::string format_service_instance_interface_version(std::uint16_t service_id, std::uint16_t instance_id,
                                                             std::uint8_t major_version, std::uint32_t minor_version) {
    return "[" + message_id_to_string(service_id, 4) + ":" + message_id_to_string(instance_id, 4) +
           "] InterfaceVersion[" + message_id_to_string(major_version, 2) + "." +
           message_id_to_string(minor_version, 8) + "]";
}

inline std::string format_service_instance_interface_major_version(std::uint16_t service_id, std::uint16_t instance_id,
                                                                   std::uint8_t major_version) {
    return "[" + message_id_to_string(service_id, 4) + ":" + message_id_to_string(instance_id, 4) +
           "] InterfaceMajorVersion[" + message_id_to_string(major_version, 2) + "]";
}

#define MSGID_FORMAT2(ID) message_id_to_string(ID, 2)
#define MSGID_FORMAT4(ID) message_id_to_string(ID, 4)
#define MSGID_FORMAT6(ID) message_id_to_string(ID, 6)
#define MSGID_FORMAT8(ID) message_id_to_string(ID, 8)

constexpr std::size_t kMaxByteMessageDumpLength = 256;

inline void print_byte_message(std::string head, const std::uint8_t* message, std::size_t message_length) {
    LGSOMEIP_LOG_DEBUG << head << "(PRINT_BYTE_MESSAGE) / len = " << message_length;

    if (!lgsomeip::Logger::instance().is_enabled(lgsomeip::Logger::LogLevel::Verbose) || message == nullptr) {
        return;
    }

    const std::size_t dump_length =
        (message_length < kMaxByteMessageDumpLength) ? message_length : kMaxByteMessageDumpLength;
    std::stringstream output;
    output << "\n  0                    8  9                   16 \n";
    output << "----------------------------------------------- \n";
    std::stringstream buffer;
    for (std::size_t index = 0; index < dump_length; index++) {
        buffer << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(message[index]) << std::dec << " ";
        if ((index + 1) % 16 == 0) {
            output << buffer.str() << "\n";
            buffer.clear();
            buffer.str("");
        }
    }
    output << buffer.str() << "\n";
    if (dump_length < message_length) {
        output << "... byte dump truncated at " << dump_length << " bytes\n";
    }
    LGSOMEIP_LOG_VERBOSE << output.str();
}

} // namespace lgsomeip

#endif // LG_SOMEIP_UTILS_LOG_FORMATLOG_H
