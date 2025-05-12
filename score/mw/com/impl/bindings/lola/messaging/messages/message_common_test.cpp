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
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"

#include "score/mw/com/message_passing/message.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(MessageCommon, ShortMsgPayloadToElementFqId)
{
    // given a ShortMessagePayload
    auto message_payload = message_passing::ShortMessagePayload{0x22110055E0E001};

    // when converting to an ElementFqId
    auto element_fq_id = ShortMsgPayloadToElementFqId(message_payload);

    // expect, that members reflect the payload
    EXPECT_EQ(element_fq_id.service_id_, 0x2211) << std::hex << message_payload;
    EXPECT_EQ(element_fq_id.element_id_, 0x55);
    EXPECT_EQ(element_fq_id.instance_id_, 0xE0E0);
    EXPECT_EQ(static_cast<std::uint8_t>(element_fq_id.element_type_), 0x01);
}

TEST(MessageCommon, ElementFqIdToShortMsgPayload)
{
    // given an ElementFqId
    auto element_fq_id = ElementFqId{0x1111, 0x15, 0x1301, 0x01};

    // when converting to a ShortMessagePayload
    auto message_payload = ElementFqIdToShortMsgPayload(element_fq_id);

    // expect, that payload reflects the element_fq_id members
    EXPECT_EQ(message_payload, 0x11110015130101);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
