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

score::mw::com::impl::someip::serialization::ISerializer* CreateSerializer(std::uint64_t datatype_hash) noexcept
{
    // A switch over the constexpr kHashId values makes every case a distinct compile-time constant.
    // If two datatypes ever hashed to the same kHashId, the duplicate case labels would be rejected
    // by the compiler, so a hash collision is caught at build time instead of silently returning the
    // wrong serializer at runtime.
    switch (datatype_hash)
    {
        case score::mw::com::impl::someip::serialization::SpeedRpm::kHashId:
            return new (std::nothrow) score::mw::com::impl::someip::serialization::SpeedRpmSerializer{};
        default:
            return nullptr;
    }
}

void DestroySerializer(score::mw::com::impl::someip::serialization::ISerializer* serializer) noexcept
{
    delete serializer;
}

}  // extern "C"
