/*********************************************************************************
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
 *******************************************************************************/

#include "score/mw/service/backend/common/port_identifier_checking.h"

#include <gtest/gtest.h>

namespace score::mw::service::backend::common::test
{
namespace
{

struct PortIdentifierGood
{
    constexpr static auto Get()
    {
        return "Some/Instance/Specifier";
    }
};

struct PortIdentifierWrongGetReturnType
{
    constexpr static auto Get()
    {
        return 10;
    }
};

struct PortIdentifierMissingGet
{
};

TEST(PortIdentifierCheckingTest, ReturnsTrueGivenATypeWithCorrectGetSignature)
{
    // Given a PortIdentifier type which contains Get() with the expected signature

    // When calling IsValidPortIdentifierType
    const auto is_valid = IsValidPortIdentifierType<PortIdentifierGood>();

    // Then the result is true
    EXPECT_TRUE(is_valid);
}

TEST(PortIdentifierCheckingTest, ReturnsFalseGivenATypeWithIncorrectGetSignature)
{
    // Given a PortIdentifier type which contains Get() with the wrong return type

    // When calling IsValidPortIdentifierType
    const auto is_valid = IsValidPortIdentifierType<PortIdentifierWrongGetReturnType>();

    // Then the result is false
    EXPECT_FALSE(is_valid);
}

TEST(PortIdentifierCheckingTest, ReturnsFalseGivenATypeWithoutGet)
{
    // Given a PortIdentifier type which does not contain Get()

    // When calling IsValidPortIdentifierType
    const auto is_valid = IsValidPortIdentifierType<PortIdentifierMissingGet>();

    // Then the result is false
    EXPECT_FALSE(is_valid);
}

}  // namespace
}  // namespace score::mw::service::backend::common::test
