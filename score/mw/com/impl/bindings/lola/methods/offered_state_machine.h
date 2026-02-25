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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_OFFERED_STATE_MACHINE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_OFFERED_STATE_MACHINE_H

#include <array>
#include <cstdint>
#include <memory>

namespace score::mw::com::impl::lola
{
namespace detail
{

class IOfferedState;
class OfferedState;
class StopOfferedState;
class ReOfferedState;

}  // namespace detail

/// \brief State machine representing the current service offered status of a service in the context of Proxy
/// auto-reconnect.
///
/// The state machine records whether a service has been offered (this is the initial state), stop offered or re-offered
/// (i.e. the service was stop offered and then offered again). This is used in Proxy auto-reconnect to ensure that we
/// only resend a SubscribeServiceMethod message via message passing in the proxy's FindService handler when the
/// skeleton has crashed (i.e. stop offered) and restarted (i.e. re-offered)
class OfferedStateMachine final
{
  public:
    enum class State : std::uint8_t
    {
        OFFERED = 0,
        STOP_OFFERED,
        RE_OFFERED
    };

    OfferedStateMachine();

    void Offer();
    void StopOffer();

    State GetCurrentState() const
    {
        return current_state_;
    }

  private:
    State current_state_;

    /// \brief Array of states on which state machine events can be called.
    ///
    /// The currently active state is retrieved by indexing into the array using current_state_. Therefore, the order of
    /// states in states_ must match the order of states in the State enum.
    std::array<std::unique_ptr<detail::IOfferedState>, 3U> states_;
};

namespace detail
{

class IOfferedState
{
  public:
    virtual ~IOfferedState() = default;
    virtual OfferedStateMachine::State Offer() = 0;
    virtual OfferedStateMachine::State StopOffer() = 0;
};

class OfferedState : public IOfferedState
{
  public:
    OfferedStateMachine::State Offer() override;
    OfferedStateMachine::State StopOffer() override;
};

class StopOfferedState : public IOfferedState
{
  public:
    OfferedStateMachine::State Offer() override;
    OfferedStateMachine::State StopOffer() override;
};

class ReOfferedState : public IOfferedState
{
  public:
    OfferedStateMachine::State Offer() override;
    OfferedStateMachine::State StopOffer() override;
};

}  // namespace detail
}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_OFFERED_STATE_MACHINE_H
