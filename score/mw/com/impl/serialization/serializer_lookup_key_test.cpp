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
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::serialization
{
namespace
{

TEST(SerializerLookupKeyTest, ExposesTheHashesItWasBuiltFrom)
{
    const SerializerLookupKey key{0xABCDU, 0x12U};

    EXPECT_EQ(key.GetStructuralHash(), 0xABCDU);
    EXPECT_EQ(key.GetAttributeHash(), 0x12U);
}

TEST(SerializerLookupKeyTest, ComposesTheDocumentedFactorySymbolName)
{
    const SerializerLookupKey key{0xABCDU, 0x12U};

    EXPECT_EQ(key.FactorySymbolName(), "create_serializer_000000000000abcd_0000000000000012");
}

TEST(SerializerLookupKeyTest, ZeroHashesProduceZeroPaddedName)
{
    const SerializerLookupKey key{0U, 0U};

    EXPECT_EQ(key.FactorySymbolName(), "create_serializer_0000000000000000_0000000000000000");
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
