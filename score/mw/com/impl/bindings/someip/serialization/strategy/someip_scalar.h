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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_SCALAR_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_SCALAR_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_wire_format.h"

#include "score/serialization/serialize_iterator.h"

#include <score/utility.hpp>

namespace score
{
namespace serialization
{
namespace someip_detail
{

/// @brief Fixed-width integers (signed/unsigned), enums, and floating point in the element's byte order (defaulting to
/// big-endian). SOME/IP scalars are packed, so no alignment padding is emitted.
template <typename Container, typename T>
void SerializeScalar(SerializeIterator<Container> iterator, const T& value, const ElementLayout& layout)
{
    WriteScalar(iterator, value, ReverseFor(layout.byte_order));
}

template <typename Container, typename T>
void DeSerializeScalar(DeSerializeIterator<Container>& iterator, T& value, const ElementLayout& layout)
{
    ReadScalar(iterator, value, ReverseFor(layout.byte_order));
}

/// @brief Boolean: a single packed byte, FALSE = 0x00, TRUE = 0x01.
template <typename Container>
void SerializeBool(SerializeIterator<Container> iterator, const bool value)
{
    iterator = value ? kBoolTrue : kBoolFalse;
}

template <typename Container>
void DeSerializeBool(DeSerializeIterator<Container>& iterator, bool& value)
{
    if (iterator.HasError())
    {
        return;
    }
    if (iterator == iterator.End())
    {
        iterator.SetError(SerializationError::kOutOfBoundsWrite);  // truncated input
        return;
    }
    value = (*iterator != kBoolFalse);  // SOME/IP: any non-zero byte decodes to TRUE
    score::cpp::ignore = ++iterator;
}

}  // namespace someip_detail
}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_SCALAR_H
