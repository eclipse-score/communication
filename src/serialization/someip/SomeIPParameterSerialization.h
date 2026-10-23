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
#include <cstring>
#include <algorithm>
#include <array>
#if defined(LINUX)
#include <endian.h>
#endif // LINUX
#include <iterator>
#include <limits>
#include <memory>
#include <string>
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

// Write a length into a length field that was reserved at pos. Returns false and leaves the field
// untouched when the value does not fit into T.
template <typename T> bool push_length(Serializer& serializer, std::size_t value, std::size_t pos) {
    if (static_cast<std::uint64_t>(value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        return false;
    }
    serializer.push(static_cast<T>(value), pos);
    return true;
}

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

    // Returns false when the content is too long for the length field. The field is left at 0 then.
    bool finish() {
        if (Config::lengthFieldActive) {
            std::size_t len = parent_->get_length() - length_pos_;
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value,
                                                std::uint8_t, typename Config::LengthFieldType>::type;
            // The length field of the struct shall describe the number
            // of bytes this struct occupies for SOME/IP transport
            return push_length<T>(*parent_, len - sizeof(T), length_pos_);
        }
        return true;
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

    // Returns false when the content is too long for the length field. The field is left at 0 then.
    bool finish() {
        std::size_t len = parent_->get_length() - length_pos_;

        switch (wire_type_) {
        case WireTypeEnum::ComplexDataTypeFromDataDefinition: {
            // We need this conditional here, if there is no length type set (type void) in the data definition.
            // The conditional makes sure, that the source is compile-able, when no length field is set.
            using T = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value, uint8_t,
                                                typename Config::LengthFieldType>::type;
            if (Config::lengthFieldActive) {
                return push_length<T>(*parent_, len - sizeof(T), length_pos_);
            }
            break;
        }

        case WireTypeEnum::ComplexDataType8BitLengthField:
            return push_length<std::uint8_t>(*parent_, len - 1, length_pos_);

        case WireTypeEnum::ComplexDataType16BitLengthField:
            return push_length<std::uint16_t>(*parent_, len - 2, length_pos_);

        case WireTypeEnum::ComplexDataType32BitLengthField:
            return push_length<std::uint32_t>(*parent_, len - 4, length_pos_);

        default:
            break;
        }
        return true;
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

    // Only the lowest bit of a boolean is used; the other bits are reserved and ignored on receiving.
    std::size_t pop_front(bool& data) {
        std::uint8_t raw{};
        const std::size_t nbytes = deserialize<std::uint8_t>(raw);
        if (nbytes != 0U) {
            data = (raw & 0x01U) != 0U;
        }
        return nbytes;
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

    // Skip nbytes. Returns nbytes, or 0 without moving when fewer bytes are left.
    std::size_t skip(const std::size_t nbytes) {
        if (nbytes > length_ - bytes_read_) {
            return 0;
        }
        pos_ += nbytes;
        bytes_read_ += nbytes;
        return nbytes;
    }

private:
    template <typename DataType> std::size_t deserialize(DataType& data) {
        std::size_t nbytes = 0;

        if ((bytes_read_ + sizeof(DataType)) <= length_) {
            // The data may start at any offset, so it must not be read through a typed pointer.
            DataType value;
            std::memcpy(&value, pos_, sizeof(DataType));
            data = from_network_byte_order(value);
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
    std::size_t length_{0};
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

    // Skip the bytes of this struct that the caller does not know (feat_req_someip_601). Returns false
    // when the announced bytes are missing. Without a length field there is nothing to skip.
    bool skip_remaining() {
        if (!Config::lengthFieldActive) {
            return true;
        }
        const std::size_t nbytes = length_ - bytes_read_;
        if (nbytes != 0U && parent_->skip(nbytes) == 0U) {
            return false;
        }
        bytes_read_ += nbytes;
        return true;
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
    std::size_t length_{0};
    WireTypeEnum wire_type_{WireTypeEnum::BaseType8Bit};

public:
    explicit TlvDeserializer(Deserializer* deserializer, ConsumeWatcher* watcher = NULL)
        : deserializer_(deserializer), watcher_(watcher) {}

    TlvTag consume_tag() {
        std::array<std::uint8_t, 2U> tag{};
        deserializer_->pop_front(tag[0]);
        deserializer_->pop_front(tag[1]);
        wire_type_ = static_cast<WireTypeEnum>((tag[0] & 0x70U) >> 4U);
        std::uint16_t data_id = static_cast<std::uint16_t>(((tag[0] & 0x0FU) << 8) | tag[1]);
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

    // Skip the rest of the current member, e.g. a member with an unknown data ID. Returns false when
    // the announced bytes are missing.
    bool skip_remaining() {
        const std::size_t nbytes = length_ - bytes_read_;
        if (nbytes != 0U && deserializer_->skip(nbytes) == 0U) {
            return false;
        }
        bytes_read_ += nbytes;
        if (watcher_ && nbytes != 0U)
            watcher_->consumed(nbytes);
        return true;
    }

    void consumed(const std::size_t bytes) override {
        if ((bytes_read_ + bytes) <= length_) {
            bytes_read_ += bytes;
            // A member nested in this one is part of the member that contains this one, too.
            if (watcher_)
                watcher_->consumed(bytes);
        }
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

// ---------------------------------------------------------------------------
// Strings (feat_req_someip_233 ... 239, 581, 582, 562, 639 ... 642, 662, 666, 687, 800)
//
// A string starts with a Byte Order Mark (BOM), which counts towards its length, and ends with a
// terminating zero (one byte for UTF-8, two bytes for UTF-16). A dynamic length string has a length
// field in front, which does not count itself. A fixed length string has no length field; its size is
// given by the interface specification and unused space is filled with zeros.
//
// The text is passed as UTF-8 in a std::string, or as UTF-16 code units in a std::u16string. The
// functions do not convert between encodings. The functions that read a string return the number of
// bytes they consumed (0 on error); a struct that contains the string must report this number to its
// ComplexTypeDeserializer with consumed(). On error the read position is undefined.
// ---------------------------------------------------------------------------
enum class StringEncoding : std::uint8_t {
    Utf8,
    Utf16BE,
    Utf16LE,
};

namespace detail {

constexpr std::uint8_t kUtf8Bom[3] = {0xEF, 0xBB, 0xBF};

inline bool encode_utf8(const std::string& text, PacketBuffer& body) {
    body.insert(body.end(), std::begin(kUtf8Bom), std::end(kUtf8Bom));
    for (const char c : text) {
        if (c == '\0') {
            return false;
        }
        body.push_back(static_cast<std::uint8_t>(c));
    }
    body.push_back(0x00U);
    return true;
}

inline void push_utf16_unit(PacketBuffer& body, std::uint16_t unit, StringEncoding encoding) {
    const std::uint8_t high = static_cast<std::uint8_t>(unit >> 8U);
    const std::uint8_t low = static_cast<std::uint8_t>(unit & 0xFFU);
    if (encoding == StringEncoding::Utf16BE) {
        body.push_back(high);
        body.push_back(low);
    } else {
        body.push_back(low);
        body.push_back(high);
    }
}

inline bool encode_utf16(const std::u16string& text, StringEncoding encoding, PacketBuffer& body) {
    if (encoding != StringEncoding::Utf16BE && encoding != StringEncoding::Utf16LE) {
        return false;
    }
    push_utf16_unit(body, 0xFEFFU, encoding);
    for (const char16_t unit : text) {
        if (unit == 0U) {
            return false;
        }
        push_utf16_unit(body, static_cast<std::uint16_t>(unit), encoding);
    }
    push_utf16_unit(body, 0x0000U, encoding);
    return true;
}

// Everything behind the first zero must be zero, too (terminator and padding).
inline bool decode_utf8(const PacketBuffer& body, std::string& text) {
    const std::size_t n = body.size();
    if (n < 4U || !std::equal(std::begin(kUtf8Bom), std::end(kUtf8Bom), body.begin()) || body[n - 1U] != 0U) {
        return false;
    }
    std::size_t end = 3U;
    while (body[end] != 0U) {
        end++;
    }
    for (std::size_t i = end; i < n; i++) {
        if (body[i] != 0U) {
            return false;
        }
    }
    text.assign(body.begin() + 3, body.begin() + static_cast<std::ptrdiff_t>(end));
    return true;
}

inline bool decode_utf16(const PacketBuffer& body, StringEncoding encoding, std::u16string& text) {
    if (encoding != StringEncoding::Utf16BE && encoding != StringEncoding::Utf16LE) {
        return false;
    }
    // feat_req_someip_641: the last byte of an odd length is ignored.
    const std::size_t n = body.size() & ~static_cast<std::size_t>(1U);
    if (n < 4U) {
        return false;
    }
    const auto unit_at = [&](std::size_t i) -> std::uint16_t {
        const std::uint8_t first = body[i];
        const std::uint8_t second = body[i + 1U];
        return encoding == StringEncoding::Utf16BE ? static_cast<std::uint16_t>((first << 8U) | second)
                                                   : static_cast<std::uint16_t>((second << 8U) | first);
    };
    if (unit_at(0U) != 0xFEFFU || unit_at(n - 2U) != 0U) {
        return false;
    }
    std::size_t end = 2U;
    while (unit_at(end) != 0U) {
        end += 2U;
    }
    for (std::size_t i = end; i < n; i += 2U) {
        if (unit_at(i) != 0U) {
            return false;
        }
    }
    text.clear();
    for (std::size_t i = 2U; i < end; i += 2U) {
        text.push_back(static_cast<char16_t>(unit_at(i)));
    }
    return true;
}

inline bool pop_bytes(Deserializer& deserializer, std::size_t size, PacketBuffer& body) {
    if (size > deserializer.get_remaining_length()) {
        return false;
    }
    body.resize(size);
    for (std::size_t i = 0U; i < size; i++) {
        if (deserializer.pop_front(body[i]) != 1U) {
            return false;
        }
    }
    return true;
}

inline void push_bytes(Serializer& serializer, const PacketBuffer& body) {
    for (const std::uint8_t byte : body) {
        serializer.push_back(byte);
    }
}

template <typename Config> bool push_dynamic(Serializer& serializer, const PacketBuffer& body) {
    static_assert(Config::lengthFieldActive, "A dynamic length string needs a length field");
    ComplexTypeSerializer<Config> string_serializer{&serializer};
    push_bytes(serializer, body);
    return string_serializer.finish();
}

inline bool push_fixed(Serializer& serializer, std::size_t size, PacketBuffer& body, std::size_t unit) {
    if (size % unit != 0U || body.size() > size) {
        return false;
    }
    body.resize(size, 0x00U);
    push_bytes(serializer, body);
    return true;
}

template <typename Config> std::size_t pop_dynamic(Deserializer& deserializer, PacketBuffer& body) {
    static_assert(Config::lengthFieldActive, "A dynamic length string needs a length field");
    typename Config::LengthFieldType length{};
    if (deserializer.pop_front(length) != sizeof(length) || !pop_bytes(deserializer, length, body)) {
        return 0U;
    }
    return sizeof(length) + static_cast<std::size_t>(length);
}

} // namespace detail

// Write a dynamic length string. Returns false when the string cannot be encoded or does not fit into the
// length field; nothing may be sent then.
template <typename Config = LengthFieldConfig<std::uint32_t>>
bool push_back_string(Serializer& serializer, const std::string& text) {
    PacketBuffer body;
    return detail::encode_utf8(text, body) && detail::push_dynamic<Config>(serializer, body);
}

template <typename Config = LengthFieldConfig<std::uint32_t>>
bool push_back_string(Serializer& serializer, const std::u16string& text, StringEncoding encoding) {
    PacketBuffer body;
    return detail::encode_utf16(text, encoding, body) && detail::push_dynamic<Config>(serializer, body);
}

// Write a fixed length string of size bytes (BOM and terminator included). Returns false when the string
// does not fit, or when size is odd for UTF-16.
inline bool push_back_fixed_string(Serializer& serializer, std::size_t size, const std::string& text) {
    PacketBuffer body;
    return detail::encode_utf8(text, body) && detail::push_fixed(serializer, size, body, 1U);
}

inline bool push_back_fixed_string(Serializer& serializer, std::size_t size, const std::u16string& text,
                                   StringEncoding encoding) {
    PacketBuffer body;
    return detail::encode_utf16(text, encoding, body) && detail::push_fixed(serializer, size, body, 2U);
}

// Read a dynamic length string. Returns the bytes consumed including the length field, or 0 on error: a missing
// or wrong BOM, a missing terminator, or a length that exceeds the data.
template <typename Config = LengthFieldConfig<std::uint32_t>>
std::size_t pop_front_string(Deserializer& deserializer, std::string& text) {
    PacketBuffer body;
    const std::size_t consumed = detail::pop_dynamic<Config>(deserializer, body);
    return (consumed != 0U && detail::decode_utf8(body, text)) ? consumed : 0U;
}

template <typename Config = LengthFieldConfig<std::uint32_t>>
std::size_t pop_front_string(Deserializer& deserializer, std::u16string& text, StringEncoding encoding) {
    PacketBuffer body;
    const std::size_t consumed = detail::pop_dynamic<Config>(deserializer, body);
    return (consumed != 0U && detail::decode_utf16(body, encoding, text)) ? consumed : 0U;
}

// Read a fixed length string of size bytes. Returns size, or 0 on error.
inline std::size_t pop_front_fixed_string(Deserializer& deserializer, std::size_t size, std::string& text) {
    PacketBuffer body;
    return (size != 0U && detail::pop_bytes(deserializer, size, body) && detail::decode_utf8(body, text)) ? size : 0U;
}

inline std::size_t pop_front_fixed_string(Deserializer& deserializer, std::size_t size, std::u16string& text,
                                          StringEncoding encoding) {
    PacketBuffer body;
    return (size != 0U && detail::pop_bytes(deserializer, size, body) && detail::decode_utf16(body, encoding, text))
               ? size
               : 0U;
}

// ---------------------------------------------------------------------------
// Union (feat_req_someip_263 ... 274, 563 ... 573, 583)
//
// A union consists of an optional length field, a type field and the selected element. The length field
// counts the element and its padding, but neither the length nor the type field. The order of the two fields
// can be configured; the default is the length field first. A type of 0 is NULL, i.e. the union holds no
// element; the other types start at 1 and are defined by the interface specification.
//
// An element that is shorter than the longest type of the union has to be padded with zeros
// (feat_req_someip_583). Without a length field all types need the same length, so the padded length
// must be given to finish() and skip_remaining().
// ---------------------------------------------------------------------------
enum class UnionFieldOrder : std::uint8_t {
    LengthFirst,
    TypeFirst,
};

namespace detail {

template <typename Config>
using LengthFieldStorage = typename std::conditional<std::is_same<typename Config::LengthFieldType, void>::value,
                                                     std::uint8_t, typename Config::LengthFieldType>::type;

} // namespace detail

template <typename Config = LengthFieldConfig<std::uint32_t>, typename TypeField = std::uint32_t,
          UnionFieldOrder Order = UnionFieldOrder::LengthFirst>
class UnionSerializer {
    static_assert(std::is_unsigned<TypeField>::value && sizeof(TypeField) <= 4U, "The type field has 8, 16 or 32 bit");

private:
    using LengthType = detail::LengthFieldStorage<Config>;

    Serializer* parent_;
    std::size_t length_pos_{0};
    std::size_t element_pos_{0};

public:
    UnionSerializer(Serializer* parent, TypeField type) : parent_{parent} {
        if (Order == UnionFieldOrder::TypeFirst) {
            parent_->push_back(type);
        }
        if (Config::lengthFieldActive) {
            length_pos_ = parent_->get_length();
            parent_->push_back(static_cast<LengthType>(0));
        }
        if (Order == UnionFieldOrder::LengthFirst) {
            parent_->push_back(type);
        }
        element_pos_ = parent_->get_length();
    }

    template <typename DataType> void push_back(DataType value) {
        parent_->push_back(value);
    }

    // Pads the element with zeros up to padded_length (0: no padding) and writes the length field. Returns
    // false when the element is longer than padded_length, or too long for the length field.
    bool finish(std::size_t padded_length = 0) {
        std::size_t length = parent_->get_length() - element_pos_;
        if (padded_length != 0U && length > padded_length) {
            return false;
        }
        for (; length < padded_length; length++) {
            parent_->push_back(static_cast<std::uint8_t>(0));
        }
        if (!Config::lengthFieldActive) {
            return true;
        }
        return push_length<LengthType>(*parent_, length, length_pos_);
    }
};

template <typename Config = LengthFieldConfig<std::uint32_t>, typename TypeField = std::uint32_t,
          UnionFieldOrder Order = UnionFieldOrder::LengthFirst>
class UnionDeserializer : public ConsumeWatcher {
    static_assert(std::is_unsigned<TypeField>::value && sizeof(TypeField) <= 4U, "The type field has 8, 16 or 32 bit");

private:
    using LengthType = detail::LengthFieldStorage<Config>;

    Deserializer* parent_;
    TypeField type_{0};
    std::size_t length_{0};
    std::size_t bytes_read_{0};
    bool valid_{true};

    void read_type() {
        valid_ = valid_ && parent_->pop_front(type_) == sizeof(TypeField);
    }

    void read_length() {
        LengthType field{};
        if (valid_ && parent_->pop_front(field) == sizeof(LengthType)) {
            length_ = field;
        } else {
            valid_ = false;
        }
    }

public:
    explicit UnionDeserializer(Deserializer* parent) : parent_(parent) {
        if (Order == UnionFieldOrder::TypeFirst) {
            read_type();
        }
        if (Config::lengthFieldActive) {
            read_length();
        }
        if (Order == UnionFieldOrder::LengthFirst) {
            read_type();
        }
    }

    // False when the length or type field is missing.
    bool valid() const {
        return valid_;
    }

    // The type of the element; 0 is NULL.
    TypeField type() const {
        return type_;
    }

    template <typename DataType> std::size_t pop_front(DataType& data) {
        std::size_t nbytes = 0;

        if (valid_ && (!Config::lengthFieldActive || (bytes_read_ + sizeof(DataType)) <= length_)) {
            nbytes = parent_->pop_front(data);
            bytes_read_ += nbytes;
        }

        return nbytes;
    }

    void consumed(const std::size_t bytes) override {
        if (!Config::lengthFieldActive || (bytes_read_ + bytes) <= length_) {
            bytes_read_ += bytes;
        }
    }

    bool has_remaining_data() const {
        return valid_ && bytes_read_ < length_;
    }

    // Skip the padding of the element (feat_req_someip_274), or the whole element of an unknown type.
    // Without a length field, padded_length is the length of the element including padding. Returns false
    // when the bytes to skip are missing.
    bool skip_remaining(std::size_t padded_length = 0) {
        if (!valid_) {
            return false;
        }
        const std::size_t total = Config::lengthFieldActive ? length_ : padded_length;
        const std::size_t nbytes = total > bytes_read_ ? total - bytes_read_ : 0U;
        if (nbytes != 0U && parent_->skip(nbytes) == 0U) {
            return false;
        }
        bytes_read_ += nbytes;
        return true;
    }
};

} // namespace serialization
} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_PARAMETER_SERIALIZATION_H
