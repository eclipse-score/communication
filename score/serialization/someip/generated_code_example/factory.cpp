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
#include "score/serialization/someip/generated_code_example/speed_rpm.h"
#include "score/serialization/someip/generated_code_example/speed_rpm_serializer.h"
#include "score/serialization/someip/serializer.h"

#include <new>

namespace
{
using score::serialization::someip::SpeedRpm;

// The exported symbol name must spell out the hashes as literals, so pin them to the datatype constants.
static_assert(SpeedRpm::kStructuralHash == 0x2c5f8d1aULL, "Symbol name does not match structural hash");
static_assert(SpeedRpm::kAttributeHash == 0x7e3a91c4ULL, "Symbol name does not match attribute hash");
}  // namespace

extern "C" {

// Symbol pattern: CreateSerializer_<structural-hash>_<attribute-hash>(<other-attributes>).
// This variant has no attributes configured at runtime.
score::serialization::someip::ISerializer* CreateSerializer_2c5f8d1a_7e3a91c4() noexcept
{
    return new (std::nothrow) score::serialization::someip::SpeedRpmSerializer{};
}

void DestroySerializer(score::serialization::someip::ISerializer* serializer) noexcept
{
    delete serializer;
}

}  // extern "C"
