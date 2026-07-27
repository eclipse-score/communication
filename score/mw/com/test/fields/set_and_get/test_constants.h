/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_TEST_CONSTANTS_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_TEST_CONSTANTS_H

#include <cstdint>

namespace score::mw::com::test
{

constexpr const char* const kInstanceSpecifierString = "test/fields/set_and_get";

/// Value initially written by the skeleton via Update() before offering the service.
constexpr std::int32_t kInitialValue = 18;

/// A valid Set() request value inside the accepted range [kMinFieldValue, kMaxFieldValue].
constexpr std::int32_t kValidSetValue = 42;

/// A Set() request value that exceeds kMaxFieldValue; the set handler clamps it.
constexpr std::int32_t kInvalidSetValue = 200;

/// Expected Get() return after Set(kInvalidSetValue): the clamped maximum.
constexpr std::int32_t kClampedSetValue = 100;

/// Lower bound enforced by the skeleton's set handler.
constexpr std::int32_t kMinFieldValue = 0;

/// Upper bound enforced by the skeleton's set handler.
constexpr std::int32_t kMaxFieldValue = 100;

/// Value written by the skeleton via Update() after the consumer signals sets-done.
constexpr std::int32_t kUpdatedValue = 55;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_TEST_CONSTANTS_H
