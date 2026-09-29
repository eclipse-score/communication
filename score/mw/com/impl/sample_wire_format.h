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
#ifndef SCORE_MW_COM_IMPL_SAMPLE_WIRE_FORMAT_H
#define SCORE_MW_COM_IMPL_SAMPLE_WIRE_FORMAT_H

#include "score/mw/com/impl/serialization_sink.h"

#include <cstddef>
#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Marks a sample type whose raw object representation _is_ its wire representation.
/// \details This is deliberately an explicit statement of intent and not derived from a type property. Trivial
///          copyability in particular is not a sufficient criterion: a trivially copyable struct still carries
///          padding bytes and host byte order, both of which are wrong on a network. Specialize this only for types
///          which never leave the host in a form another host has to interpret.
/// \tparam Enable Hook for SFINAE constrained partial specializations.
template <typename SampleType, typename Enable = void>
struct IsRawWireRepresentation : std::false_type
{
};

/// \brief Always false, but dependent on the template parameter, so that a static_assert using it only fires when
///        the enclosing template is actually instantiated.
template <typename>
inline constexpr bool kNoWireFormatVisible = false;

/// \brief Defines how a sample type is represented on the wire.
///
/// \details This is the customization point through which the strongly typed layer teaches the type-erased bindings
///          how to marshal a sample. Specializations live _outside_ this layer (next to the sample type, usually
///          emitted by a code generator), so that no binding or wire format specific knowledge enters
///          score/mw/com/impl.
///
///          There is intentionally no fallback implementation. A silent default would be resolved at the point of
///          instantiation, so a translation unit which does not see the specialization would quietly serialize
///          differently than one which does. Both would compile and link, which is an ODR violation that no
///          diagnostic is required for. Failing to compile instead makes such a divergence impossible: a
///          translation unit either sees the specialization or it does not build.
///
///          Currently only the send direction is defined. When the proxy side becomes type-erased and needs a
///          receive direction, it belongs in _this_ trait rather than in a separate mechanism, so that both halves
///          of one wire format stay defined in one place and cannot drift apart.
///
/// \tparam SampleType The type to be represented on the wire.
/// \tparam Enable Hook for SFINAE constrained partial specializations.
template <typename SampleType, typename Enable = void>
struct SampleWireFormat
{
    static_assert(kNoWireFormatVisible<SampleType>,
                  "No SampleWireFormat specialization is visible for this sample type. Either include the generated "
                  "wire format header for the type, or specialize IsRawWireRepresentation for it if its raw object "
                  "representation is deliberately meant to be its wire representation.");
};

/// \brief Wire format of a type whose raw object representation is its wire representation.
template <typename SampleType>
struct SampleWireFormat<SampleType, std::enable_if_t<IsRawWireRepresentation<SampleType>::value>>
{
    static_assert(std::is_trivially_copyable_v<SampleType>,
                  "Only a trivially copyable type can use its raw object representation as wire representation.");

    /// \brief Worst case size of the wire representation in bytes.
    /// \details Handed down to the bindings so that they can size their serialization buffer up front, instead of
    ///          growing it while sending.
    static constexpr std::size_t kMaxSerializedSize{sizeof(SampleType)};

    static bool Serialize(const SampleType& sample, ISerializationSink& sink) noexcept
    {
        return sink.Write(&sample, sizeof(SampleType));
    }
};

}  // namespace score::mw::com::impl

/// \brief Declares that the raw object representation of Type is its wire representation.
/// \details Convenience wrapper around specializing IsRawWireRepresentation. Has to be used at global scope, and
///          only where the statement is actually true, i.e. where no consumer on another host has to interpret the
///          bytes.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) a macro is the only way to open the namespace at the use site
#define SCORE_MW_COM_DECLARE_RAW_WIRE_REPRESENTATION(Type) \
    namespace score::mw::com::impl                         \
    {                                                      \
    template <>                                            \
    struct IsRawWireRepresentation<Type> : std::true_type  \
    {                                                      \
    };                                                     \
    }

#endif  // SCORE_MW_COM_IMPL_SAMPLE_WIRE_FORMAT_H
