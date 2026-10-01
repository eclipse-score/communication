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

#ifndef LG_SOMEIP_BYTE_ORDER_H
#define LG_SOMEIP_BYTE_ORDER_H

#include <cstdint>

#if defined(LINUX)
#include <endian.h>
#elif defined(QNX)
#include <sys/platform.h>
#endif

namespace lgsomeip {

constexpr bool check_byte_order(void) {
#if defined(LINUX)
    return __BYTE_ORDER == __LITTLE_ENDIAN;
#elif defined(QNX)
#if defined(__LITTLEENDIAN__)
    return true;
#else
    return false;
#endif
#else
    unsigned int index = 1;
    return (*reinterpret_cast<char*>(&index) == 0x01);
#endif
}

inline void set_byte(std::uint8_t* destination, const std::uint8_t* source, std::uint32_t length) {
    if (check_byte_order() == false) {
        for (std::uint32_t index = 0; index < length; index++) {
            destination[index] = source[index];
        }
    } else {
        for (std::uint32_t index = 0; index < length; index++) {
            destination[index] = source[length - index - 1];
        }
    }
}

template <typename T> inline void set_byte_stream(std::uint8_t* destination, T* source) {
    if (check_byte_order()) {
        std::uint8_t* source_bytes = reinterpret_cast<std::uint8_t*>(source);
        std::uint8_t* destination_bytes = reinterpret_cast<std::uint8_t*>(destination);

        set_byte(destination_bytes, source_bytes, sizeof(T));
    } else {
        *reinterpret_cast<T*>(destination) = *source;
    }
}

template <typename T> inline void get_byte_stream(T* destination, std::uint8_t* source) {
    if (check_byte_order()) {
        std::uint8_t* source_bytes = reinterpret_cast<std::uint8_t*>(source);
        std::uint8_t* destination_bytes = reinterpret_cast<std::uint8_t*>(destination);

        set_byte(destination_bytes, source_bytes, sizeof(T));
    } else {
        *destination = *reinterpret_cast<T*>(source);
    }
}

template <typename T> inline void set_byte_stream(std::uint8_t* destination, T* source, std::uint32_t length) {
    std::uint8_t* source_bytes = reinterpret_cast<std::uint8_t*>(source);
    std::uint8_t* destination_bytes = reinterpret_cast<std::uint8_t*>(destination);

    set_byte(destination_bytes, source_bytes, length);
}

template <typename T> inline void get_byte_stream(T* destination, std::uint8_t* source, std::uint32_t length) {
    std::uint8_t* source_bytes = reinterpret_cast<std::uint8_t*>(source);
    std::uint8_t* destination_bytes = reinterpret_cast<std::uint8_t*>(destination);

    set_byte(destination_bytes, source_bytes, length);
}

template <typename T> inline T translate_byte_order(T source) {
    T result;
    std::uint8_t* source_bytes = reinterpret_cast<std::uint8_t*>(&source);
    std::uint8_t* destination_bytes = reinterpret_cast<std::uint8_t*>(&result);

    set_byte(destination_bytes, source_bytes, sizeof(T));

    return result;
}

} // namespace lgsomeip

#endif // LG_SOMEIP_BYTE_ORDER_H
