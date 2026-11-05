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
#include "score/mw/com/impl/serialization/serialization_orchestrator.h"

#include "score/mw/com/impl/com_error.h"

#include <string>

namespace score::mw::com::impl::serialization
{

namespace
{

/// Signature of the serializer factory symbol exported by the serializer library, following the
/// documented `ISerializer* create_serializer_<structural-hash>_<attribute-hash>()` pattern.
using SerializerFactoryFn = ISerializer* (*)() noexcept;

}  // namespace

SerializationOrchestrator::SerializationOrchestrator(SerializationInfrastructure& infrastructure) noexcept
    : infrastructure_{&infrastructure}
{
}

score::Result<std::unique_ptr<ISerializer>> SerializationOrchestrator::SelectSerializer(
    const SerializerLookupKey& key) const noexcept
{
    if (!infrastructure_->IsOpen().has_value())
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serialization infrastructure is not open");
    }

    const std::string factory_symbol_name = key.FactorySymbolName();
    const auto factory = infrastructure_->ResolveSymbol<SerializerFactoryFn>(factory_symbol_name.c_str());
    if (factory == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serializer factory symbol not found");
    }

    ISerializer* const serializer = factory();
    if (serializer == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serializer factory returned nullptr");
    }

    return std::unique_ptr<ISerializer>{serializer};
}

}  // namespace score::mw::com::impl::serialization
