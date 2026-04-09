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
#ifndef SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_SERVICE_DATA_CONTROL_LOCAL_VIEW_H
#define SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_SERVICE_DATA_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_control_local_view.h"

#include <score/assert.hpp>

#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// SkeletonServiceDataControlLocalView is a view class which stores views to the individual objects in shared memory.
///
/// This class will be created by a Skeleton on PrepareOffer. It provides a local view into all the elements within
/// ServiceDataControl. Since the memory layout of ServiceDataControl will never change after construction (e.g. the
/// event_controls_ map will never be resized after construction), each Skeleton can simply create a
/// SkeletonServiceDataControlLocalView to avoid accessing the shared memory via OffsetPtr. Additional information about
/// the performance impacts of OffsetPtr are described in EventDataControl.
class SkeletonServiceDataControlLocalView
{
  public:
    explicit SkeletonServiceDataControlLocalView(ServiceDataControl& service_data_control) : event_controls_{}
    {
        for (auto& element : service_data_control.event_controls_)
        {
            const auto insertion_result = event_controls_.emplace(
                std::piecewise_construct, std::forward_as_tuple(element.first), std::forward_as_tuple(element.second));

            const bool was_inserted = insertion_result.second;
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(was_inserted);
        }
    }

    ~SkeletonServiceDataControlLocalView() noexcept = default;

    SkeletonServiceDataControlLocalView(const SkeletonServiceDataControlLocalView&) = delete;
    SkeletonServiceDataControlLocalView& operator=(const SkeletonServiceDataControlLocalView&) = delete;
    SkeletonServiceDataControlLocalView(SkeletonServiceDataControlLocalView&&) noexcept = delete;
    SkeletonServiceDataControlLocalView& operator=(SkeletonServiceDataControlLocalView&& other) noexcept = delete;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, SkeletonEventControlLocalView> event_controls_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_SERVICE_DATA_CONTROL_LOCAL_VIEW_H
