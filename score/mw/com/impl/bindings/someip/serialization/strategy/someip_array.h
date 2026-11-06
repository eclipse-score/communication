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
#include <utility>

namespace score
{
namespace serialization
{
namespace someip_detail
{

/// @brief Dynamic array: [length][element_0 ... element_{n-1}]. Per SOME/IP, if this variable-length member is not the
/// last element (align_following), padding is appended afterwards so the data that follows is aligned to the element's
/// alignment_bits. The length is the actual element count (matching LengthPrefixedVector's own semantics).
template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
void SerializeLengthPrefixedVector(Visitor& visitor,
                                   SerializeIterator<Container> iterator,
                                   const LengthPrefixedVector<UnderlyingType, LengthType>& value,
                                   const ElementLayout& layout,
                                   const bool align_following)
{
    SerializeScalar(iterator, value.size(), layout);  // actual element count
    const std::size_t count = value.size();
    std::size_t index{0U};
    for (const auto& sub_value : value)
    {
        visitor.PushArrayElement((index + 1U) == count);
        visitor.Next(sub_value);  // route each element back through the visitor (handles nested types)
        visitor.PopField();
        ++index;
    }
    if (align_following)
    {
        EmitAlignmentPadding(iterator, layout.alignment_bits);
    }
}

template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
Result<void> DeSerializeLengthPrefixedVector(Visitor& visitor,
                                             DeSerializeIterator<Container>& iterator,
                                             LengthPrefixedVector<UnderlyingType, LengthType>& value,
                                             const ElementLayout& layout,
                                             const bool align_following)
{
    LengthType counter{};
    visitor.Next(counter);

    for (; counter > static_cast<LengthType>(0); counter--)
    {
        UnderlyingType underlying_type{};
        visitor.PushArrayElement(counter == static_cast<LengthType>(1));
        visitor.Next(underlying_type);
        visitor.PopField();
        value.push_back(underlying_type);
    }

    if (align_following)
    {
        SkipAlignmentPadding(iterator, layout.alignment_bits);
    }
    return {};
}

/// @brief Bounded array: [length (= element count)][elements]. As for the dynamic array, trailing alignment padding is
/// appended when this is not the last element (align_following). The length-field width and alignment come from the
/// element's layout. score::cpp::static_vector has a compile-time capacity N, which is what makes a finite GetMaxSize
/// possible.
template <typename Visitor, typename Container, typename T, std::size_t N>
void SerializeStaticVector(Visitor& visitor,
                           SerializeIterator<Container> iterator,
                           const score::cpp::static_vector<T, N>& value,
                           const ElementLayout& layout,
                           const bool align_following)
{
    WriteLengthField(iterator, static_cast<std::uint64_t>(value.size()), layout);  // actual element count
    const std::size_t count = value.size();
    std::size_t index{0U};
    for (const auto& element : value)
    {
        visitor.PushArrayElement((index + 1U) == count);
        visitor.Next(element);  // route each element back through the visitor (handles nested types)
        visitor.PopField();
        ++index;
    }
    if (align_following)
    {
        EmitAlignmentPadding(iterator, layout.alignment_bits);
    }
}

template <typename Visitor, typename Container, typename T, std::size_t N>
Result<void> DeSerializeStaticVector(Visitor& visitor,
                                     DeSerializeIterator<Container>& iterator,
                                     score::cpp::static_vector<T, N>& value,
                                     const ElementLayout& layout,
                                     const bool align_following)
{
    const std::uint64_t count = ReadLengthField(iterator, layout);
    // Guard against manipulated input declaring more elements than the fixed-capacity container can hold.
    if (count > static_cast<std::uint64_t>(N))
    {
        return MakeUnexpected(SerializationError::kDeserializedCountExceedsCapacity);
    }
    for (std::uint64_t index{0U}; index < count; ++index)
    {
        T element{};
        visitor.PushArrayElement((index + 1U) == count);
        visitor.Next(element);
        visitor.PopField();
        value.push_back(std::move(element));
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

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_ARRAY_H
