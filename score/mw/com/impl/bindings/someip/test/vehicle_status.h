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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_TEST_VEHICLE_STATUS_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_TEST_VEHICLE_STATUS_H

#include "score/mw/com/impl/bindings/someip/someip_serializer.h"
#include "score/mw/com/impl/bindings/someip/someip_writer.h"
#include "score/mw/com/impl/sample_wire_format.h"
#include "score/mw/com/impl/serialization_sink.h"

#include <cstdint>

namespace score::mw::com::impl::someip::test
{

/// \brief Example event type showing why the raw object representation is not a wire representation.
/// \details In memory this is 16 bytes: the four std::uint16_t need 2 byte alignment, so the compiler inserts one
///          padding byte after `gear`. On the wire it is 15 bytes, big endian, without any padding.
struct WheelSpeeds
{
    std::uint16_t front_left;
    std::uint16_t front_right;
    std::uint16_t rear_left;
    std::uint16_t rear_right;
};

struct VehicleStatus
{
    std::uint32_t timestamp_ms;
    std::uint16_t speed_kmh;
    std::uint8_t gear;
    WheelSpeeds wheel_speeds;
};

}  // namespace score::mw::com::impl::someip::test

namespace score::mw::com::impl::someip
{

// Everything below is what a code generator would emit next to the type definition, so that naming the type always
// also makes its wire format visible.

/// \brief SOME/IP layout of WheelSpeeds. Takes the writer of the enclosing serialization, so nested members share
///        its state.
template <>
struct SomeIpSerializer<test::WheelSpeeds>
{
    static constexpr std::size_t kSerializedSize{8U};

    static bool Serialize(const test::WheelSpeeds& sample, SomeIpWriter& writer) noexcept
    {
        return writer.Write(sample.front_left) && writer.Write(sample.front_right) && writer.Write(sample.rear_left) &&
               writer.Write(sample.rear_right);
    }
};

/// \brief SOME/IP layout of VehicleStatus.
template <>
struct SomeIpSerializer<test::VehicleStatus>
{
    static constexpr std::size_t kSerializedSize{4U + 2U + 1U + SomeIpSerializer<test::WheelSpeeds>::kSerializedSize};

    static bool Serialize(const test::VehicleStatus& sample, SomeIpWriter& writer) noexcept
    {
        return writer.Write(sample.timestamp_ms) && writer.Write(sample.speed_kmh) && writer.Write(sample.gear) &&
               SomeIpSerializer<test::WheelSpeeds>::Serialize(sample.wheel_speeds, writer);
    }
};

}  // namespace score::mw::com::impl::someip

namespace score::mw::com::impl
{

/// \brief Entry point bridging the binding independent customization point to the SOME/IP format.
/// \details This is the single place where the writer is created; every nested serializer receives that same writer
///          by reference.
template <>
struct SampleWireFormat<someip::test::VehicleStatus>
{
    static constexpr std::size_t kMaxSerializedSize{
        someip::SomeIpSerializer<someip::test::VehicleStatus>::kSerializedSize};

    static bool Serialize(const someip::test::VehicleStatus& sample, ISerializationSink& sink) noexcept
    {
        someip::SomeIpWriter writer{sink};
        const bool serialized = someip::SomeIpSerializer<someip::test::VehicleStatus>::Serialize(sample, writer);
        return serialized && (!writer.HasFailed());
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_TEST_VEHICLE_STATUS_H
