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

#include "score/mw/com/types.h"

namespace score::mw::com::tutorial
{
// Starting with this chapter we use a different service interface than in the previous chapters: instead of an event
// ("message") the TirePressureService exposes four *fields*. A field is like an event "on steroids": in addition to the
// pure event semantics (notification of new values via a notifier) it carries a *current value*. The provider therefore
// has to supply an initial value for every field before it may offer the service.
//
// Each of the four fields carries the current tire pressure (in bar) of one wheel. All fields are declared with the
// WithNotifier tag only. WithNotifier enables the notifier part of the proxy-side field API (Subscribe, GetNewSamples,
// SetReceiveHandler, GetSubscriptionState, ...) - which is exactly the subset we need here to receive the field value
// updates. (WithGetter / WithSetter, which enable the request/response style Get()/Set() on a field, are covered in a
// later chapter.)
template <typename Trait>
class TirePressureInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

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
}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_TIRE_PRESSURE_SERVICE_H
