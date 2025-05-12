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
#ifndef SCORE_MW_COM_IMPL_TRACING_SKELETON_EVENT_TRACING_DATA_H
#define SCORE_MW_COM_IMPL_TRACING_SKELETON_EVENT_TRACING_DATA_H

#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/service_element_tracing_data.h"

namespace score::mw::com::impl::tracing
{

class SkeletonEventTracingData
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementInstanceIdentifierView service_element_instance_identifier_view{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementTracingData service_element_tracing_data{};

    // coverity[autosar_cpp14_m11_0_1_violation]
    bool enable_send{false};
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool enable_send_with_allocate{false};
};

void DisableAllTracePoints(SkeletonEventTracingData& skeleton_event_tracing_data) noexcept;

inline bool operator==(const SkeletonEventTracingData& lhs, const SkeletonEventTracingData& rhs) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This a false-positive, all operands are parenthesized.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    return ((lhs.service_element_instance_identifier_view == rhs.service_element_instance_identifier_view) &&
            (lhs.service_element_tracing_data == rhs.service_element_tracing_data) &&
            (lhs.enable_send == rhs.enable_send) && (lhs.enable_send_with_allocate == rhs.enable_send_with_allocate));
}

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_SKELETON_EVENT_TRACING_DATA_H
