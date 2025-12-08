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
#include "score/mw/com/impl/bindings/lola/methods/offered_state_machine.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>
#include <memory>

namespace score::mw::com::impl::lola
{
namespace
{

class OfferedStateMachineFixture : public ::testing::Test
{
  public:
    OfferedStateMachineFixture& GivenAnOfferedStateMachine()
    {
        offered_state_machine_ = std::make_unique<OfferedStateMachine>();
        return *this;
    }

    OfferedStateMachineFixture& WhichIsInStopOfferedState()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(offered_state_machine_ != nullptr);
        offered_state_machine_->StopOffer();
        return *this;
    }

    OfferedStateMachineFixture& WhichIsInReOfferedState()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(offered_state_machine_ != nullptr);
        offered_state_machine_->StopOffer();
        offered_state_machine_->Offer();
        return *this;
    }

    std::unique_ptr<OfferedStateMachine> offered_state_machine_{nullptr};
};

using OfferedStateFixture = OfferedStateMachineFixture;
TEST_F(OfferedStateFixture, StateMachineIsInitiallyInOfferedState)
{
    GivenAnOfferedStateMachine();

    // When calling GetCurrentState
    const auto current_state = offered_state_machine_->GetCurrentState();

    // Then the state machine should be in OFFERED state
    EXPECT_EQ(current_state, OfferedStateMachine::State::OFFERED);
}

TEST_F(OfferedStateFixture, CallingOfferStaysInOfferedState)
{
    GivenAnOfferedStateMachine();

    // When calling Offer
    offered_state_machine_->Offer();

    // Then the state machine should still be in OFFERED state
    EXPECT_EQ(offered_state_machine_->GetCurrentState(), OfferedStateMachine::State::OFFERED);
}

TEST_F(OfferedStateFixture, CallingStopOfferTransitionsToStopOffered)
{
    GivenAnOfferedStateMachine();

    // When calling StopOffer
    offered_state_machine_->StopOffer();

    // Then the state machine should be in STOP_OFFERED state
    EXPECT_EQ(offered_state_machine_->GetCurrentState(), OfferedStateMachine::State::STOP_OFFERED);
}

using StopOfferedStateFixture = OfferedStateMachineFixture;
TEST_F(StopOfferedStateFixture, CallingOfferTransitionsToReOffered)
{
    GivenAnOfferedStateMachine().WhichIsInStopOfferedState();

    // When calling Offer
    offered_state_machine_->Offer();

    // Then the state machine should be in RE_OFFERED state
    EXPECT_EQ(offered_state_machine_->GetCurrentState(), OfferedStateMachine::State::RE_OFFERED);
}

TEST_F(StopOfferedStateFixture, CallingStopOfferTerminates)
{
    GivenAnOfferedStateMachine().WhichIsInStopOfferedState();

    // When calling StopOffer
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(offered_state_machine_->StopOffer());
}

using ReOfferedStateFixture = OfferedStateMachineFixture;
TEST_F(ReOfferedStateFixture, CallingOfferTerminates)
{
    GivenAnOfferedStateMachine().WhichIsInReOfferedState();

    // When calling Offer
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(offered_state_machine_->Offer());
}

TEST_F(ReOfferedStateFixture, CallingStopOfferTransitionsToStopOffered)
{
    GivenAnOfferedStateMachine().WhichIsInReOfferedState();

    // When calling StopOffer
    offered_state_machine_->StopOffer();

    // Then the state machine should be in STOP_OFFERED state
    EXPECT_EQ(offered_state_machine_->GetCurrentState(), OfferedStateMachine::State::STOP_OFFERED);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
