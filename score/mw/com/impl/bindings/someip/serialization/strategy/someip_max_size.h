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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_MAX_SIZE_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_MAX_SIZE_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"

#include "static_reflection_with_serialization/visitor/visit.h"
#include "static_reflection_with_serialization/visitor/visit_as_struct.h"

#include <score/static_vector.hpp>
#include <score/utility.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace score
{
namespace serialization
{

// -----------------------------------------------------------------------------
// GetMaxSize: worst-case serialized size of a type under SomeIpStrategy.
//
// Computed from the type's capacities, not from a concrete value, so it only
// yields a finite bound when every dynamic member is capacity-bounded (e.g.
// score::cpp::static_vector<T, N>). This is exactly the information LoLa needs to size
// the fixed shared-memory slot once at Skeleton creation. It reuses the same
// reflection traversal as serialization -- it is just a different sink.
// -----------------------------------------------------------------------------
namespace someip_detail
{
struct MaxSizeSink
{
    std::size_t bytes{0U};
    LayoutCursor cursor{};  // layout of the element currently being visited
};

/// @brief Fixed-width scalars, enums (and bool, size 1) contribute their fixed width. No padding behind fixed-length
/// elements.
template <typename T, std::enable_if_t<is_fixed_width_scalar<T>::value, bool> = true>
// coverity[autosar_cpp14_m3_2_2_violation]
void visit_as(MaxSizeSink& sink, T& /*unused*/)
{
    sink.bytes += sizeof(T);
}

inline void visit_as(MaxSizeSink& sink, bool& /*unused*/)
{
    sink.bytes += kBoolWireSize;
}

/// @brief Bounded array: length field + N times the per-element worst case + worst-case trailing alignment padding
/// (padding goes behind variable-length data). The trailing padding is at most (alignment_bytes - 1).
template <typename T, std::size_t N>
// coverity[autosar_cpp14_m3_2_2_violation]
void visit_as(MaxSizeSink& sink, score::cpp::static_vector<T, N>& /*unused*/)
{
    sink.bytes += sink.cursor.leaf.length_field_bytes;
    MaxSizeSink element{0U, EnterArrayElement(sink.cursor)};
    T probe{};
    ::score::common::visitor::visit(element, probe);
    sink.bytes += N * element.bytes;
    const std::size_t align = static_cast<std::size_t>(sink.cursor.leaf.alignment_bits) / kBitsPerByte;
    if (align > 1U)
    {
        sink.bytes += (align - 1U);  // worst-case trailing padding behind the variable-length element
    }
}

/// @brief Struct: sum of the worst case of each field (recurses through the same traversal).
template <typename S, typename... Args>
// coverity[autosar_cpp14_m3_2_2_violation]
void visit_as_struct(MaxSizeSink& sink, S&& /*unused*/, Args&... fields)
{
    const LayoutCursor outer = sink.cursor;
    std::size_t index{0U};
    const auto handle = [&sink, &index, &outer](auto& field) {
        sink.cursor = EnterField(outer, index);
        ::score::common::visitor::visit(sink, field);
        ++index;
    };
    const std::initializer_list<int> expand{(handle(fields), 0)...};
    score::cpp::ignore = expand;
    sink.cursor = outer;
}
}  // namespace someip_detail

/// @brief Returns the worst-case number of bytes SomeIpStrategy can serialize a value of type T into.
/// Only finite when every dynamic member of T is capacity-bounded (SOME/IP / LoLa requirement).
template <typename T>
std::size_t GetMaxSize()
{
    someip_detail::MaxSizeSink sink{};
    T instance{};
    ::score::common::visitor::visit(sink, instance);
    return sink.bytes;
}

/// @brief Layout-aware overload: includes worst-case SOME/IP alignment padding for the given datatype layout.
template <typename T>
std::size_t GetMaxSize(const SomeIpLayout& layout)
{
    someip_detail::MaxSizeSink sink{0U, someip_detail::RootCursor(layout)};
    T instance{};
    ::score::common::visitor::visit(sink, instance);
    return sink.bytes;
}

}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_MAX_SIZE_H
