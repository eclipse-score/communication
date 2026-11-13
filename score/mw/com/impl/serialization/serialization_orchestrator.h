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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_ORCHESTRATOR_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_ORCHESTRATOR_H

#include "score/mw/com/impl/serialization/serialization_infrastructure.h"
#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include "score/result/result.h"

#include <memory>

namespace score::mw::com::impl::serialization
{

/// \brief Binding-independent entry point that turns a serializer lookup into a usable serializer.
///
/// This is the orchestration step that sits on top of `SerializationInfrastructure`. Given the
/// lookup key (structural hash from the service interface, attribute hash from the deployment
/// configuration) it resolves the matching factory symbol from the already loaded serializer
/// library and creates the concrete, binding-specific `ISerializer`.
///
/// The orchestrator owns the serializer selection decision. The binding only opts in (see
/// `SerializationRequest`); it never resolves symbols or manages the library lifecycle itself.
class SerializationOrchestrator final
{
  public:
    /// \param infrastructure The loaded serializer library whose lifecycle and symbol cache stay in
    ///        the binding-independent layer. Must outlive this orchestrator.
    explicit SerializationOrchestrator(SerializationInfrastructure& infrastructure) noexcept;

    /// \brief Select and create the serializer identified by `key`.
    ///
    /// Resolves `key.FactorySymbolName()` from the serializer library and invokes it to create the
    /// concrete serializer.
    /// \return The created serializer, or a binding failure if the library is not open, the factory
    ///         symbol is missing, or the factory returned a nullptr.
    score::Result<std::unique_ptr<ISerializer>> SelectSerializer(const SerializerLookupKey& key) const noexcept;

  private:
    SerializationInfrastructure* infrastructure_;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_ORCHESTRATOR_H
