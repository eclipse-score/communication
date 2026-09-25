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
#ifndef SCORE_MW_COM_IMPL_SAMPLE_SERIALIZATION_SPEC_H
#define SCORE_MW_COM_IMPL_SAMPLE_SERIALIZATION_SPEC_H

#include "score/mw/com/impl/serialize_sample_callback.h"

#include <cstddef>

namespace score::mw::com::impl
{

/// \brief Everything a type-erased binding needs in order to produce the wire representation of a sample.
///
/// \details Callback and size bound are handed over as one unit on purpose. A binding which serializes needs both:
///          the callback to produce the bytes, and the bound to size its buffer before any data flows. Passing them
///          separately would leave "which size belongs to no callback" undefined, whereas an optional around this
///          struct expresses both-or-neither.
struct SampleSerializationSpec
{
    /// \brief Produces the wire representation of one sample.
    SerializeSampleCallback serialize;

    /// \brief Worst case size of that wire representation in bytes.
    /// \details Lets a binding reserve its serialization buffer once, up front, instead of growing it while
    ///          sending. This mirrors how the LoLa binding calculates its shared memory size analytically before
    ///          any data flows.
    std::size_t max_serialized_size;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SAMPLE_SERIALIZATION_SPEC_H
