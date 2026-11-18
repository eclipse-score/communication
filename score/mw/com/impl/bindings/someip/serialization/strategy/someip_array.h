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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_ARRAY_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_ARRAY_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_scalar.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_wire_format.h"

#include "score/serialization/serialize_iterator.h"
#include "score/serialization/types/length_prefixed_vector.h"

#include <score/static_vector.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace score
{
namespace serialization
{
namespace someip_detail
{

/// @brief Writes [length][element_0 ... element_{n-1}] where the length is the byte size of the serialized elements
/// including padding between them.
template <typename Visitor, typename Container, typename Range>
void SerializeArray(Visitor& visitor,
                    SerializeIterator<Container> iterator,
                    const Range& value,
                    const ElementLayout& layout,
                    const bool align_following)
{
    const auto length_offset = iterator.GetResult();
    WriteLengthField(iterator, 0U, layout);  // placeholder: the byte length is only known after the elements
    const auto data_begin = iterator.GetResult();
    const std::size_t count = value.size();
    std::size_t index{0U};
    for (const auto& element : value)
    {
        visitor.PushArrayElement((index + 1U) == count);
        visitor.Next(element);  // route each element back through the visitor (handles nested types)
        visitor.PopField();
        ++index;
    }
    const auto data_end = iterator.GetResult();
    if (length_offset.has_value() && data_begin.has_value() && data_end.has_value())
    {
        PatchLengthField(
            iterator, length_offset.value(), static_cast<std::uint64_t>(data_end.value() - data_begin.value()), layout);
    }
    if (align_following)
    {
        EmitAlignmentPadding(iterator, layout.alignment_bits);
    }
}

/// @brief Reads elements until the byte length from the length field is consumed; at most `capacity` elements.
template <typename Element, typename Visitor, typename Container, typename Append>
Result<void> DeSerializeArray(Visitor& visitor,
                              DeSerializeIterator<Container>& iterator,
                              const ElementLayout& layout,
                              const bool align_following,
                              const std::size_t capacity,
                              Append append)
{
    const std::uint64_t byte_length = ReadLengthField(iterator, layout);
    if (iterator.HasError())
    {
        return MakeUnexpected(SerializationError::kOutOfBoundsWrite);
    }
    const std::size_t data_begin = iterator.GetResult().value();
    const std::size_t remaining = iterator.End().GetResult().value() - data_begin;
    // Guard against manipulated input declaring more bytes than remain.
    if (byte_length > static_cast<std::uint64_t>(remaining))
    {
        return MakeUnexpected(SerializationError::kOutOfBoundsWrite);
    }
    const std::size_t data_end = data_begin + static_cast<std::size_t>(byte_length);

    std::size_t count{0U};
    std::size_t position{data_begin};
    while (position < data_end)
    {
        if (count >= capacity)
        {
            return MakeUnexpected(SerializationError::kDeserializedCountExceedsCapacity);
        }
        Element element{};
        // The element count is not on the wire, so the last element is unknown up front.
        visitor.PushArrayElement(false);
        visitor.Next(element);
        visitor.PopField();
        if (iterator.HasError())
        {
            return MakeUnexpected(SerializationError::kOutOfBoundsWrite);
        }
        const std::size_t next_position = iterator.GetResult().value();
        // An element must make progress and must not cross the declared array end.
        if ((next_position <= position) || (next_position > data_end))
        {
            return MakeUnexpected(SerializationError::kOutOfBoundsWrite);
        }
        append(std::move(element));
        position = next_position;
        ++count;
    }
    if (align_following)
    {
        SkipAlignmentPadding(iterator, layout.alignment_bits);
    }
    return {};
}

/// @brief Dynamic array: [length in bytes][elements], plus trailing alignment padding when data follows. The length
/// field width is sizeof(LengthType).
template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
void SerializeLengthPrefixedVector(Visitor& visitor,
                                   SerializeIterator<Container> iterator,
                                   const LengthPrefixedVector<UnderlyingType, LengthType>& value,
                                   const ElementLayout& layout,
                                   const bool align_following)
{
    ElementLayout length_layout{layout};
    length_layout.length_field_bytes = static_cast<std::uint8_t>(sizeof(LengthType));
    SerializeArray(visitor, iterator, value, length_layout, align_following);
}

template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
Result<void> DeSerializeLengthPrefixedVector(Visitor& visitor,
                                             DeSerializeIterator<Container>& iterator,
                                             LengthPrefixedVector<UnderlyingType, LengthType>& value,
                                             const ElementLayout& layout,
                                             const bool align_following)
{
    ElementLayout length_layout{layout};
    length_layout.length_field_bytes = static_cast<std::uint8_t>(sizeof(LengthType));
    return DeSerializeArray<UnderlyingType>(visitor,
                                            iterator,
                                            length_layout,
                                            align_following,
                                            static_cast<std::size_t>(std::numeric_limits<LengthType>::max()),
                                            [&value](UnderlyingType&& element) {
                                                value.push_back(std::move(element));
                                            });
}

/// @brief Bounded array: [length in bytes][elements], plus trailing alignment padding when data follows. The capacity
/// N makes a finite GetMaxSize possible.
template <typename Visitor, typename Container, typename T, std::size_t N>
void SerializeStaticVector(Visitor& visitor,
                           SerializeIterator<Container> iterator,
                           const score::cpp::static_vector<T, N>& value,
                           const ElementLayout& layout,
                           const bool align_following)
{
    SerializeArray(visitor, iterator, value, layout, align_following);
}

template <typename Visitor, typename Container, typename T, std::size_t N>
Result<void> DeSerializeStaticVector(Visitor& visitor,
                                     DeSerializeIterator<Container>& iterator,
                                     score::cpp::static_vector<T, N>& value,
                                     const ElementLayout& layout,
                                     const bool align_following)
{
    return DeSerializeArray<T>(visitor, iterator, layout, align_following, N, [&value](T&& element) {
        value.push_back(std::move(element));
    });
}

}  // namespace someip_detail
}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_ARRAY_H
