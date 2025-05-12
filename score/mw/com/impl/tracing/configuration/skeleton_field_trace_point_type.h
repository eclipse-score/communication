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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SKELETON_FIELD_TRACE_POINT_TYPE_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SKELETON_FIELD_TRACE_POINT_TYPE_H

#include <cstdint>

namespace score::mw::com::impl::tracing
{

enum class SkeletonFieldTracePointType : std::uint8_t
{
    INVALID = 0U,
    UPDATE,
    UPDATE_WITH_ALLOCATE,
    GET_CALL,
    GET_CALL_RESULT,
    SET_CALL,
    SET_CALL_RESULT,
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SKELETON_FIELD_TRACE_POINT_TYPE_H
