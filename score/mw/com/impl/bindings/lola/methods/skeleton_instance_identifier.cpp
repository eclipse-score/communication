/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"

namespace score::mw::com::impl::lola
{

bool operator==(const SkeletonInstanceIdentifier& lhs, const SkeletonInstanceIdentifier& rhs) noexcept
{
    return ((lhs.service_id == rhs.service_id) && (lhs.instance_id == rhs.instance_id));
}

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const SkeletonInstanceIdentifier& value) noexcept
{
    stream << "Service ID:" << value.service_id << ". Instance ID:" << value.instance_id;
    return stream;
}

}  // namespace score::mw::com::impl::lola
