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

/********************************************************************************
 * io_dispatcher error domain implementation.
 ********************************************************************************/

#include "score/io_dispatcher/io_dispatcher_error.h"

namespace io_dispatcher
{

std::string_view IoDispatcherErrorDomain::MessageFor(const score::result::ErrorCode& code) const noexcept
{
    switch (static_cast<IoDispatcherErrc>(code))
    {
        case IoDispatcherErrc::kApiError:
            return "API error: invalid arguments, handle not registered, or illegal state";
        case IoDispatcherErrc::kInvalidHandle:
            return "The native handle the callback was registered for is not valid anymore";
        case IoDispatcherErrc::kResource:
            return "Resource limit reached or allocation failed";
        case IoDispatcherErrc::kFatal:
            return "Unrecoverable error";
        default:
            return "Unknown io_dispatcher error";
    }
}

namespace
{
constexpr IoDispatcherErrorDomain io_dispatcher_error_domain;
}

score::result::Error MakeError(const IoDispatcherErrc code, const std::string_view user_message) noexcept
{
    return {static_cast<score::result::ErrorCode>(code), io_dispatcher_error_domain, user_message};
}

}  // namespace io_dispatcher
