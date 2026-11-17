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
#ifndef SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_H
#define SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_H

#include <cstdint>
#include <vector>

namespace score::serialization::someip
{

/// \brief Motor telemetry sample carrying the current speed, engine RPM and a speed history.
///
/// This is the datatype generator output for the ExampleService::SpeedRpm event: the SampleType plus
/// its structural hash. Producer, consumer and mw::com all include this header, so all three observe
/// the same hashes.
struct SpeedRpm
{
    static constexpr std::uint64_t kStructuralHash = 0x2c5f8d1aULL;
    // Hash of the attributes fixed at compile time in the serializer (here: big-endian, packed).
    static constexpr std::uint64_t kAttributeHash = 0x7e3a91c4ULL;

    float speed_kmh;
    std::uint16_t rpm;
    std::vector<float> speed_samples;
};

}  // namespace score::serialization::someip

#endif  // SCORE_SERIALIZATION_SOMEIP_SPEED_RPM_H
