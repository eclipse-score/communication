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

#ifndef LG_SOMEIP_MESSAGE_SERIALIZER_H
#define LG_SOMEIP_MESSAGE_SERIALIZER_H

#include <type_traits>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#include <exception/Exception.h>
#include <utils/byteorder/bytestream.h>

namespace lgsomeip {

class Serializer;
class Deserializer;
class SizeHelper;

// Enumerable type support.
template <typename T, typename Tagged = void> struct IsEnumerable {
    static const bool value = false;
};

template <typename T> struct IsEnumerable<T, typename T::IsEnumerable> {
    static const bool value = true;
};

class Enumerable {
public:
    using IsEnumerable = void;
    virtual void enumerate(Serializer& serializer) = 0;   // Used by Serializer.
    virtual void enumerate(Deserializer& serializer) = 0; // Used by Deserializer.
    virtual std::uint32_t size() const = 0;
};

// Serialization interface.
class Serializer {
public:
    Serializer(bool big_endian = true) : big_endian_(big_endian) {}

    ~Serializer() {}

    // Serialize a complex type derived from Enumerable.
    template <typename T> void push(T data, typename std::enable_if<IsEnumerable<T>::value>::type* = 0);

    // Serialize a trivially copyable, non-enumerable type.
    template <typename T>
    void push(T data,
              typename std::enable_if<!IsEnumerable<T>::value && std::is_trivially_copyable<T>::value>::type* = 0);

    // Serialize a variable-length array.
    template <typename T> void push(const std::vector<T>& data);

    // Serialize a fixed-length array.
    template <typename T, int N> void push(const T data[N]);

    // Serialize a variable-length string.
    template <int N> void push(const std::string& string);

    std::vector<uint8_t>& get_data() {
        return data_;
    }

private:
    bool big_endian_;
    std::vector<uint8_t> data_;
};

// Deserialization interface.
class Deserializer {
public:
    Deserializer(bool big_endian = true) : big_endian_(big_endian), current_position_(0) {}

    ~Deserializer() {}

    // Deserialize a complex type derived from Enumerable.
    template <typename T> void pop(T& data, typename std::enable_if<IsEnumerable<T>::value>::type* = 0);

    // Deserialize a trivially copyable, non-enumerable type.
    template <typename T>
    void pop(T& data,
             typename std::enable_if<!IsEnumerable<T>::value && std::is_trivially_copyable<T>::value>::type* = 0);

    // Deserialize a variable-length array.
    template <typename T> void pop(std::vector<T>& data);

    // Deserialize a fixed-length array.
    template <typename T, int N> void pop(T (&data)[N]);

    // Deserialize a variable-length string.
    template <int N> void pop(std::string& string);

    void set_data(std::vector<uint8_t>& data) {
        data_ = data;
        current_position_ = 0;
    }

private:
    bool big_endian_;
    std::vector<uint8_t> data_;
    std::uint32_t current_position_{0};
};

// Serialized-size calculation.
class SizeHelper {
public:
    template <typename U>
    static typename std::enable_if<!IsEnumerable<U>::value, std::size_t>::type get_type_size(U& type);

    template <typename U>
    static typename std::enable_if<IsEnumerable<U>::value, std::size_t>::type get_type_size(U& type);
};

// Serializer implementation.
// Serialize a complex type derived from Enumerable.
template <typename T> void Serializer::push(T data, typename std::enable_if<IsEnumerable<T>::value>::type*) {
    data.enumerate(*this);
}

// Serialize a trivially copyable, non-enumerable type.
template <typename T>
void Serializer::push(T input_data,
                      typename std::enable_if<!IsEnumerable<T>::value && std::is_trivially_copyable<T>::value>::type*) {
    T converted_data = (big_endian_) ? translate_byte_order(input_data) : input_data;
    std::uint32_t length = sizeof(T);

    std::uint8_t* item = reinterpret_cast<std::uint8_t*>(&converted_data);
    for (std::uint32_t index = 0; index < length; index++)
        data_.push_back(*(item + index));
}

// Serialize a variable-length array.
template <typename T> void Serializer::push(const std::vector<T>& data) {
    std::uint32_t size = 0;
    if (data.size() > 0) {
        size = data.size() * SizeHelper::get_type_size(data[0]);
    }

    push(size);
    for (const T& item : data)
        push(item);
}

// Serialize a fixed-length array.
template <typename T, int N> void Serializer::push(const T data[N]) {
    for (int index = 0; index < N; index++) {
        push(data[index]);
    }
}

// Serialize a string prefixed by a one-, two-, or four-byte length.
// This supports only ASCII strings with one-byte characters.
template <int N> void Serializer::push(const std::string& data) {
    int size = data.size();
    if (N == 1)
        push(static_cast<std::uint8_t>(size));
    else if (N == 2)
        push(static_cast<std::uint16_t>(size));
    else if (N == 4)
        push(static_cast<std::uint32_t>(size));
    else
        throw LSAR_RUNTIME_ERROR("string length is wrong");

    for (auto item : data)
        push(item);
}

// Deserializer implementation.
// Deserialize a complex type derived from Enumerable.
template <typename T> void Deserializer::pop(T& data, typename std::enable_if<IsEnumerable<T>::value>::type*) {
    data.enumerate(*this);
}

// Deserialize a trivially copyable, non-enumerable type.
template <typename T>
void Deserializer::pop(
    T& data, typename std::enable_if<!IsEnumerable<T>::value && std::is_trivially_copyable<T>::value>::type*) {
    std::uint32_t length = sizeof(T);
    get_byte_stream(&data, &data_[current_position_]);
    current_position_ += length;
}

// Deserialize a variable-length array.
template <typename T> void Deserializer::pop(std::vector<T>& data) {
    T item;
    std::uint32_t total_length = 0;
    std::uint32_t unit_length = SizeHelper::get_type_size(item);

    pop(total_length);
    if (total_length % unit_length != 0) {
        LSAR_RUNTIME_ERROR("the length of variable array is wrong");
    }

    while (total_length > 0) {
        pop(item);
        data.push_back(item);
        total_length -= unit_length;
    }
}

// Deserialize a fixed-length array.
template <typename T, int N> void Deserializer::pop(T (&data)[N]) {
    for (int index = 0; index < N; index++) {
        pop(data[index]);
    }
}

// for dynamic length string : N is the length of string (1, 2, 4)
// Only support ASCII(Char size is 1)
template <int N> void Deserializer::pop(std::string& data) {
    std::uint32_t size = 0;
    if (N == 1) {
        std::uint8_t temporary_length;
        pop(temporary_length);
        size = temporary_length;
    } else if (N == 2) {
        std::uint16_t temporary_length;
        pop(temporary_length);
        size = temporary_length;
    } else if (N == 4) {
        std::uint32_t temporary_length;
        pop(temporary_length);
        size = temporary_length;
    } else
        throw LSAR_RUNTIME_ERROR("string length is wrong");

    data.resize(size);
    for (std::uint32_t index = 0; index < size; index++)
        data[index] = data_[current_position_ + index];
    current_position_ += size;
}

// SizeHelper implementation.
// Calculate the serialized size of a data type.
template <typename U>
typename std::enable_if<!IsEnumerable<U>::value, std::size_t>::type SizeHelper::get_type_size(U& type) {
    return sizeof(U);
}

template <typename U>
typename std::enable_if<IsEnumerable<U>::value, std::size_t>::type SizeHelper::get_type_size(U& type) {
    return type.size();
}

} // namespace lgsomeip

#endif // LG_SOMEIP_MESSAGE_SERIALIZER_H
