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
/// (visitor + SerializeIterator). Per-element byte order and alignment come from a per-datatype SomeIpLayout resolved by
/// field index (SOME/IP default: big-endian / packed); a default-constructed strategy is uniform. It
/// relies on two small framework hooks: SerializeIterator::Offset() and MetaInformation::field_index_.
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
/// * score::cpp::pmr::string (UTF-8 on wire: configurable-width length field, then a UTF-8 BOM, the characters, and a
///   null terminator -- compact, no capacity padding)
///
/// This is the key contrast to the UDS strategy: dynamic data is emitted at its *actual* size, not padded to a fixed
/// capacity. Byte order defaults to big-endian (the SOME/IP default) but is a per-element layout choice.
///
/// @note Alignment padding behind a variable-length member uses MetaInformation::is_last_element_in_struct_, which is
/// scoped to the immediate struct. A variable-length member that is last in a nested struct but followed by more data
/// in an enclosing struct is treated as "last" here (no trailing padding), whereas strict SOME/IP would still pad.
/// Fully covering that case needs "last in the whole message" tracking; left as a known limitation for now.
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
        const auto& meta = visitor.GetMetaInformation();
        someip_detail::SerializeLengthPrefixedVector(
            visitor, iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
    }

    template <typename Visitor, typename Container, typename UnderlyingType, typename LengthType>
    Result<void> DeSerialize(Visitor& visitor,
                             DeSerializeIterator<Container>& iterator,
                             LengthPrefixedVector<UnderlyingType, LengthType>& value) const
    {
        const auto& meta = visitor.GetMetaInformation();
        return someip_detail::DeSerializeLengthPrefixedVector(
            visitor, iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
    }

    /// @brief SOME/IP UTF-8 string, with trailing alignment padding when not the last struct element.
    template <typename Visitor, typename Container>
    void Serialize(Visitor& visitor, SerializeIterator<Container> iterator, const score::cpp::pmr::string& value) const
    {
        const auto& meta = visitor.GetMetaInformation();
        someip_detail::SerializeString(
            iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
    }

    template <typename Visitor, typename Container>
    Result<void> DeSerialize(Visitor& visitor, DeSerializeIterator<Container>& iterator, score::cpp::pmr::string& value)
        const
    {
        const auto& meta = visitor.GetMetaInformation();
        return someip_detail::DeSerializeString(
            visitor, iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
    }

    /// @brief Bounded array: [length][elements], with trailing alignment padding when not the last struct element.
    template <typename Visitor, typename Container, typename T, std::size_t N>
    void Serialize(Visitor& visitor, SerializeIterator<Container> iterator, const score::cpp::static_vector<T, N>& value)
        const
    {
        const auto& meta = visitor.GetMetaInformation();
        someip_detail::SerializeStaticVector(
            visitor, iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
    }

    template <typename Visitor, typename Container, typename T, std::size_t N>
    Result<void> DeSerialize(Visitor& visitor,
                             DeSerializeIterator<Container>& iterator,
                             score::cpp::static_vector<T, N>& value) const
    {
        const auto& meta = visitor.GetMetaInformation();
        return someip_detail::DeSerializeStaticVector(
            visitor, iterator, value, LayoutFor(visitor), !meta.is_last_element_in_struct_);
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

    // Resolves the layout of the element currently being visited by following the field-index path from the root
    // through nested structs. A null nested layout means that subtree is uniform (SOME/IP default).
    ElementLayout ResolveElementLayout(const std::size_t* const path, const std::size_t depth) const
    {
        const SomeIpLayout* current = &layout_;
        for (std::size_t level{0U}; (level + 1U) < depth; ++level)
        {
            const ElementLayout ancestor = ElementAt(*current, path[level]);
            if (ancestor.nested == nullptr)
            {
                return ElementLayout{};  // uniform subtree below an ancestor without an explicit nested layout
            }
            current = ancestor.nested;
        }
        const std::size_t leaf_index = (depth == 0U) ? 0U : path[depth - 1U];
        return ElementAt(*current, leaf_index);
    }

    template <typename Visitor>
    ElementLayout LayoutFor(const Visitor& visitor) const
    {
        return ResolveElementLayout(visitor.FieldPath().data(), visitor.FieldDepth());
    }

    // Per-datatype wire layout (byte order + alignment). A default-constructed layout is uniform big-endian / packed,
    // reproducing the SOME/IP default without any per-field overrides.
    SomeIpLayout layout_{};
};

}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_STRATEGY_H
