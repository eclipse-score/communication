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
#ifndef SCORE_MW_COM_IMPL_RAW_WIRE_REPRESENTATION_FUNDAMENTALS_H
#define SCORE_MW_COM_IMPL_RAW_WIRE_REPRESENTATION_FUNDAMENTALS_H

#include "score/mw/com/impl/sample_wire_format.h"

#include <array>
#include <cstddef>
#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Declares the object representation of the fundamental types to be their wire representation.
///
/// \details This is true for every binding which hands the object representation itself to its consumers rather
///          than a serialized form of it. LoLa is such a binding: its consumers read the sample in place from
///          shared memory, on the same host and built with the same toolchain, so byte order and padding are shared
///          by construction.
///
/// \attention This is _not_ true for a binding which puts the bytes on a network. SOME/IP for instance requires
///            network byte order, so an event whose top level type is a fundamental type and which is deployed over
///            SOME/IP needs its own SampleWireFormat specialization. Since SampleWireFormat is keyed on the sample
///            type alone, such a type cannot use both representations within one binary. See the corresponding note
///            in sample_wire_format.h.
template <typename SampleType>
struct IsRawWireRepresentation<SampleType,
                               std::enable_if_t<std::is_arithmetic_v<SampleType> || std::is_enum_v<SampleType>>>
    : std::true_type
{
};

/// \brief An array is raw exactly when its element type is.
/// \details The decision is propagated from the element type instead of being granted to every array, so that an
///          array of a type which needs real marshalling keeps needing it.
template <typename ElementType, std::size_t Size>
struct IsRawWireRepresentation<std::array<ElementType, Size>> : IsRawWireRepresentation<ElementType>
{
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_RAW_WIRE_REPRESENTATION_FUNDAMENTALS_H
