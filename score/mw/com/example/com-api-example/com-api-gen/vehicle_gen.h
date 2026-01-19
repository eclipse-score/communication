/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#ifndef SCORE_MW_COM_EXAMPLE_COM_API_EXAMPLE_VEHICLE_DATATYPE_H
#define SCORE_MW_COM_EXAMPLE_COM_API_EXAMPLE_VEHICLE_DATATYPE_H

#include "score/mw/com/types.h"

namespace score::mw::com
{

// Simple data structure for Tire
struct Tire
{
    float pressure;
};

// Simple data structure for Exhaust
struct Exhaust
{
};

// Vehicle interface template
template <typename Trait>
class VehicleInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<Tire> left_tire{*this, "left_tire"};
    typename Trait::template Event<Exhaust> exhaust{*this, "exhaust"};
};

// Proxy and Skeleton type aliases
using VehicleProxy = AsProxy<VehicleInterface>;
using VehicleSkeleton = AsSkeleton<VehicleInterface>;

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_EXAMPLE_COM_API_EXAMPLE_VEHICLE_DATATYPE_H
