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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_PROPERTIES_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_PROPERTIES_H

#include <cstddef>

namespace score::mw::com::impl::someip
{

/// \brief Deployment derived properties of a SOME/IP skeleton event (resp. field).
///
/// \details Mirrors lola::SkeletonEventProperties minus the members which only exist because LoLa slots live in
///          shared memory (IPC tracing slots) or serve the not-yet-supported field getter/setter.
class SkeletonEventProperties
{
  public:
    SkeletonEventProperties(std::size_t number_of_slots, std::size_t max_subscribers_in, bool enforce_max_samples_in)
        : max_subscribers(max_subscribers_in),
          enforce_max_samples(enforce_max_samples_in),
          number_of_slots_(number_of_slots)
    {
    }

    /// \brief Returns the total number of slots configured for the event (or field).
    [[nodiscard]] std::size_t GetTotalNumberOfSlots() const noexcept
    {
        return number_of_slots_;
    }

    std::size_t max_subscribers;

    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with
    // hardware or conforming to communication protocols shall be trivial, standard-layout and only contain members of
    // types with defined sizes."
    // Rationale: False positive: this struct is never serialized; each member is accessed individually.
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    bool enforce_max_samples;

  private:
    std::size_t number_of_slots_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_EVENT_PROPERTIES_H
