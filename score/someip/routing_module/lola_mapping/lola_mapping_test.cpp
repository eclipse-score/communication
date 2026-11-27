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
#include "score/someip/routing_module/lola_mapping/lola_mapping.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace score::someip::routing_module
{
namespace
{

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

TEST(LolaMappingTest, NamesServiceInstancesAndEventsInFourDigitLowercaseHex)
{
    Trace("RoutingModule.CR_RoutingModule_LolaMapping",
          "SOME/IP service, instance and event IDs",
          "their LoLa instance specifier and event name are formed",
          "the IDs appear as four-digit lowercase hexadecimal numbers");

    EXPECT_EQ(LolaInstanceSpecifier(0x1234U, 0xABCDU), "someip/1234/abcd");
    EXPECT_EQ(LolaInstanceSpecifier(0x000FU, 0x0000U), "someip/000f/0000");
    EXPECT_EQ(LolaEventName(0x8001U), "event_8001");
    EXPECT_EQ(LolaEventName(0x00A0U), "event_00a0");
}

TEST(LolaMappingTest, EventSampleHasAFixedLayout)
{
    Trace("RoutingModule.CR_RoutingModule_LolaMapping",
          "the LoLa sample shared by the routing module and LoLa applications",
          "its layout is inspected",
          "size, session, interface version and message type precede a 1400-byte payload at offset 8");

    EXPECT_EQ(offsetof(EventSample, size), 0U);
    EXPECT_EQ(offsetof(EventSample, session), 4U);
    EXPECT_EQ(offsetof(EventSample, interface_version), 6U);
    EXPECT_EQ(offsetof(EventSample, message_type), 7U);
    EXPECT_EQ(offsetof(EventSample, payload), 8U);
    EXPECT_EQ(sizeof(EventSample::payload), 1400U);
}

}  // namespace
}  // namespace score::someip::routing_module
