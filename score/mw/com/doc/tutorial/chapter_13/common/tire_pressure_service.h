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
#ifndef SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_SERVICE_H
#define SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_SERVICE_H

// Service interface for the heap-free examples. Based on the TirePressureService from
// chapter 7 (four per-wheel pressure fields with WithNotifier), extended with one event
// (tire_pressure_update) to demonstrate heap-free event send and receive alongside
// heap-free field updates.
//
// Instantiate with AsSkeleton<TirePressureInterface> or AsProxy<TirePressureInterface>.

#include "score/mw/com/types.h"

namespace score::mw::com::tutorial
{

template <typename Trait>
class TirePressureInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // Event: signals a new tire pressure reading (front-left wheel, in bar).
    // The skeleton calls Allocate() and Send(); the proxy calls Subscribe() and GetNewSamples().
    typename Trait::template Event<float> tire_pressure_update{*this, "tire_pressure_update"};

    // Fields: current tire pressure per wheel (in bar).
    // WithNotifier enables Subscribe() and GetNewSamples() on the proxy side.
    // The examples use tire_pressure_front_left as the representative field for
    // field-update demonstrations.
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_front_left{
        *this,
        "tire_pressure_front_left"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_front_right{
        *this,
        "tire_pressure_front_right"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_rear_left{
        *this,
        "tire_pressure_rear_left"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_rear_right{
        *this,
        "tire_pressure_rear_right"};
};

using TirePressureSkeleton = score::mw::com::AsSkeleton<TirePressureInterface>;
using TirePressureProxy = score::mw::com::AsProxy<TirePressureInterface>;

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_SERVICE_H
