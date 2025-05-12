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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_PROPERTIES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_PROPERTIES_H

#include <cstddef>

namespace score::mw::com::impl::lola
{

struct SkeletonEventProperties
{
    std::size_t number_of_slots;
    std::size_t max_subscribers;

    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
    // or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
    // defined sizes."
    // Rationale: False positive: bool is not a fixed width type, however, the layout of this struct is not important
    // since this struct is not used to interface with hardware / is never serialized. Each member is accessed and used
    // individually.
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    bool enforce_max_samples;
};

}  // namespace score::mw::com::impl::lola

#endif
