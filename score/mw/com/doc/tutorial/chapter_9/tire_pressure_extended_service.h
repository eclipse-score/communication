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
#ifndef SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_EXTENDED_SERVICE_H
#define SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_EXTENDED_SERVICE_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <type_traits>

namespace score::mw::com::tutorial
{

// Identifies one of the four tires of the vehicle. It is the data type of the "tire_pressure_warning" event: whenever a
// tire's pressure leaves the currently configured threshold band, the provider sends one such event carrying the
// affected tire. The enum has a fixed-size underlying type (std::uint8_t) so that it is trivially copyable and can be
// transported through shared memory.
enum class Tire : std::uint8_t
{
    front_left,
    front_right,
    rear_left,
    rear_right,
};

// The value type of the "tire_pressure_thresholds" field. It describes the pressure band a tire pressure is expected to
// stay within. As it is the data type of a field with a getter and a setter (i.e. it is transported as a method
// argument/return value through shared memory), it must be trivially copyable: it therefore only aggregates two floats.
struct TirePressureThreshold
{
    float lower_threshold;
    float upper_threshold;
};

static_assert(std::is_trivially_copyable_v<TirePressureThreshold>,
              "TirePressureThreshold must be trivially copyable to be usable as a field (method) data type.");

// This chapter is based on the TirePressureService of chapter 7, but extends it to demonstrate the request/response
// style field accessors Get() (WithGetter) and Set() (WithSetter) - which are, semantically, just methods on a field.
//
// The four per-wheel tire pressure fields are now declared with the WithGetter tag *instead of* WithNotifier: the
// consumer no longer subscribes to receive value updates event-driven, but actively reads the current value via Get()
// whenever it needs it. In addition there is a fifth field "tire_pressure_thresholds" which carries the current
// threshold band and is declared WithGetter *and* WithSetter, so that a consumer can both read and modify it. Finally,
// an event "tire_pressure_warning" is used by the provider to notify consumers about a tire whose pressure has left the
// current threshold band.
template <typename Trait>
class TirePressureExtendedInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Field<float, score::mw::com::WithGetter> tire_pressure_front_left{
        *this,
        "tire_pressure_front_left"};
    typename Trait::template Field<float, score::mw::com::WithGetter> tire_pressure_front_right{
        *this,
        "tire_pressure_front_right"};
    typename Trait::template Field<float, score::mw::com::WithGetter> tire_pressure_rear_left{
        *this,
        "tire_pressure_rear_left"};
    typename Trait::template Field<float, score::mw::com::WithGetter> tire_pressure_rear_right{
        *this,
        "tire_pressure_rear_right"};

    // The threshold band a consumer may both read (WithGetter) and adjust (WithSetter).
    typename Trait::template Field<TirePressureThreshold, score::mw::com::WithGetter, score::mw::com::WithSetter>
        tire_pressure_thresholds{*this, "tire_pressure_thresholds"};

    // Notifies consumers about a tire whose current pressure has left the configured threshold band.
    typename Trait::template Event<Tire> tire_pressure_warning{*this, "tire_pressure_warning"};
};

using TirePressureExtendedProxy = score::mw::com::AsProxy<TirePressureExtendedInterface>;
using TirePressureExtendedSkeleton = score::mw::com::AsSkeleton<TirePressureExtendedInterface>;

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_EXTENDED_SERVICE_H
