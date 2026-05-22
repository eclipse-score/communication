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
#include "score/mw/com/gateway/transport_layer/transport_error.h"

namespace
{
constexpr score::mw::com::gateway::TransportErrorDomain g_transport_error_domain;
}  // namespace

score::result::Error score::mw::com::gateway::MakeError(const TransportErrorc code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_transport_error_domain, message};
}
