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
#include "score/mw/com/impl/serialization/serialization_request.h"

#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

#include <string>

namespace score::mw::com::impl::serialization
{
namespace
{

InstanceSpecifier MakeInstanceSpecifier(const std::string& value)
{
    auto instance_specifier = InstanceSpecifier::Create(std::string{value});
    return std::move(instance_specifier).value();
}

TEST(SerializationRequestTest, CarriesTheInstanceSpecifierTheBindingOptedInFor)
{
    const auto instance_specifier = MakeInstanceSpecifier("/some/service/instance");

    const SerializationRequest request{instance_specifier};

    EXPECT_EQ(request.GetInstanceSpecifier(), instance_specifier);
}

TEST(SerializationRequestTest, KeepsDistinctInstanceSpecifiersSeparate)
{
    const SerializationRequest first_request{MakeInstanceSpecifier("/service/one")};
    const SerializationRequest second_request{MakeInstanceSpecifier("/service/two")};

    EXPECT_NE(first_request.GetInstanceSpecifier(), second_request.GetInstanceSpecifier());
    EXPECT_EQ(first_request.GetInstanceSpecifier(), "/service/one");
    EXPECT_EQ(second_request.GetInstanceSpecifier(), "/service/two");
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
