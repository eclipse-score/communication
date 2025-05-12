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
#include "score/mw/com/impl/bindings/lola/path_builder.h"

#include <iomanip>
#include <string>

namespace score::mw::com::impl::lola
{

namespace
{

using InstanceId = LolaServiceInstanceId::InstanceId;

}  // namespace

/// Append a string of the form XXXXXXXXXXXXXXXX-YYYYY where X is the
/// service id and Y is the instance id.
///
/// \pre instanceId of binding must be available.
///
/// \param out Stream to write the string to.
void AppendServiceAndInstance(std::ostream& out, const std::uint16_t service_id, const InstanceId instance_id) noexcept
{
    AppendService(out, service_id);
    AppendInstanceId(out, instance_id);
}

void AppendService(std::ostream& out, const std::uint16_t service_id) noexcept
{
    out << std::setfill('0') << std::setw(16) << service_id << '-';
}

void AppendInstanceId(std::ostream& out, const InstanceId instance_id) noexcept
{
    out << std::setw(5) << instance_id;
}

}  // namespace score::mw::com::impl::lola
