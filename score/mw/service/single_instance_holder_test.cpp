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

#include "score/mw/service/single_instance_holder.h"

#include <gtest/gtest.h>
#include <cstdint>

namespace score::mw::service::test
{
namespace
{

using TestType = std::uint32_t;

TEST(SingleInstanceHolder, CanConstructSingleInstanceHolder)
{
    // When constructing a SingleInstanceHolder with SingleInstanceHolderCreator::Construct
    const std::uint32_t constructed_value{14U};
    const auto single_instance_holder = SingleInstanceHolderCreator<TestType>::Construct<TestType>(constructed_value);

    // Then the created instance should have been created with the arguments provided to the Construct call
    EXPECT_EQ(*single_instance_holder, constructed_value);
}

}  // namespace
}  // namespace score::mw::service::test
