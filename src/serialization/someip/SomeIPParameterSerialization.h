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

#ifndef LG_SOMEIP_SOMEIP_PARAMETER_SERIALIZATION_H
#define LG_SOMEIP_SOMEIP_PARAMETER_SERIALIZATION_H

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>
#if defined(LINUX)
#include <endian.h>
#endif // LINUX
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace lgsomeip {
namespace serialization {

static inline bool is_host_network_byte_order(void) {
#if defined(LINUX)
    return !(__BYTE_ORDER == __LITTLE_ENDIAN);
#else
    static unsigned int i = 1;
    return !(*reinterpret_cast<char*>(&i) == 0x01);
#endif // LINUX
}

inline constexpr bool swap_order(bool v) {
    return v;
}

inline constexpr std::uint8_t swap_order(std::uint8_t v) {
    return v;
}

inline constexpr std::int8_t swap_order(std::int8_t v) {
    return v;
}

inline constexpr std::uint16_t swap_order(std::uint16_t v) {
    return static_cast<std::uint16_t>((v << 8) | ((v >> 8) & 0x00fful));
}

inline constexpr std::int16_t swap_order(std::int16_t v) {
    return static_cast<std::int16_t>((v << 8) | ((v >> 8) & 0x00fful));
}

template <typename T> inline T swap_order(T src) {
    T ret = 0;
    std::size_t len = sizeof(T);
    std::uint8_t* spos = reinterpret_cast<std::uint8_t*>(&src);
    std::uint8_t* rpos = reinterpret_cast<std::uint8_t*>(&ret);
    for (std::uint32_t i = 0; i < len; i++) {
        rpos[i] = spos[len - i - 1];
    }

    return ret;
}

template <typename T> inline T to_network_byte_order(T src) {
    return is_host_network_byte_order() ? src : swap_order(src);
}

template <typename T> inline T from_network_byte_order(T src) {
    return is_host_network_byte_order() ? src : swap_order(src);
}

using PacketBuffer = std::vector<std::uint8_t>;

// Serializer.
class Serializer {
private:
    std::unique_ptr<PacketBuffer> buffer_;
    std::size_t size_;

public:
    Serializer() : buffer_{new PacketBuffer}, size_(0) {}
    explicit Serializer(std::unique_ptr<PacketBuffer> buf) : buffer_{std::move(buf)}, size_{buffer_->size()} {}

    // Deliberately undefined so unsupported types fail during instantiation.
    template <typename T> void push_back(T v);

    void push_back(bool v) {
        serialize<bool>(v);
        size_ += 1;
    }

    void push_back(std::uint8_t v) {
        serialize<std::uint8_t>(v);
        size_ += 1;
    }

    void push_back(std::int8_t v) {
        serialize<std::int8_t>(v);
        size_ += 1;
    }

    void push_back(std::uint16_t v) {
        serialize<std::uint16_t>(v);
        size_ += 2;
    }

    void push_back(std::int16_t v) {
        serialize<std::int16_t>(v);
        size_ += 2;
    }

    void push_back(std::uint32_t v) {
        serialize<std::uint32_t>(v);
        size_ += 4;
    }

    void push_back(std::int32_t v) {
        serialize<std::int32_t>(v);
        size_ += 4;
    }

    void push_back(std::uint64_t v) {
        serialize<std::uint64_t>(v);
        size_ += 8;
    }

    void push_back(std::int64_t v) {
        serialize<std::int64_t>(v);
        size_ += 8;
    }

    void push_back(float v) {
        serialize<float>(v);
        size_ += 4;
    }

    void push_back(double v) {
        serialize<double>(v);
        size_ += 8;
    }

    // Write a length field at the specified position.
    void push(std::uint8_t v, std::size_t pos) {
        serialize<std::uint8_t>(v, pos);
    }

    // Write a length field at the specified position.
    void push(std::uint16_t v, std::size_t pos) {
        serialize<std::uint16_t>(v, pos);
    }

    // Write a length field at the specified position.
    void push(std::uint32_t v, std::size_t pos) {
        serialize<std::uint32_t>(v, pos);
    }

    std::size_t get_length() const {
        return size_;
    }

    PacketBuffer& get_buffer() {
        return *buffer_;
    }

    std::unique_ptr<PacketBuffer> finish() {
        return std::move(buffer_);
    }

private:
    // serialize data into buffer
    // It uses back_insert_iterator of the vector
    template <typename DataType> void serialize(DataType data) {
        data = to_network_byte_order(data);
        auto* first = reinterpret_cast<std::uint8_t*>(&data);
        auto* last = first + sizeof(DataType);
        std::copy(first, last, std::back_inserter(*buffer_));
    }

    // serialize data into buffer
    // It uses back_insert_iterator of the vector
    template <typename DataType> void serialize(DataType data, std::size_t pos) {
        data = to_network_byte_order(data);
        auto* first = reinterpret_cast<std::uint8_t*>(&data);
        auto* last = first + sizeof(DataType);
        PacketBuffer::iterator it = (*buffer_).begin();
        std::copy(first, last, it + pos);
    }
};

// An optional length field of 8, 16 or 32 Bit may be inserted in front of the Struct
template <typename T> struct LengthFieldConfig {
    using LengthFieldType = T;
    constexpr static bool lengthFieldActive{true};
};

// Specialized Template
// No length field
template <> struct LengthFieldConfig<void> {
    using LengthFieldType = void;
    constexpr static bool lengthFieldActive{false};
};

// Complex Data Type Serializer such as struct, array
template <typename Config> class ComplexTypeSerializer {
private:
    Serializer* parent_;
    std::size_t length_pos_{0};

public:
    ComplexTypeSerializer(Serializer* parent) : parent_{parent} {
        reserve_length_field();
    }

    template <typename DataType> void push_back(DataType value) {
        parent_->push_back(value);
    }

    std::size_t get_length() const {
        return parent_->get_length();
    }

    PacketBuffer& get_buffer() {
        return parent_->get_buffer();
    }

    void reserve_length_field() {
        if (Config::lengthFieldActive) {
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value,
                                                std::uint8_t, typename Config::LengthFieldType>::type;
            length_pos_ = parent_->get_length();
            T length{0};
            parent_->push_back(length);
        }
    }

    void finish() {
        if (Config::lengthFieldActive) {
            std::size_t len = parent_->get_length() - length_pos_;
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value,
                                                std::uint8_t, typename Config::LengthFieldType>::type;
            // The length field of the struct shall describe the number
            // of bytes this struct occupies for SOME/IP transport
            T length = static_cast<T>(len - sizeof(T));
            parent_->push(length, length_pos_);
        }
    }
};

// Specifies the available wire types for SOME/IP serialization.
enum class WireTypeEnum : std::uint8_t {
    BaseType8Bit = 0x00U,  // 8 Bit Data Base data type
    BaseType16Bit = 0x01U, // 16 Bit Data Base data type
    BaseType32Bit = 0x02U, // 32 Bit Data Base data type
    BaseType64Bit = 0x03U, // 64 Bit Data Base data type

    // Complex Data Type: Array, Struct, String, Union with length field of
    // static size(configured in data definition).
    ComplexDataTypeFromDataDefinition = 0x04U,

    // Complex Data Type: Array, Struct, String, Union
    // with length field size 1 byte (ignore static definition).
    ComplexDataType8BitLengthField = 0x05U,

    // Complex Data Type: Array, Struct, String, Union
    // with length field size 2 byte (ignore static definition).
    ComplexDataType16BitLengthField = 0x06U,

    // Complex Data Type: Array, Struct, String, Union
    // with length field size 4 byte (ignore static definition).
    ComplexDataType32BitLengthField = 0x07U,
};

template <typename Config> class TlvSerializer {
private:
    Serializer* parent_;
    std::size_t length_pos_;
    WireTypeEnum wire_type_;

public:
    using DataIdType = std::uint16_t;

    // The length of a tag shall be two bytes
    TlvSerializer(Serializer* parent, WireTypeEnum type, DataIdType data_id) : parent_(parent), wire_type_(type) {
        const std::uint8_t byte0 = static_cast<std::uint8_t>((static_cast<std::uint8_t>(wire_type_) & 0x07U) << 0x04U) |
                                   static_cast<std::uint8_t>((data_id & 0x0F00U) >> 0x08U);
        const std::uint8_t byte1 = static_cast<std::uint8_t>(data_id & 0xFFU);

        parent_->push_back(byte0);
        parent_->push_back(byte1);
        length_pos_ = parent_->get_length();
        switch (wire_type_) {
        case WireTypeEnum::ComplexDataTypeFromDataDefinition: {
            // We need this conditional here, if there is no length type set (type void) in the data definition.
            // The conditional makes sure, that the source is compile-able, when no length field is set.
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value, uint8_t,
                                                typename Config::LengthFieldType>::type;
            if (Config::lengthFieldActive) {
                T field{};
                parent_->push_back(field);
            }
            break;
        }

        case WireTypeEnum::ComplexDataType8BitLengthField: {
            std::uint8_t length{0};
            parent_->push_back(length);
            break;
        }

        case WireTypeEnum::ComplexDataType16BitLengthField: {
            std::uint16_t length{0};
            parent_->push_back(length);
            break;
        }

        case WireTypeEnum::ComplexDataType32BitLengthField: {
            std::uint32_t length{0};
            parent_->push_back(length);
            break;
        }

        default:
            break;
        }
    }

    void finish() {
        std::size_t len = parent_->get_length() - length_pos_;

        switch (wire_type_) {
        case WireTypeEnum::ComplexDataTypeFromDataDefinition: {
            // We need this conditional here, if there is no length type set (type void) in the data definition.
            // The conditional makes sure, that the source is compile-able, when no length field is set.
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value, uint8_t,
                                                typename Config::LengthFieldType>::type;
            if (Config::lengthFieldActive) {
                const T length = static_cast<T>(len - sizeof(T));
                parent_->push(length, length_pos_);
            }
            break;
        }

        case WireTypeEnum::ComplexDataType8BitLengthField: {
            std::uint8_t length = static_cast<std::uint8_t>(len - 1);
            parent_->push(length, length_pos_);
            break;
        }

        case WireTypeEnum::ComplexDataType16BitLengthField: {
            std::uint16_t length = static_cast<std::uint16_t>(len - 2);
            parent_->push(length, length_pos_);
            break;
        }

        case WireTypeEnum::ComplexDataType32BitLengthField: {
            std::uint32_t length = static_cast<std::uint32_t>(len - 4);
            parent_->push(length, length_pos_);
            break;
        }

        default:
            break;
        }
    }

    template <typename DataType> void push_back(DataType value) {
        parent_->push_back(value);
    }

    std::size_t get_length() const {
        return parent_->get_length();
    }

    PacketBuffer& get_buffer() {
        return parent_->get_buffer();
    }
};

class Deserializer {
private:
    std::unique_ptr<PacketBuffer> buffer_;
    std::uint8_t* pos_;
    std::size_t length_;
    std::size_t bytes_read_{0};

public:
    explicit Deserializer(std::unique_ptr<PacketBuffer> buffer)
        : buffer_{std::move(buffer)}, pos_{buffer_->data()}, length_{buffer_->size()} {}

    template <typename DataType> std::size_t pop_front(DataType& data) {
        return deserialize<DataType>(data);
    }

    PacketBuffer& get_buffer() {
        return *buffer_;
    }

    std::unique_ptr<PacketBuffer> release_buffer() {
        return std::move(buffer_);
    }

    std::size_t get_remaining_length() const {
        return static_cast<std::size_t>(&(*buffer_->end()) - pos_);
    }

private:
    template <typename DataType> std::size_t deserialize(DataType& data) {
        std::size_t nbytes = 0;

        if ((bytes_read_ + sizeof(DataType)) <= length_) {
            data = from_network_byte_order(*reinterpret_cast<DataType*>(&*pos_));
            pos_ += sizeof(DataType);
            bytes_read_ += sizeof(DataType);
            nbytes = sizeof(DataType);
        }

        return nbytes;
    }
};

class ConsumeWatcher {
public:
    virtual void consumed(const std::size_t bytes) = 0;
};

template <typename Config> class ComplexTypeDeserializer : public ConsumeWatcher {
private:
    Deserializer* parent_;
    std::size_t length_;
    std::size_t bytes_read_{0};

    std::size_t consume_length_field() {
        using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value, int,
                                            typename Config::LengthFieldType>::type;
        T length_field = 0;
        const std::size_t nbytes = parent_->pop_front(length_field);

        if (nbytes == 0U)
            length_field = 0U;

        return length_field;
    }

public:
    explicit ComplexTypeDeserializer(Deserializer* parent) : parent_(parent) {
        if (Config::lengthFieldActive) {
            length_ = consume_length_field();
        }
    }

    template <typename DataType> std::size_t pop_front(DataType& data) {
        std::size_t nbytes = 0;

        if (Config::lengthFieldActive) {
            if ((bytes_read_ + sizeof(DataType)) <= length_) {
                nbytes = parent_->pop_front(data);
                bytes_read_ += nbytes;
            }
        } else {
            // If there is no length field active (for instance for structs)
            // we try to pop data for any case.
            nbytes = parent_->pop_front(data);
            bytes_read_ += nbytes;
        }

        return nbytes;
    }

    void consumed(const std::size_t bytes) override {
        if ((bytes_read_ + bytes) <= length_)
            bytes_read_ += bytes;
    }

    bool has_remaining_data() const {
        return bytes_read_ < length_;
    }
};

struct TlvTag {
    std::uint16_t data_id_;
    std::size_t len_;
};

template <typename Config> class TlvDeserializer : public ConsumeWatcher {
private:
    Deserializer* deserializer_;
    ConsumeWatcher* watcher_;
    std::size_t bytes_read_{0};
    std::size_t length_;
    WireTypeEnum wire_type_;

public:
    explicit TlvDeserializer(Deserializer* deserializer, ConsumeWatcher* watcher = NULL)
        : deserializer_(deserializer), watcher_(watcher) {}

    TlvTag consume_tag() {
        std::array<std::uint8_t, 2U> tag;
        deserializer_->pop_front(tag[0]);
        deserializer_->pop_front(tag[1]);
        wire_type_ = static_cast<WireTypeEnum>((tag[0] & 0x70U) >> 8U);
        std::uint16_t data_id = ((tag[0] & 0x0FU) << 16) | tag[1];
        bytes_read_ = 0U;

        if (watcher_)
            watcher_->consumed(2U);

        length_ = consume_length();

        return {data_id, length_};
    }

    template <typename DataType> std::size_t pop_front(DataType& data) {
        std::size_t nbytes = 0;
        if ((bytes_read_ + sizeof(DataType)) <= length_) {
            nbytes = deserializer_->pop_front(data);
            bytes_read_ += nbytes;
            if (watcher_)
                watcher_->consumed(nbytes);
        }

        return nbytes;
    }

    bool has_remaining_data() const {
        return bytes_read_ < length_;
    }

    void consumed(const std::size_t bytes) override {
        if ((bytes_read_ + bytes) <= length_)
            bytes_read_ += bytes;
    }

private:
    std::size_t consume_length() {
        std::size_t len = 0;

        switch (wire_type_) {
        case WireTypeEnum::ComplexDataTypeFromDataDefinition: {
            typename Config::LengthFieldType field;
            if (deserializer_->pop_front(field) > 0U)
                len = field;
            if (watcher_)
                watcher_->consumed(sizeof(typename Config::LengthFieldType));
            break;
        }

        case WireTypeEnum::ComplexDataType8BitLengthField: {
            std::uint8_t field;
            if (deserializer_->pop_front(field) > 0U)
                len = field;
            if (watcher_)
                watcher_->consumed(1U);
            break;
        }

        case WireTypeEnum::ComplexDataType16BitLengthField: {
            std::uint16_t field;
            if (deserializer_->pop_front(field) > 0U)
                len = field;
            if (watcher_)
                watcher_->consumed(2U);
            break;
        }

        case WireTypeEnum::ComplexDataType32BitLengthField: {
            std::uint32_t field{};
            if (deserializer_->pop_front(field) > 0U)
                len = field;
            if (watcher_)
                watcher_->consumed(4U);
            break;
        }

        case WireTypeEnum::BaseType8Bit:
            len = 1U;
            break;

        case WireTypeEnum::BaseType16Bit:
            len = 2U;
            break;

        case WireTypeEnum::BaseType32Bit:
            len = 4U;
            break;

        case WireTypeEnum::BaseType64Bit:
            len = 8U;
            break;

        default:
            break;
        }

        return len;
    }
};

} // namespace serialization
} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_PARAMETER_SERIALIZATION_H
