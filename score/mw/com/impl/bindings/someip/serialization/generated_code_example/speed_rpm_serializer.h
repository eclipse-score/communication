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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_SERIALIZER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_SERIALIZER_H

#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_strategy.h"
#include "score/mw/com/impl/serialization/serializer.h"

#include "score/serialization/serializer.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::serialization::someip
{

/// \brief ISerializer implementation for the SpeedRpm datatype, built on the SomeIpStrategy.
///
/// The strategy runs with its default layout: big-endian, packed (no alignment padding) and 4 byte
/// length fields. These are the attributes fixed at compile time, which the attribute hash in the name of
/// the exported factory function stands for.
///
/// Wire layout:
///   [0..3]   speed_kmh          (float)
///   [4..5]   rpm                (uint16)
///   [6..9]   speed_samples size (uint32 length field, counting the bytes of speed_samples data)
///   [10..]   speed_samples data (size / sizeof(float) floats)
class SpeedRpmSerializer final : public ISerializer
{
  public:
    score::Result<std::size_t> Serialize(const void* sample, score::cpp::span<std::uint8_t> buffer) const noexcept override;
    score::Result<void> Deserialize(score::cpp::span<const std::uint8_t> buffer, void* sample) const noexcept override;
    score::Result<std::size_t> GetMaxSerializedSize() const noexcept override;

  private:
    Serializer<SomeIpStrategy> serializer_{SomeIpStrategy{}};
};

}  // namespace score::serialization::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_SERIALIZER_H
