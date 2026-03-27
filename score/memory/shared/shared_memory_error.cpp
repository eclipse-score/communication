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
#include "score/memory/shared/shared_memory_error.h"

namespace score::memory::shared
{

namespace
{

constexpr SharedMemoryErrorDomain g_shared_memory_error_domain{};

}  // namespace

score::result::Error MakeError(const SharedMemoryErrorCode code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_shared_memory_error_domain, message};
}

}  // namespace score::memory::shared
