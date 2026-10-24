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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_KEY_RESOLVER_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_KEY_RESOLVER_H

#include "score/mw/com/impl/serialization/serialization_request.h"
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include "score/result/result.h"

namespace score::mw::com::impl::serialization
{

/// \brief Resolves a binding's opt-in request into the key that identifies the serializer to load.
///
/// This is the seam between the binding's generic opt-in (`SerializationRequest`, which only carries
/// an `InstanceSpecifier`) and the serializer selection used by the orchestrator
/// (`SerializerLookupKey`, the structural and attribute hashes).
///
/// The real implementation derives the structural hash from the service interface and the attribute
/// hash from the deployment configuration discovered through the `InstanceSpecifier`. That
/// derivation depends on the configuration subsystem, so it is kept behind this interface to keep
/// the facade and the binding independent of it and to make the flow testable with a fake resolver.
class ISerializerKeyResolver
{
  public:
    virtual ~ISerializerKeyResolver() = default;

    /// \brief Derive the serializer lookup key for the given opt-in request.
    /// \return The lookup key, or a failure if no serializer selection can be determined for the
    ///         request's instance.
    virtual score::Result<SerializerLookupKey> Resolve(const SerializationRequest& request) const noexcept = 0;

  protected:
    ISerializerKeyResolver() = default;
    ISerializerKeyResolver(const ISerializerKeyResolver&) = default;
    ISerializerKeyResolver& operator=(const ISerializerKeyResolver&) = default;
    ISerializerKeyResolver(ISerializerKeyResolver&&) = default;
    ISerializerKeyResolver& operator=(ISerializerKeyResolver&&) = default;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_KEY_RESOLVER_H
