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
#ifndef SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_SERIALIZER_H
#define SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_SERIALIZER_H

#include "score/serialization/someip/generated_code_example/speed_rpm.h"
#include "score/serialization/someip/serializer.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::serialization::someip
{

/// \brief ISerializer implementation for the SpeedRpm datatype.
///
/// Wire layout (big-endian, SOME/IP):
///   [0..3]   speed_kmh          (float)
///   [4..5]   rpm                (uint16)
///   [6..9]   speed_samples size (uint32 length prefix)
///   [10..]   speed_samples data (size * float)
class SpeedRpmSerializer final : public ISerializer
{
  public:
    score::Result<void> Serialize(const void* sample, score::cpp::span<std::uint8_t> buffer) const noexcept override;
    score::Result<void> Deserialize(score::cpp::span<const std::uint8_t> buffer, void* sample) const noexcept override;
    score::Result<std::size_t> GetMaxSerializedSize() const noexcept override;

  private:
    // speed_kmh (4) + rpm (2) + length prefix (4).
    static constexpr std::size_t kFixedFieldsSize = 10U;
    static constexpr std::uint32_t kMaxSpeedSamples = 100U;
    static constexpr std::size_t kMaxSerializedSize = kFixedFieldsSize + (kMaxSpeedSamples * sizeof(float));
};

}  // namespace score::serialization::someip

#endif  // SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_SERIALIZER_H
