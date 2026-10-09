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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SPEED_RPM_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SPEED_RPM_H

#include <cstdint>
#include <vector>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Motor telemetry sample carrying the current speed, engine RPM and a speed history.
///
/// This is the datatype generator output for the ExampleService::SpeedRpm event: the SampleType plus
/// its structural hash. Producer, consumer and mw::com all include this header, so all three observe
/// the same kHashId.
struct SpeedRpm
{
    static constexpr std::uint64_t kHashId = 0x2c5f8d1aULL;

    float speed_kmh;
    std::uint16_t rpm;
    std::vector<float> speed_samples;
};

}  // namespace score::mw::com::impl::someip::serialization

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SPEED_RPM_H
