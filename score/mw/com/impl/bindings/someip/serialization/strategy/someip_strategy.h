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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_array.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_max_size.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_scalar.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_string.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_wire_format.h"

#include "score/serialization/serialize_iterator.h"
#include "score/serialization/types/length_prefixed_vector.h"

#include <score/static_vector.hpp>
#include <score/string.hpp>

#include <cstddef>
#include <type_traits>

namespace score
{
namespace serialization
{

/// @brief Example SOME/IP serialization strategy.
///
/// @details Demonstrates how SOME/IP can be expressed as a SerializationStrategy on top of the existing infrastructure
/// (visitor + SerializeIterator). Per-element byte order, alignment and length-field width come from a per-datatype
/// SomeIpLayout (SOME/IP default: big-endian / packed); a default-constructed strategy is uniform. It relies on a few
/// small framework hooks: SerializeIterator::Offset() for position-dependent padding, and the visitor's field path
/// (CommonVisitorLogic::FieldPath/LastPath) for depth-aware layout and alignment.
///
/// The per-type encoding/decoding logic lives in dedicated headers (someip_scalar.h, someip_array.h, someip_string.h)
/// built on the shared wire mechanics in someip_wire_format.h; the overloads below are thin dispatchers that resolve
/// the element's layout and delegate.
///
/// Supported so far:
/// * all fixed-width integers (signed/unsigned) and enums, big- or little-endian (per element)
/// * floating point (IEEE-754 binary32/binary64), big- or little-endian (per element)
/// * bool (single byte, 0x00 / 0x01)
/// * structs, including nested ones (each field is routed back through the strategy by the visitor, in declaration
///   order, with its own per-field layout)
/// * score::serialization::types::LengthPrefixedVector (on wire: configurable n-byte length field, then the actual
///   elements -- compact, no capacity padding)
/// * score::cpp::static_vector (bounded array: configurable length field, then the actual elements)
/// * score::cpp::pmr::string (UTF-8 on wire: configurable-width length field, then a UTF-8 BOM, the characters, and a
///   null terminator -- compact, no capacity padding)
///
/// This is the key contrast to the UDS strategy: dynamic data is emitted at its *actual* size, not padded to a fixed
/// capacity. Byte order defaults to big-endian (the SOME/IP default) but is a per-element layout choice.
///
/// Nested layout resolution: the layout is a tree -- each ElementLayout may carry a `nested` SomeIpLayout -- and the
/// element currently visited is resolved by replaying the visitor's field path from the root (see
/// ResolveElementLayout). A null nested layout keeps a subtree uniform, reproducing the single-level default. The cases
/// handled, and how:
/// * Flat struct: each field uses layout.per_field[field_index] (or layout.default_). This is the original behaviour.
/// * Nested struct (arbitrary depth): on descending into field i, the field's `nested` layout becomes the layout for
///   that sub-struct's fields; with no `nested` the sub-struct inherits the field's own layout.
/// * Array of scalars: the length field uses the array field's layout; elements inherit that same layout (so they share
///   its byte order), because an array-element path step with a null `nested` inherits the array's layout.
/// * Array of structs: the array field's `nested` layout describes the element struct's fields, resolved per field.
/// * Array of arrays (any depth): each array element is marked in the field path (kArrayElementMarker), so an inner
///   array descends into the outer array's `nested` layout and gets its own length-field width, alignment and byte
///   order, independent of the outer array.
/// * String elements/fields: length field, BOM, characters and terminator use the resolved element layout.
///
/// @note Trailing alignment padding behind a variable-length member is emitted whenever data follows it anywhere in the
/// message, i.e. it is not the last element at every enclosing nesting level (FollowedByMoreData walks LastPath).
/// So a member that is last in a nested struct but followed by more data in an enclosing struct is still padded, and a
/// non-last array element is padded ahead of the next element.
///
/// @note Maximum nesting depth is bounded by CommonVisitorLogic::kMaxNestingDepth.
class SomeIpStrategy final
{
  public:
    SomeIpStrategy() noexcept = default;
    explicit SomeIpStrategy(const SomeIpLayout& layout) noexcept : layout_{layout} {}

    /// @brief Fixed-width integers (signed/unsigned), enums, and floating point.
    template <typename Visitor,
              typename Container,
              typename T,
              std::enable_if_t<someip_detail::is_fixed_width_scalar<T>::value, bool> = true>
    void Serialize(Visitor& visitor, SerializeIterator<Container> iterator, const T& value) const
    {
        someip_detail::SerializeScalar(iterator, value, LayoutFor(visitor));
    }

    template <typename Visitor,
              typename Container,
              typename T,
              std::enable_if_t<someip_detail::is_fixed_width_scalar<T>::value, bool> = true>
    Result<void> DeSerialize(Visitor& visitor, DeSerializeIterator<Container>& iterator, T& value) const
    {
        someip_detail::DeSerializeScalar(iterator, value, LayoutFor(visitor));
        return ErrorOf(iterator);
    }

    /// @brief Boolean: a single byte, FALSE = 0x00, TRUE = 0x01.
    template <typename Visitor, typename Container>
    void Serialize(Visitor& /*visitor*/, SerializeIterator<Container> iterator, const bool& value) const
    {
        someip_detail::SerializeBool(iterator, value);
    }

    template <typename Visitor, typename Container>
    Result<void> DeSerialize(Visitor& /*visitor*/, DeSerializeIterator<Container>& iterator, bool& value) const
    {
        someip_detail::DeSerializeBool(iterator, value);
        return ErrorOf(iterator);
    }

    /// @brief Dynamic array: [length][elements], with trailing alignment padding when not the last struct element.
    template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
    void Serialize(Visitor& visitor,
                   SerializeIterator<Container> iterator,
                   const LengthPrefixedVector<UnderlyingType, LengthType>& value) const
    {
        someip_detail::SerializeLengthPrefixedVector(
            visitor, iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

    template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
    Result<void> DeSerialize(Visitor& visitor,
                             DeSerializeIterator<Container>& iterator,
                             LengthPrefixedVector<UnderlyingType, LengthType>& value) const
    {
        return someip_detail::DeSerializeLengthPrefixedVector(
            visitor, iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

    /// @brief SOME/IP UTF-8 string, with trailing alignment padding when not the last struct element.
    template <typename Visitor, typename Container>
    void Serialize(Visitor& visitor, SerializeIterator<Container> iterator, const score::cpp::pmr::string& value) const
    {
        someip_detail::SerializeString(iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

    template <typename Visitor, typename Container>
    Result<void> DeSerialize(Visitor& visitor,
                             DeSerializeIterator<Container>& iterator,
                             score::cpp::pmr::string& value) const
    {
        return someip_detail::DeSerializeString(
            visitor, iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

    /// @brief Bounded array: [length][elements], with trailing alignment padding when not the last struct element.
    template <typename Visitor, typename Container, typename T, std::size_t N>
    void Serialize(Visitor& visitor,
                   SerializeIterator<Container> iterator,
                   const score::cpp::static_vector<T, N>& value) const
    {
        someip_detail::SerializeStaticVector(visitor, iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

    template <typename Visitor, typename Container, typename T, std::size_t N>
    Result<void> DeSerialize(Visitor& visitor,
                             DeSerializeIterator<Container>& iterator,
                             score::cpp::static_vector<T, N>& value) const
    {
        return someip_detail::DeSerializeStaticVector(
            visitor, iterator, value, LayoutFor(visitor), FollowedByMoreData(visitor));
    }

  private:
    template <typename Container>
    static Result<void> ErrorOf(const DeSerializeIterator<Container>& iterator)
    {
        if (iterator.HasError())
        {
            return Unexpected{iterator.GetResult().error()};
        }
        return {};
    }

    static ElementLayout ElementAt(const SomeIpLayout& layout, const std::size_t field_index)
    {
        return ((layout.per_field != nullptr) && (field_index < layout.per_field_count)) ? layout.per_field[field_index]
                                                                                         : layout.default_;
    }

    // Resolves the layout of the element currently being visited by following the field path from the root. Struct
    // field markers index into the current struct layout; an array-element marker descends into the array's nested
    // layout (or, when that is uniform/null, lets the element inherit the array's own layout).
    ElementLayout ResolveElementLayout(const std::size_t* const path,
                                       const std::size_t depth,
                                       const std::size_t array_marker) const
    {
        const SomeIpLayout* struct_layout = &layout_;
        ElementLayout leaf = layout_.default_;
        for (std::size_t level{0U}; level < depth; ++level)
        {
            if (path[level] == array_marker)
            {
                if (leaf.nested != nullptr)
                {
                    struct_layout = leaf.nested;
                    leaf = leaf.nested->default_;
                }
                else
                {
                    struct_layout = nullptr;  // uniform array: the element inherits the array's own layout
                }
            }
            else if (struct_layout != nullptr)
            {
                leaf = ElementAt(*struct_layout, path[level]);
                struct_layout = leaf.nested;  // null => uniform subtree below, keep inheriting leaf
            }
            else
            {
                // Uniform subtree below a null nested layout: keep inheriting the current leaf.
            }
        }
        return leaf;
    }

    template <typename Visitor>
    ElementLayout LayoutFor(const Visitor& visitor) const
    {
        return ResolveElementLayout(visitor.FieldPath().data(), visitor.FieldDepth(), Visitor::kArrayElementMarker);
    }

    // True when data follows the element currently being visited anywhere in the message, i.e. it is not the last
    // element at every enclosing nesting level. SOME/IP appends trailing alignment padding only in that case.
    template <typename Visitor>
    static bool FollowedByMoreData(const Visitor& visitor)
    {
        for (const bool is_last : visitor.LastPath())
        {
            if (!is_last)
            {
                return true;
            }
        }
        return false;
    }

    // Per-datatype wire layout (byte order + alignment). A default-constructed layout is uniform big-endian / packed,
    // reproducing the SOME/IP default without any per-field overrides.
    SomeIpLayout layout_{};
};

}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_H
