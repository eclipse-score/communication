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
#ifndef SCORE_MW_COM_IMPL_SERIALIZE_SAMPLE_CALLBACK_H
#define SCORE_MW_COM_IMPL_SERIALIZE_SAMPLE_CALLBACK_H

#include "score/mw/com/impl/serialization_sink.h"

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Callback type for serializing a sample. The callback takes a pointer to the type-erased sample to be
/// serialized and the sink to write the serialized representation into.
/// \details The callback is handed over from our strongly typed (binding independent) layer, which has the strong
/// type definition, to the type-erased (binding) layers, in cases where a binding has to produce a wire
/// representation of a sample. A binding only sees `void*` plus size/alignment, which is not sufficient to marshal
/// a sample (e.g. endianness conversion or variable length members); only a layer with type knowledge can do that.
/// Therefore it is the one to provide such a serializer.
/// This mirrors InitializeSampleCallback, which solves the same problem for type-correct initialization of
/// type-erased storage.
/// Possible implementation:
/// auto serialize_sample_callback = [](const void* type_erased_sample, ISerializationSink& sink){
///      Write the raw object representation (valid for trivially copyable types)
///      score::cpp::ignore = sink.Write(type_erased_sample, sizeof(EventType));
/// }
using SerializeSampleCallback = score::cpp::callback<void(const void*, ISerializationSink&)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERIALIZE_SAMPLE_CALLBACK_H
