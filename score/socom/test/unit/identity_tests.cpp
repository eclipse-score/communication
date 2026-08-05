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

#include "gtest/gtest.h"
#include "score/socom/runtime.hpp"

using namespace ::testing;

namespace score::socom {

struct Some_type {};

TEST(IdentityPlain, Construct) {
    Some_type instance{};
    EXPECT_NO_FATAL_FAILURE(Bridge_identity::make(instance));
}

class IdentityTest : public Test {
   protected:
    Some_type instance_1{};
    Some_type instance_2{};
    Bridge_identity identity_1{Bridge_identity::make(instance_1)};
    Bridge_identity identity_2{Bridge_identity::make(instance_2)};
};

TEST_F(IdentityTest, CompareEqual) {
    EXPECT_TRUE(identity_1 == identity_1);
    EXPECT_FALSE(identity_1 == identity_2);
}

TEST_F(IdentityTest, CompareNotEqual) {
    EXPECT_FALSE(identity_1 != identity_1);
    EXPECT_TRUE(identity_1 != identity_2);
}

}  // namespace score::socom
