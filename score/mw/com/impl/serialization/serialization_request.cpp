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
#include "score/mw/com/impl/serialization/serialization_request.h"

#include <utility>

namespace score::mw::com::impl::serialization
{

SerializationRequest::SerializationRequest(InstanceSpecifier instance_specifier) noexcept
    : instance_specifier_{std::move(instance_specifier)}
{
}

const InstanceSpecifier& SerializationRequest::GetInstanceSpecifier() const noexcept
{
    return instance_specifier_;
}

}  // namespace score::mw::com::impl::serialization
