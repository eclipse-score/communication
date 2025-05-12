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
#include "score/mw/com/message_passing/serializer.h"

#include "score/mw/com/message_passing/shared_properties.h"

#include <score/utility.hpp>

#include <cstring>

namespace
{
void SerializeMessageId(score::mw::com::message_passing::RawMessageBuffer& buffer,
                        const score::mw::com::message_passing::MessageId& message_id)
{
    // Serialization implies operations with raw memory using std::memcpy(). The operand being serialized
    // must comply with TriviallyCopyable which it does. The destination buffer is just a place for bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // We ignore the return of memcpy because we do not need it and does not carry any error information
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&buffer.at(score::mw::com::message_passing::GetMessageIdPosition())),
                              static_cast<const void*>(&message_id),
                              sizeof(message_id));
}
score::mw::com::message_passing::MessageId DeserializeMessageId(
    const score::mw::com::message_passing::RawMessageBuffer& buffer)
{
    score::mw::com::message_passing::MessageId message_id{};
    // Serialization implies operations with raw memory using std::memcpy(). The operand being deserialized
    // must comply with TriviallyCopyable which it does. The source buffer is just raw bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // We ignore the return of memcpy because we do not need it and does not carry any error information
    score::cpp::ignore =
        // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
        std::memcpy(static_cast<void*>(&message_id),
                    static_cast<const void*>(&buffer.at(score::mw::com::message_passing::GetMessageIdPosition())),
                    sizeof(message_id));
    return message_id;
}
}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no overlapping by std::memcopy is guaranteed as we use two different
// identifiers. So no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto score::mw::com::message_passing::SerializeToRawMessage(const ShortMessage& message) noexcept -> RawMessageBuffer
{
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is already used in the next line.
    // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used as the
    // null-pointer-constant". No null pointer is used.
    // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
    // as the null-pointer-constant". No null pointer is used.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
    // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
    constexpr auto MESSAGE_END_PAYLOAD = GetMessageStartPayload() + sizeof(ShortMessage::payload);
    static_assert(MESSAGE_END_PAYLOAD <= std::tuple_size<RawMessageBuffer>::value,
                  "RawMessageBuffer to small for short message, unsafe memory operation!");

    RawMessageBuffer buffer{};
    buffer.at(GetMessageTypePosition()) =
        static_cast<std::underlying_type<MessageType>::type>(MessageType::kShortMessage);

    SerializeMessageId(buffer, message.id);

    // Serialization implies operations with raw memory using std::memcpy(). The operand being serialized
    // must comply with TriviallyCopyable which it does. The destination buffer is just a place for bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // We ignore the return of memcpy because we do not need it and does not carry any error information
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&buffer.at(GetMessagePidPosition())),
                              static_cast<const void*>(&message.pid),
                              sizeof(message.pid));

    // Serialization implies operations with raw memory using std::memcpy(). The operand being serialized
    // must comply with TriviallyCopyable which it does. The destination buffer is just a place for bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&buffer.at(GetMessageStartPayload())),
                              static_cast<const void*>(&message.payload),
                              sizeof(message.payload));

    return buffer;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no overlapping by std::memcopy is guaranteed as we use two different
// identifiers. So no way for calling std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto score::mw::com::message_passing::SerializeToRawMessage(const MediumMessage& message) noexcept -> RawMessageBuffer
{
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is already used in the next line.
    // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used as the
    // null-pointer-constant". No null pointer is used.
    // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
    // as the null-pointer-constant". No null pointer is used.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
    // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
    constexpr auto MESSAGE_END_PAYLOAD = GetMessageStartPayload() + sizeof(MediumMessage::payload);
    static_assert(MESSAGE_END_PAYLOAD <= std::tuple_size<RawMessageBuffer>::value,
                  "RawMessageBuffer to small for medium message, unsafe memory operation!");

    RawMessageBuffer buffer{};
    buffer[GetMessageTypePosition()] =
        static_cast<std::underlying_type<MessageType>::type>(MessageType::kMediumMessage);

    SerializeMessageId(buffer, message.id);

    // memcpy doesn't return error code.
    // Serialization implies operations with raw memory using std::memcpy(). The operand being serialized
    // must comply with TriviallyCopyable which it does. The destination buffer is just a place for bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&buffer.at(GetMessagePidPosition())),
                              static_cast<const void*>(&message.pid),
                              sizeof(message.pid));

    // Serialization implies operations with raw memory using std::memcpy(). The operand being serialized
    // must comply with TriviallyCopyable which it does. The destination buffer is just a place for bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&buffer.at(GetMessageStartPayload())),
                              static_cast<const void*>(&message.payload),
                              sizeof(message.payload));
    return buffer;
}

// Suppress "AUTOSAR C++14 A3-1-1", the rule states: " It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule." DeserializeToShortMessage is completly defined in
// seralizer.coo file and declared in serializer.h file. This is false positive.
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no overlapping by std::memcopy is guaranteed as we use two different
// identifiers. So no way for calling std::terminate().
// coverity[autosar_cpp14_a3_1_1_violation : FALSE]
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto score::mw::com::message_passing::DeserializeToShortMessage(const RawMessageBuffer& buffer) noexcept -> ShortMessage
{
    ShortMessage message{};
    message.id = DeserializeMessageId(buffer);

    // memcpy doesn't return error code.
    // Serialization implies operations with raw memory using std::memcpy(). The operand being deserialized
    // must comply with TriviallyCopyable which it does. The source buffer is just raw bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&message.pid),
                              static_cast<const void*>(&buffer.at(GetMessagePidPosition())),
                              // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used
                              // as the null-pointer-constant". No null pointer is used.
                              // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
                              // as the null-pointer-constant". No null pointer is used.
                              // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
                              // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
                              sizeof(ShortMessage::pid));

    // Serialization implies operations with raw memory using std::memcpy(). The operand being deserialized
    // must comply with TriviallyCopyable which it does. The source buffer is just raw bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&message.payload),
                              static_cast<const void*>(&buffer.at(GetMessageStartPayload())),
                              // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used
                              // as the null-pointer-constant". No null pointer is used.
                              // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
                              // as the null-pointer-constant". No null pointer is used.
                              // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
                              // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
                              sizeof(ShortMessage::payload));
    return message;
}

// Suppress "AUTOSAR C++14 A3-1-1", the rule states: " It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule." DeserializeToMediumMessage is completely defined in
// seralizer.coo file and declared in serializer.h file. This is false positive.
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, no overlapping by std::memcopy is guaranteed as we use two different
// identifiers. So no way for calling std::terminate().
// coverity[autosar_cpp14_a3_1_1_violation : FALSE]
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto score::mw::com::message_passing::DeserializeToMediumMessage(const RawMessageBuffer& buffer) noexcept -> MediumMessage
{
    // Suppress "AUTOSAR C++14 M8-5-2" rule finding. This rule declares: "Braces shall be used to indicate and match
    // the structure in the non-zero initialization of arrays and structures"
    // False positive: AUTOSAR C++14 M-8-5-2 refers to MISRA C++:2008 8-5-2 allows top level brace initialization.
    // We want to make sure that default initialization is always performed.
    // coverity[autosar_cpp14_m8_5_2_violation : FALSE]
    MediumMessage message{};
    message.id = DeserializeMessageId(buffer);

    // memcpy doesn't return error code.
    // Serialization implies operations with raw memory using std::memcpy(). The operand being deserialized
    // must comply with TriviallyCopyable which it does. The source buffer is just raw bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTNEXTLINE(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap
    score::cpp::ignore = std::memcpy(static_cast<void*>(&message.pid),
                              static_cast<const void*>(&buffer.at(GetMessagePidPosition())),
                              // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used
                              // as the null-pointer-constant". No null pointer is used.
                              // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
                              // as the null-pointer-constant". No null pointer is used.
                              // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
                              // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
                              sizeof(MediumMessage::pid));

    // Serialization implies operations with raw memory using std::memcpy(). The operand being deserialized
    // must comply with TriviallyCopyable which it does. The source buffer is just raw bytes.
    // No overlapping is guaranteed by fact that we take addresses of two different identifiers.
    // NOLINTBEGIN(score-banned-function): std::memcpy() of TriviallyCopyable type and no overlap

    // Suppress "AUTOSAR C++14 A12-0-2" The rule states: "Bitwise operations and operations that assume data
    // representation in memory shall not be performed on objects." Serialization and deserialization operations involve
    // copying raw memory using std::memcpy. The object being deserialized (std::array<unsigned char, 16>) is
    // TriviallyCopyable, and thus, safe to copy with memcpy.The source buffer is just raw bytes and this low-level
    // operation is necessary for performance optimization in handling raw data.
    // coverity[autosar_cpp14_a12_0_2_violation]
    score::cpp::ignore = std::memcpy(static_cast<void*>(&message.payload),
                              static_cast<const void*>(&buffer.at(GetMessageStartPayload())),
                              // Suppress "AUTOSAR C++14 A4-10-1", The rule states: "Only nullptr literal shall be used
                              // as the null-pointer-constant". No null pointer is used.
                              // Suppress "AUTOSAR C++14 m4-10-2", The rule states: "Literal zero (0) shall not be used
                              // as the null-pointer-constant". No null pointer is used.
                              // coverity[autosar_cpp14_a4_10_1_violation : FALSE]
                              // coverity[autosar_cpp14_m4_10_2_violation : FALSE]
                              sizeof(MediumMessage::payload));
    // NOLINTEND(score-banned-function): see above for detailed explanation
    return message;
}
