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
#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm_serializer.h"

#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_max_size.h"
#include "score/mw/com/impl/serialization/serializer_abi.h"

#include "score/serialization/error/serialization_error.h"

#include <type_traits>
#include <utility>

namespace score::mw::com::impl::someip::serialization
{

using impl::serialization::SerializerErrc;
using score::serialization::SerializationError;

score::Result<std::size_t> SpeedRpmSerializer::Serialize(const void* sample,
                                                         score::cpp::span<std::uint8_t> buffer) const noexcept
{
    if (sample == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kInvalidArgument);
    }

    const auto written = serializer_.Serialize(*static_cast<const SpeedRpm*>(sample), buffer);
    if (!written.has_value())
    {
        // The error domain of the serialization library must not cross the library boundary, so every
        // failure is reported as SerializerErrc. The strategy writes the buffer front to back and reports an
        // out-of-bounds write once it runs out of space.
        if (written.error() == MakeError(SerializationError::kOutOfBoundsWrite))
        {
            return MakeUnexpected(SerializerErrc::kBufferTooSmall);
        }
        return MakeUnexpected(SerializerErrc::kInternalError, "serialization failed");
    }

    return written.value();
}

score::Result<void> SpeedRpmSerializer::Deserialize(score::cpp::span<const std::uint8_t> buffer,
                                                    void* sample) const noexcept
{
    if (sample == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kInvalidArgument);
    }

    // The strategy appends to bounded containers, so decode into a fresh object. This also leaves the caller's
    // sample untouched when decoding fails.
    SpeedRpm decoded{};
    const auto read = serializer_.DeSerialize(buffer, decoded);
    if (!read.has_value())
    {
        if (read.error() == MakeError(SerializationError::kDeserializedCountExceedsCapacity))
        {
            return MakeUnexpected(SerializerErrc::kDeserializationFailed, "speed_samples length out of range");
        }
        return MakeUnexpected(SerializerErrc::kDeserializationFailed, "buffer truncated or malformed");
    }

    *static_cast<SpeedRpm*>(sample) = std::move(decoded);
    return {};
}

score::Result<std::size_t> SpeedRpmSerializer::GetMaxSerializedSize() const noexcept
{
    return score::serialization::GetMaxSize<SpeedRpm>();
}

}  // namespace score::mw::com::impl::someip::serialization

// Every generated library exports the descriptor of the environment it was built in. mw::com compares it with
// its own before it resolves or calls anything else in the library. A single declaration is used because only
// then the constexpr variable gets external linkage.
extern "C" constexpr score::mw::com::impl::serialization::SerializerAbiInfo mwcom_serializer_abi_info =
    score::mw::com::impl::serialization::MakeSerializerAbiInfo();

extern "C" {

// Every generated library exports one factory function per serializer variant, resolved by mw::com via
// dlsym. The symbol name encodes the structural hash of the datatype and the hash of the attributes
// fixed at compile time:
//   ISerializer* CreateSerializer_<structural-hash>_<attribute-hash>(<other-attributes>) noexcept;
// <other-attributes> are the attributes configured at runtime (none for this variant). The factory
// returns a heap-allocated ISerializer, or nullptr if construction fails. The caller owns it and
// releases it with DestroySerializer, so allocation and deallocation happen on the same side.
score::mw::com::impl::serialization::ISerializer* CreateSerializer_2c5f8d1a_7e3a91c4() noexcept
{
    return score::mw::com::impl::serialization::CreateSerializerOf<
        score::mw::com::impl::someip::serialization::SpeedRpmSerializer>();
}

void DestroySerializer(score::mw::com::impl::serialization::ISerializer* serializer) noexcept
{
    delete serializer;
}

}  // extern "C"

// mw::com resolves the entry points with the function types of the ABI header. Asserting them here makes a
// signature that drifts from the contract a compile error instead of undefined behaviour at runtime.
static_assert(std::is_same_v<decltype(&CreateSerializer_2c5f8d1a_7e3a91c4),
                             score::mw::com::impl::serialization::CreateSerializerFn>,
              "CreateSerializer_<suffix> does not match the factory signature of the ABI.");
static_assert(std::is_same_v<decltype(&DestroySerializer), score::mw::com::impl::serialization::DestroySerializerFn>,
              "DestroySerializer does not match the destroy signature of the ABI.");
