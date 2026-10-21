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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRING_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRING_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_scalar.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_wire_format.h"

#include "score/serialization/serialize_iterator.h"

#include <score/string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace score
{
namespace serialization
{
namespace someip_detail
{

/// @brief The UTF-8 byte order mark (BOM) that prefixes every SOME/IP UTF-8 string on the wire.
constexpr std::array<std::uint8_t, 3U> kUtf8Bom{0xEFU, 0xBBU, 0xBFU};
/// @brief SOME/IP UTF-8 strings are null-terminated on the wire.
constexpr std::uint8_t kStringTerminator{0x00U};

/// @brief SOME/IP UTF-8 string: [length][BOM (EF BB BF)][characters][0x00 terminator]. The length counts all on-wire
/// bytes (BOM + characters + terminator). As a variable-length member, trailing alignment padding is appended when it
/// is not the last element (align_following), aligning the data that follows to the element's alignment_bits. The
/// length-field width comes from the element's layout; characters are written verbatim (the string is assumed UTF-8).
template <typename Container>
void SerializeString(SerializeIterator<Container> iterator,
                     const score::cpp::pmr::string& value,
                     const ElementLayout&          layout,
                     const bool                    align_following)
{
    const std::uint64_t byte_count = kUtf8Bom.size() + value.size() + sizeof(kStringTerminator);  // BOM + characters + terminator
    WriteLengthField(iterator, byte_count, layout);
    for (const std::uint8_t bom_byte : kUtf8Bom)
    {
        SerializeScalar(iterator, bom_byte, layout);
    }
    for (const auto& character : value)
    {
        SerializeScalar(iterator, static_cast<std::uint8_t>(character), layout);
    }
    SerializeScalar(iterator, kStringTerminator, layout);
    if (align_following)
    {
        EmitAlignmentPadding(iterator, layout.alignment_bits);
    }
}

template <typename Visitor, typename Container>
Result<void> DeSerializeString(Visitor&                      visitor,
                               DeSerializeIterator<Container>& iterator,
                               score::cpp::pmr::string&       value,
                               const ElementLayout&          layout,
                               const bool                    align_following)
{
    const std::uint64_t length = ReadLengthField(iterator, layout);

    // Guard against manipulated input declaring more payload bytes than remain, before reserving memory for them.
    const std::size_t remaining = iterator.End().Offset() - iterator.Offset();
    if (length > static_cast<std::uint64_t>(remaining))
    {
        return MakeUnexpected(SerializationError::kOutOfBoundsWrite);
    }

    std::vector<std::uint8_t> bytes{};
    bytes.reserve(length);
    for (std::uint64_t index{0U}; index < length; ++index)
    {
        std::uint8_t byte{};
        visitor.Next(byte);
        bytes.push_back(byte);
    }

    // Strip the leading UTF-8 BOM (if present) and the trailing null terminator; the rest is the string content.
    std::size_t begin{0U};
    if ((bytes.size() >= kUtf8Bom.size()) &&
        std::equal(kUtf8Bom.cbegin(), kUtf8Bom.cend(), bytes.cbegin()))
    {
        begin = kUtf8Bom.size();
    }
    std::size_t end{bytes.size()};
    if ((end > begin) && (bytes[end - 1U] == kStringTerminator))
    {
        --end;
    }
    for (std::size_t index{begin}; index < end; ++index)
    {
        value.push_back(static_cast<char>(bytes[index]));
    }

    if (align_following)
    {
        SkipAlignmentPadding(iterator, layout.alignment_bits);
    }
    return {};
}

}  // namespace someip_detail
}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRING_H
