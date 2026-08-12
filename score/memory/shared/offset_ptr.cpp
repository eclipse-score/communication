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
#include "score/memory/shared/offset_ptr.h"

namespace score::memory::shared
{

namespace
{

/// \brief global (process wide) flag, whether bounds-checking shall be done.
/// \details defaults to true (for safety reasons). Users of shared-memory/OffsetPtr infrastructure can enable/disable
///          it via EnableOffsetPtrBoundsChecking(bool enable)
bool bounds_checking_enabled = true;

}  // anonymous namespace

namespace detail_offset_ptr
{

bool IsBoundsCheckingEnabled() noexcept
{
    return bounds_checking_enabled;
}

}  // namespace detail_offset_ptr

bool EnableOffsetPtrBoundsChecking(const bool enable)
{
    const bool previous_value = bounds_checking_enabled;
    bounds_checking_enabled = enable;
    return previous_value;
}

}  // namespace score::memory::shared
