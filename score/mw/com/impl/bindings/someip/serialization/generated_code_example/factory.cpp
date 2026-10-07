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
#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"
#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm_serializer.h"
#include "score/mw/com/impl/bindings/someip/serialization/serializer.h"

#include <new>

extern "C" {

// Generated factory for the SpeedRpm datatype.
//   structural-hash = SpeedRpm::kHashId (0x2c5f8d1a) -> 000000002c5f8d1a
//   attribute-hash  = 0 (no attributes fixed at compile time) -> 0000000000000000
// mw::com resolves this symbol by its composed name; an unknown datatype simply has no matching
// symbol, so dlsym fails and Proxy/Skeleton creation fails.
score::mw::com::impl::someip::serialization::ISerializer* create_serializer_000000002c5f8d1a_0000000000000000() noexcept
{
    return new (std::nothrow) score::mw::com::impl::someip::serialization::SpeedRpmSerializer{};
}

void DestroySerializer(score::mw::com::impl::someip::serialization::ISerializer* serializer) noexcept
{
    delete serializer;
}

}  // extern "C"
