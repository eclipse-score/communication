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

#include "score/mw/com/gateway/transport_layer/sample/messages/message_error.h"

namespace score::mw::com::gateway
{

namespace
{
constexpr MessageErrorDomain g_MessageErrorDomain;
}  // namespace

score::result::Error MakeError(const MessageErrorc code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_MessageErrorDomain, message};
}

}  // namespace score::mw::com::gateway
