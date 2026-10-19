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

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

OfferedStateMachine::OfferedStateMachine()
    : current_state_{State::OFFERED},
      states_{std::make_unique<detail::OfferedState>(),
              std::make_unique<detail::StopOfferedState>(),
              std::make_unique<detail::ReOfferedState>()}
{
}

void OfferedStateMachine::Offer()
{
    current_state_ = states_.at(static_cast<std::uint8_t>(current_state_))->Offer();
}

void OfferedStateMachine::StopOffer()
{
    current_state_ = states_.at(static_cast<std::uint8_t>(current_state_))->StopOffer();
}

namespace detail
{

OfferedStateMachine::State OfferedState::Offer()
{
    return OfferedStateMachine::State::OFFERED;
}

OfferedStateMachine::State OfferedState::StopOffer()
{
    return OfferedStateMachine::State::STOP_OFFERED;
}

OfferedStateMachine::State StopOfferedState::Offer()
{
    return OfferedStateMachine::State::RE_OFFERED;
}

OfferedStateMachine::State StopOfferedState::StopOffer()
{
    // Reachable in principle, but must not be reached in production: StopOffer() can be called while in
    // StopOfferedState. The Skeleton must not call StopOffer() twice without an Offer() in between in production.
    SCORE_LANGUAGE_FUTURECPP_UNREACHABLE();
}

OfferedStateMachine::State ReOfferedState::Offer()
{
    // Reachable in principle, but must not be reached in production: Offer() can be called while in ReOfferedState. The
    // Skeleton must not call Offer() twice without a StopOffer() in between in production.
    SCORE_LANGUAGE_FUTURECPP_UNREACHABLE();
}

OfferedStateMachine::State ReOfferedState::StopOffer()
{
    return OfferedStateMachine::State::STOP_OFFERED;
}

}  // namespace detail
}  // namespace score::mw::com::impl::lola
