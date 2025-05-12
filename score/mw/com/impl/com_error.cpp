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
#include "score/mw/com/impl/com_error.h"

namespace
{
constexpr score::mw::com::impl::ComErrorDomain g_comErrorDomain;
}  // namespace

score::result::Error score::mw::com::impl::MakeError(const ComErrc code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_comErrorDomain, message};
}
