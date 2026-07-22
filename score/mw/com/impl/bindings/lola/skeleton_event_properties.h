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

class SkeletonEventProperties
{
  public:
    SkeletonEventProperties(std::size_t number_of_slots,
                            std::size_t number_of_tracing_slots,
                            std::size_t number_of_field_getter_slots,
                            bool is_setter_enabled,
                            std::size_t max_subscribers_in,
                            bool enforce_max_samples_in)
        : max_subscribers(max_subscribers_in),
          enforce_max_samples(enforce_max_samples_in),
          number_of_slots_(number_of_slots),
          number_of_tracing_slots_(number_of_tracing_slots),
          number_of_field_getter_slots_(number_of_field_getter_slots),
          is_setter_enabled_(is_setter_enabled)
    {
    }

    /// \brief Returns the total number of slots configured for the event (or field) configured by the user (via
    /// numberOfSampleSlots and numberOfIpcTracingSlots configuration properties) and required to support field Getter
    /// fuctionality in case a SkeletonField has WithGetter enabled.
    [[nodiscard]] std::size_t GetTotalNumberOfSlots() const
    {
        const std::size_t number_of_field_setter_slots = is_setter_enabled_ ? 1 : 0;
        return number_of_slots_ + number_of_tracing_slots_ + number_of_field_getter_slots_ +
               number_of_field_setter_slots;
    }

    /// \brief Returns the number of slots required to support Getter functionality.
    ///
    /// This will return 0 for an event or field without WithGetter enabled. For a field with WithGetter enabled, this
    /// will return the number of slots required to support the maximum number of concurrent Getter calls.
    [[nodiscard]] std::size_t GetNumberOfFieldGetterSlots() const
    {
        return number_of_field_getter_slots_;
    }

    std::size_t max_subscribers;

    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
    // or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
    // defined sizes."
    // Rationale: False positive: bool is not a fixed width type, however, the layout of this struct is not important
    // since this struct is not used to interface with hardware / is never serialized. Each member is accessed and used
    // individually.
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    bool enforce_max_samples;

  private:
    /// \brief The number of slots configured by the user for the event (or field) via numberOfSampleSlots configuration
    /// property.
    ///
    /// For an event, this is the number of slots that the provider needs to provide its consumers with. E.g. if the
    /// provider has 2 consumers which each want to access 2 samples, this value would be 4 (or 5 if they want to ensure
    /// that the provider can always provide a sample a new value even if all other consumers are using their samples).
    /// For a field with a Notifier enabled, the same logic applies as for an event. For a field without a Notifier
    /// enabled (which by definition has a Getter enabled), this value in general can be 0..n:
    ///     0 - If the maximum number of concurrent Getter calls are currently being called, then a call to Allocate()
    ///     will return an error (This allows the provider to reduce the memory footprint by reducing the slot vector
    ///     size. This could be useful if the provider doesn't want to update the value for example so they don't have
    ///     to worry about being blocked by Getter calls).
    ///     1 - If the maximum number of concurrent Getter calls are currently being called, then a call to Allocate()
    ///     will always succeed (This is the general, non-power use case).
    ///     n - If the maximum number of concurrent Getter calls are currently being called, then n SampleAllocateePtrs
    ///     from calls to Allocate can be held at one time. Once n SampleAllocateePtrs are held, then a call to
    ///     Allocate() will return an error.
    std::size_t number_of_slots_;

    /// \brief The number of slots configured by the user for the event (or field) via numberOfIpcTracingSlots
    /// configuration
    std::size_t number_of_tracing_slots_;

    /// \brief The number of slots that are required to support the maximum number of concurrent Getter calls for a
    /// field with a Getter enabled.
    ///
    /// The maximum number of concurrent Getter calls is defined by the static constexpr
    /// kMaxConcurrentFieldGetterSamplePtrs in SkeletonEventCommon. This value is not configured by the user but rather
    /// chosen by the implementation of a field on the lola binding level.
    std::size_t number_of_field_getter_slots_;

    // if setter enabled: add 1 slot (because this is different to allocate in that setter is called by consumer process
    // so provider cannot control calls and we don't want consumers to block provider).

    /// \brief Indicates whether the field has a Setter enabled which will require an additional slot to be allocated.
    ///
    /// We currently only support a single setter call at a time as we don't see a reasonable use case for multiple
    /// concurrent setter calls. So we use a flag instead of a number of slots required to make this distinction
    /// clearer.
    bool is_setter_enabled_;
};

}  // namespace score::mw::com::impl::lola

#endif
