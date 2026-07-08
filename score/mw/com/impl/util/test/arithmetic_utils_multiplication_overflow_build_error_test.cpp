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
#include "score/mw/com/impl/util/arithmetic_utils.h"

#include <cstdint>
#include <limits>

namespace score::mw::com::impl
{

namespace
{

constexpr std::uint8_t kLhs = 1U + std::numeric_limits<std::uint8_t>::max() / 2U;
constexpr std::uint8_t kRhs = 2U;

// triggers a compile-time overflow assertion in score::mw::com::impl::static_assert_multiplication_does_not_overflow()
constexpr auto kResult = score::mw::com::impl::multiply_without_overflow<std::uint8_t, kLhs, kRhs>();

}  // namespace

}  // namespace score::mw::com::impl
