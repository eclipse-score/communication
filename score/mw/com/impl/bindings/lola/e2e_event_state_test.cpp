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
#include "score/mw/com/impl/bindings/lola/e2e_event_state.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr E2EEventTypeDeployment kE2EDeployment{E2EProfile::kP4, 0x3AU, 2U};

TEST(E2EEventStateTest, NoStateWithoutDeployment)
{
    // Given a header storage but no deployment
    const auto header_storage = std::make_shared<e2e::HeaderStorage>(1U);

    // When creating the E2E state
    const auto state = MakeE2EEventState<e2e::CheckContext>(std::nullopt, header_storage);

    // Then there is no state
    EXPECT_FALSE(state.has_value());
}

TEST(E2EEventStateTest, NoStateWithoutHeaderStorage)
{
    // Given a deployment but no header storage
    // When creating the E2E state
    const auto state = MakeE2EEventState<e2e::CheckContext>(kE2EDeployment, nullptr);

    // Then there is no state
    EXPECT_FALSE(state.has_value());
}

TEST(E2EEventStateTest, StateTakesProfileConfigurationFromDeploymentAndSharesHeaderStorage)
{
    // Given a deployment and a header storage
    const auto header_storage = std::make_shared<e2e::HeaderStorage>(1U);

    // When creating the E2E state
    const auto state = MakeE2EEventState<e2e::CheckContext>(kE2EDeployment, header_storage);

    // Then the state shares the header storage and carries the deployment's data ID and max delta counter
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->header_storage, header_storage);
    EXPECT_EQ(state->profile_configuration.data_id, kE2EDeployment.data_id_);
    EXPECT_EQ(state->profile_configuration.max_delta_counter, kE2EDeployment.max_delta_counter_);
}

TEST(E2EEventStateTest, StateStartsWithFreshContext)
{
    // Given a deployment and a header storage
    const auto header_storage = std::make_shared<e2e::HeaderStorage>(1U);

    // When creating the provider-side and the consumer-side E2E state
    const auto protect_state = MakeE2EEventState<e2e::ProtectContext>(kE2EDeployment, header_storage);
    const auto check_state = MakeE2EEventState<e2e::CheckContext>(kE2EDeployment, header_storage);

    // Then both contexts are in their initial state
    ASSERT_TRUE(protect_state.has_value());
    ASSERT_TRUE(check_state.has_value());
    EXPECT_EQ(protect_state->context.next_counter, 0U);
    EXPECT_FALSE(check_state->context.has_last_counter);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
