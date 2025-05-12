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
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

TEST(SkeletonEventTracePointTypeTest, DefaultConstructedEnumValueIsInvalid)
{
    // Given a default constructed SkeletonEventTracePointType
    SkeletonEventTracePointType skeleton_event_trace_point_type{};

    // Then the value of the enum should be invalid
    EXPECT_EQ(skeleton_event_trace_point_type, SkeletonEventTracePointType::INVALID);
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
