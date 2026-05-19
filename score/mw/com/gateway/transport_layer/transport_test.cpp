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
#include "score/mw/com/gateway/transport_layer/transport.h"

#include <gtest/gtest.h>

#include <string>

namespace score::mw::com::gateway
{
namespace
{

TEST(ServiceElementConfigurationTest, SerializeMembersExposeMutableReferences)
{
    ServiceElementConfiguration config{"EventA", DataTypeSizeInfo{32U, 8U}};
    auto members = config.GetSerializeMembers();

    std::get<0>(members) = "EventB";
    std::get<1>(members) = DataTypeSizeInfo{64U, 8U};

    EXPECT_EQ(config.element_name, "EventB");
    EXPECT_EQ(config.size_info.Size(), 64U);
    EXPECT_EQ(config.size_info.Alignment(), 8U);
}

TEST(ServiceElementConfigurationTest, SerializeMembersConstOverloadReturnsConstReferences)
{
    const ServiceElementConfiguration config{"EventA", DataTypeSizeInfo{32U, 8U}};
    const auto members = config.GetSerializeMembers();

    EXPECT_EQ(std::get<0>(members), "EventA");
    EXPECT_EQ(std::get<1>(members).Size(), 32U);
    EXPECT_EQ(std::get<1>(members).Alignment(), 8U);
}

}  // namespace
}  // namespace score::mw::com::gateway
