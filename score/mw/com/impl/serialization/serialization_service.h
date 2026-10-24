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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_SERVICE_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_SERVICE_H

#include "score/mw/com/impl/serialization/serialization_orchestrator.h"
#include "score/mw/com/impl/serialization/serialization_request.h"
#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_key_resolver.h"

#include "score/result/result.h"

#include <memory>

namespace score::mw::com::impl::serialization
{

/// \brief Binding-independent entry point a binding calls to obtain a serializer.
///
/// This is the "defined API" from the design: the binding only hands over a `SerializationRequest`
/// (its opt-in) and receives a ready-to-use `ISerializer`. Everything in between stays here:
///   1. the key resolver turns the request into a `SerializerLookupKey`
///      (structural hash from the service interface, attribute hash from the deployment config),
///   2. the orchestrator loads and creates the matching serializer.
///
/// The binding never sees lookup keys, factory symbols, or the dynamic loader.
class SerializationService final
{
  public:
    /// \param orchestrator Creates the serializer from a lookup key. Must outlive this service.
    /// \param key_resolver Derives the lookup key from a request. Must outlive this service.
    SerializationService(SerializationOrchestrator& orchestrator,
                         const ISerializerKeyResolver& key_resolver) noexcept;

    /// \brief Obtain the serializer for the binding's opt-in request.
    /// \return The created serializer, or a failure if the request cannot be resolved to a serializer
    ///         or the serializer cannot be created.
    score::Result<std::unique_ptr<ISerializer>> GetSerializer(const SerializationRequest& request) const noexcept;

  private:
    SerializationOrchestrator* orchestrator_;
    const ISerializerKeyResolver* key_resolver_;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_SERVICE_H
