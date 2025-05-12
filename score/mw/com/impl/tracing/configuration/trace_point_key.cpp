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
#include "score/mw/com/impl/tracing/configuration/trace_point_key.h"

namespace score::mw::com::impl::tracing
{

bool operator==(const TracePointKey& lhs, const TracePointKey& rhs) noexcept
{
    return ((lhs.service_element == rhs.service_element) && (lhs.trace_point_type == rhs.trace_point_type));
}

}  // namespace score::mw::com::impl::tracing
