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
#ifndef SCORE_MW_COM_EXAMPLES_COMMON_SENSOR_INTERFACE_H
#define SCORE_MW_COM_EXAMPLES_COMMON_SENSOR_INTERFACE_H

// Shared typed service interface used by all four heap-free examples.
// Instantiate with AsSkeleton<SensorInterface> or AsProxy<SensorInterface>.

#include "score/mw/com/types.h"

#include <cstdint>

namespace examples
{

// Payload for the "reading" event.
struct SensorReading
{
    std::uint32_t sequence;  // monotonically increasing counter
    float value;             // sensor measurement for that cycle
};

// Service interface template. Trait resolves to SkeletonTrait or ProxyTrait.
template <typename Trait>
class SensorInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // Event: latest sensor reading. Skeleton sends; proxy receives.
    typename Trait::template Event<SensorReading> reading{*this, "reading"};

    // Field: calibration status code.
    // WithNotifier — proxy can subscribe and receive updates via GetNewSamples.
    // Skeleton must call Update() before OfferService() to set the initial value.
    typename Trait::template Field<std::uint32_t, score::mw::com::WithNotifier> calibration_status{
        *this,
        "calibration_status"};
};

using SensorSkeleton = score::mw::com::AsSkeleton<SensorInterface>;
using SensorProxy = score::mw::com::AsProxy<SensorInterface>;

}  // namespace examples

#endif  // SCORE_MW_COM_EXAMPLES_COMMON_SENSOR_INTERFACE_H
