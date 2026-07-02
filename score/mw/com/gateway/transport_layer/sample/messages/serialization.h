/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_SERIALIZATION_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_SERIALIZATION_H_

#include "score/mw/com/gateway/transport_layer/sample/messages/message_error.h"

#include <score/span.hpp>

#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace score::mw::com::gateway
{

// Helper to detect std::vector
template <typename T>
struct is_std_vector : std::false_type
{
};

template <typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : std::true_type
{
};

// Helper to detect std::string
template <typename T>
struct is_std_string : std::false_type
{
};

template <>
struct is_std_string<std::string> : std::true_type
{
};

// Tag types for dispatch
struct trivially_copyable_tag
{
};
struct std_vector_tag
{
};
struct std_string_tag
{
};
struct non_trivial_tag
{
};

// Tag selector for Serialize/Deserialize
template <typename T>
constexpr auto serialization_tag()
{
    if constexpr (is_std_string<std::decay_t<T>>::value)
    {
        return std_string_tag{};
    }
    else if constexpr (std::is_trivially_copyable_v<T>)
    {
        return trivially_copyable_tag{};
    }
    else if constexpr (is_std_vector<std::decay_t<T>>::value)
    {
        return std_vector_tag{};
    }
    else
    {
        return non_trivial_tag{};
    }
}

// Main entry point for Serialize
/**
 * @brief Serialize a value of any type into a byte buffer. This function will dispatch to the correct implementation
 * based on the type of the value.
 *
 * The supported types are:
 * - std::string: serialized as a null-terminated string (i.e. the string data followed by a null terminator). The
 *   required buffer size is the string size + 1 (for the null terminator).
 * - Trivially copyable types: serialized as a binary copy of the value. The required buffer size is sizeof the type.
 * - std::vector: serialized as a 16-bit unsigned integer for the number of elements, followed by the serialized
 *   elements. The required buffer size is 2 (for the number of elements) + the sum of the required buffer sizes of the
 *   elements.
 * - Non-trivial, non-vector, non-string types: serialized by calling GetSerializeMembers() to get a tuple of
 *   references to the members that need to be serialized, and then serializing each member in order. The required
 *   buffer size is the sum of the required buffer sizes of the members. If any member fails to serialize (e.g. due to
 *   insufficient buffer size), the entire serialization fails with a buffer too small error.
 *
 * @tparam T type to be serialized
 * @param value value to be serialized
 * @param target_buffer buffer to serialize into
 * @return number of bytes written to the buffer, or an error if the buffer is too small
 */
template <typename T>
score::Result<uint32_t> Serialize(const T& value, score::cpp::span<std::byte> target_buffer);

// Main entry point for Deserialize
/**
 * @brief Deserialize a value of any type from a byte buffer. This function will dispatch to the correct implementation
 * based on the type of the value.
 *
 * The supported types are:
 * - std::string: deserialized from a null-terminated string (i.e. the string data followed by a null terminator).
 * - Trivially copyable types: deserialized as a binary copy of the value.
 * - std::vector: deserialized as a 16-bit unsigned integer for the number of elements, followed by the deserialized
 *   elements.
 * - Non-trivial, non-vector, non-string types: deserialized by calling GetSerializeMembers() to get a tuple of
 *   references to the members that need to be deserialized, and then deserializing each member in order.
 *
 * @tparam T type to be deserialized
 * @param value value to be deserialized
 * @param source_buffer buffer to deserialize from
 * @return number of bytes read from the buffer, or an error if the buffer is too small or invalid
 */
template <typename T>
score::Result<uint32_t> Deserialize(T& value, score::cpp::span<const std::byte> source_buffer);

/**
 * @brief Serialize implementation for std::string.
 * @details See above in the main Serialize function
 */
template <typename T>
score::Result<uint32_t> Serialize(const T& string, score::cpp::span<std::byte> target_buffer, std_string_tag)
{
    const auto required_size = string.size() + 1U;
    if (target_buffer.size() < required_size)
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kBufferTooSmall);
    }

    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): This is no string_view.
    std::memcpy(target_buffer.data(), string.data(), string.size());
    std::memset(&target_buffer[string.size()], 0, 1U);
    return static_cast<uint32_t>(required_size);
}

/**
 * @brief Serialize implementation for trivially copyable types.
 * @details See above in the main Serialize function
 */
template <typename T>
score::Result<uint32_t> Serialize(const T& trivial_copyable,
                                  score::cpp::span<std::byte> target_buffer,
                                  trivially_copyable_tag)
{
    constexpr auto required_size = sizeof(T);
    if (target_buffer.size() < required_size)
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kBufferTooSmall);
    }
    std::memcpy(static_cast<void*>(target_buffer.data()), static_cast<const void*>(&trivial_copyable), sizeof(T));
    return static_cast<uint32_t>(required_size);
}

/**
 * @brief Serialize implementation for std::vector.
 * @details See above in the main Serialize function
 */
template <typename T>
score::Result<uint32_t> Serialize(const T& vector, score::cpp::span<std::byte> target_buffer, std_vector_tag)
{
    std::uint32_t copied_size{0};
    if (vector.size() > std::numeric_limits<std::uint16_t>::max())
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kBufferTooSmall);
    }
    std::uint16_t num_elements = static_cast<std::uint16_t>(vector.size());
    if (target_buffer.size() < sizeof(num_elements))
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kBufferTooSmall);
    }
    std::memcpy(&target_buffer[copied_size], &num_elements, sizeof(num_elements));
    copied_size += static_cast<std::uint32_t>(sizeof(num_elements));
    for (const auto& element : vector)
    {
        auto element_result = Serialize(element, target_buffer.subspan(copied_size));
        if (!element_result)
        {
            return element_result;
        }
        copied_size += element_result.value();
    }
    return copied_size;
}

/**
 * @brief Serialize implementation for non-trivial, non-vector, non-string types.
 * @details See above in the main Serialize function
 */
template <typename T>
score::Result<uint32_t> Serialize(const T& non_trivial_copyable,
                                  score::cpp::span<std::byte> target_buffer,
                                  non_trivial_tag)
{
    auto serializable_members = non_trivial_copyable.GetSerializeMembers();
    std::uint32_t total_written_bytes{0};
    bool error_found = false;
    std::apply(
        [&](auto&... member) {
            (([&] {
                 if (error_found)
                 {
                     return;
                 }
                 auto bytes_written = Serialize(member, target_buffer);
                 if (!bytes_written)
                 {
                     error_found = true;
                     total_written_bytes = 0;
                     return;
                 }
                 target_buffer = target_buffer.subspan(bytes_written.value());
                 total_written_bytes += bytes_written.value();
             }()),
             ...);
        },
        serializable_members);
    if (error_found)
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kBufferTooSmall);
    }
    return total_written_bytes;
}

template <typename T>
score::Result<uint32_t> Serialize(const T& value, score::cpp::span<std::byte> target_buffer)
{
    return Serialize(value, target_buffer, serialization_tag<T>());
}

template <typename T>
std::size_t ComputeSerializedSize(const T& value);

template <typename T>
std::size_t ComputeSerializedSize(const T& value, std_string_tag)
{
    return value.size() + 1U;
}

template <typename T>
std::size_t ComputeSerializedSize(const T&, trivially_copyable_tag)
{
    return sizeof(T);
}

template <typename T>
std::size_t ComputeSerializedSize(const T& vector, std_vector_tag)
{
    std::size_t size = sizeof(std::uint16_t);
    for (const auto& element : vector)
    {
        size += ComputeSerializedSize(element);
    }
    return size;
}

template <typename T>
std::size_t ComputeSerializedSize(const T& non_trivial, non_trivial_tag)
{
    auto members = non_trivial.GetSerializeMembers();
    std::size_t size = 0U;
    std::apply(
        [&](const auto&... member) {
            ((size += ComputeSerializedSize(member)), ...);
        },
        members);
    return size;
}

template <typename T>
std::size_t ComputeSerializedSize(const T& value)
{
    return ComputeSerializedSize(value, serialization_tag<T>());
}

/**
 * @brief Deserialize implementation for std::string.
 * @details See above in the main Deserialize function
 */
template <typename T>
score::Result<uint32_t> Deserialize(T& target_string, score::cpp::span<const std::byte> source_buffer, std_string_tag)
{
    auto string_start = reinterpret_cast<const char*>(source_buffer.data());
    const auto string_len = strnlen(string_start, source_buffer.size());
    auto remaining_size = source_buffer.size();
    if (string_len == remaining_size || string_len == 0)
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kPayloadInvalid);
    }
    target_string.assign(string_start, string_len);
    return static_cast<uint32_t>(string_len + 1U);
}

/**
 * @brief Deserialize implementation for trivially copyable types.
 * @details See above in the main Deserialize function
 */
template <typename T>
score::Result<uint32_t> Deserialize(T& target, score::cpp::span<const std::byte> source_buffer, trivially_copyable_tag)
{
    if (source_buffer.size() < sizeof(T))
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kPayloadInvalid);
    }
    std::memcpy(static_cast<void*>(&target), static_cast<const void*>(source_buffer.data()), sizeof(T));
    return static_cast<uint32_t>(sizeof(T));
}

/**
 * @brief Deserialize implementation for std::vector.
 * @details See above in the main Deserialize function
 */
template <typename T>
score::Result<uint32_t> Deserialize(T& vector, score::cpp::span<const std::byte> source_buffer, std_vector_tag)
{
    if (source_buffer.size() < sizeof(std::uint16_t))
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kPayloadInvalid);
    }
    uint32_t bytes_consumed{0};
    std::uint16_t num_elements{};
    std::memcpy(&num_elements, source_buffer.data(), sizeof(std::uint16_t));
    bytes_consumed += static_cast<uint32_t>(sizeof(std::uint16_t));
    vector.clear();
    vector.reserve(num_elements);
    for (std::uint16_t i = 0; i < num_elements; i++)
    {
        typename T::value_type element{};
        auto element_result = Deserialize(element, source_buffer.subspan(bytes_consumed));
        if (!element_result)
        {
            return element_result;
        }
        bytes_consumed += element_result.value();
        vector.emplace_back(std::move(element));
    }
    return bytes_consumed;
}

/**
 * @brief Deserialize implementation for non-trivial, non-vector, non-string types.
 * @details See above in the main Deserialize function
 */
template <typename T>
score::Result<uint32_t> Deserialize(T& non_trivial_copyable,
                                    score::cpp::span<const std::byte> source_buffer,
                                    non_trivial_tag)
{
    auto deserializable_members = non_trivial_copyable.GetSerializeMembers();
    std::uint32_t total_consumed_bytes{0};
    bool error_found = false;
    std::apply(
        [&](auto&... member) {
            (([&] {
                 if (error_found)
                     return;
                 auto bytes_consumed = Deserialize(member, source_buffer);
                 if (!bytes_consumed)
                 {
                     error_found = true;
                     total_consumed_bytes = 0;
                     return;
                 }
                 source_buffer = source_buffer.subspan(bytes_consumed.value());
                 total_consumed_bytes += bytes_consumed.value();
             }()),
             ...);
        },
        deserializable_members);
    if (error_found)
    {
        return score::MakeUnexpected(score::mw::com::gateway::MessageErrorc::kPayloadInvalid);
    }
    return total_consumed_bytes;
}

template <typename T>
score::Result<uint32_t> Deserialize(T& value, score::cpp::span<const std::byte> source_buffer)
{
    return Deserialize(value, source_buffer, serialization_tag<T>());
}

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_SERIALIZATION_H_
