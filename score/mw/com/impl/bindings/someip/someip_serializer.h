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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_SERIALIZER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_SERIALIZER_H

#include "score/mw/com/impl/bindings/someip/someip_writer.h"

namespace score::mw::com::impl::someip
{

/// \brief Produces the SOME/IP wire representation of a type.
///
/// \details Specialized per type, usually by generated code. This is the format level counterpart of the binding
///          independent SampleWireFormat: the latter is the entry point the type-erased binding calls, the former
///          describes how one type is laid out in the SOME/IP wire format.
///
///          Note that the parameter is a SomeIpWriter and _not_ an ISerializationSink. The writer is stateful (it
///          tracks the length fields still to be patched), so it has to be created once per serialization and
///          shared by all nested members. Taking a writer here means a nested specialization has nothing it could
///          create a second writer from, which turns that mistake from a convention into a compile error.
///
/// \tparam SampleType The type to be laid out.
/// \tparam Enable Hook for SFINAE constrained partial specializations.
template <typename SampleType, typename Enable = void>
struct SomeIpSerializer;

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_SERIALIZER_H
