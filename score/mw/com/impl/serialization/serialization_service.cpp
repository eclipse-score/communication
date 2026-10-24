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
#include "score/mw/com/impl/serialization/serialization_service.h"

#include "score/mw/com/impl/com_error.h"

namespace score::mw::com::impl::serialization
{

SerializationService::SerializationService(SerializationOrchestrator& orchestrator,
                                           const ISerializerKeyResolver& key_resolver) noexcept
    : orchestrator_{&orchestrator}, key_resolver_{&key_resolver}
{
}

score::Result<std::unique_ptr<ISerializer>> SerializationService::GetSerializer(
    const SerializationRequest& request) const noexcept
{
    const auto lookup_key = key_resolver_->Resolve(request);
    if (!lookup_key.has_value())
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure,
                                     "could not resolve serializer selection for serialization request");
    }

    return orchestrator_->SelectSerializer(lookup_key.value());
}

}  // namespace score::mw::com::impl::serialization
