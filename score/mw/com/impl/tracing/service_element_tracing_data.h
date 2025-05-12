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
#ifndef SCORE_MW_COM_IMPL_TRACING_SERVICE_ELEMENT_TRACING_DATA_H
#define SCORE_MW_COM_IMPL_TRACING_SERVICE_ELEMENT_TRACING_DATA_H

#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include <cstdint>
namespace score::mw::com::impl::tracing
{

struct ServiceElementTracingData
{
    using SamplePointerIndex = LolaEventInstanceDeployment::SampleSlotCountType;
    using TracingSlotSizeType = LolaEventInstanceDeployment::TracingSlotSizeType;

    SamplePointerIndex service_element_range_start;
    TracingSlotSizeType number_of_service_element_tracing_slots;
};

inline bool operator==(const ServiceElementTracingData& lhs, const ServiceElementTracingData& rhs) noexcept
{
    return ((lhs.number_of_service_element_tracing_slots == rhs.number_of_service_element_tracing_slots) &&
            (lhs.service_element_range_start == rhs.service_element_range_start));
}

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_SERVICE_ELEMENT_TRACING_DATA_H
