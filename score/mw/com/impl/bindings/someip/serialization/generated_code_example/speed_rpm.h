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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_H

#include "static_reflection_with_serialization/visitor/visit_as_struct.h"

#include <score/static_vector.hpp>

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Motor telemetry sample carrying the current speed, engine RPM and a speed history.
///
/// This is the datatype generator output for the ExampleService::SpeedRpm event: the SampleType.
/// Producer, consumer and mw::com all include this header, so all three observe the same definition.
///
/// The datatype is made visitable, which lets the SomeIpStrategy serialize it field by field. Every
/// dynamic member is capacity-bounded, which bounds the serialized size (see GetMaxSize).
struct SpeedRpm
{
    static constexpr std::size_t kMaxSpeedSamples = 100U;

    float speed_kmh;
    std::uint16_t rpm;
    score::cpp::static_vector<float, kMaxSpeedSamples> speed_samples;
};

SCORE_STRUCT_VISITABLE(SpeedRpm, speed_kmh, rpm, speed_samples)

}  // namespace score::mw::com::impl::someip::serialization

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_GENERATED_CODE_EXAMPLE_SPEED_RPM_H
