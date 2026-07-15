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
#ifndef SCORE_MW_COM_TUTORIAL_CHAPTER_12_COMMON_SERVICE_INTERFACE_H
#define SCORE_MW_COM_TUTORIAL_CHAPTER_12_COMMON_SERVICE_INTERFACE_H

// Service interface for the heap-free examples. Instantiate with AsSkeleton<SensorInterface> or
// AsProxy<SensorInterface>.

#include "score/mw/com/types.h"

#include <cstdint>

namespace sensor
{

struct SensorReading
{
    std::uint32_t sequence;
    float value;
};

// Service interface template. Trait resolves to SkeletonTrait or ProxyTrait.
template <typename Trait>
class SensorInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // Event: latest sensor reading. Skeleton sends; proxy receives.
    typename Trait::template Event<SensorReading> reading{*this, "reading"};

    typename Trait::template Field<std::uint32_t, score::mw::com::WithNotifier> calibration_status{
        *this,
        "calibration_status"};
};

using SensorSkeleton = score::mw::com::AsSkeleton<SensorInterface>;
using SensorProxy = score::mw::com::AsProxy<SensorInterface>;

}  // namespace sensor

#endif  // SCORE_MW_COM_TUTORIAL_CHAPTER_12_COMMON_SERVICE_INTERFACE_H
