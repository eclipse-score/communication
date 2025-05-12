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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SUBSCRIPTION_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SUBSCRIPTION_CONTROL_H

#include "score/memory/shared/atomic_indirector.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include <atomic>
#include <cstdint>

namespace score::mw::com::impl::lola
{
template <class EventSubscriptionControl>
class EventSubscriptionControlAttorney;

enum class SubscribeResult : std::uint8_t
{
    kSuccess,                 ///< the subscribe call with the given amount of samples was successful
    kMaxSubscribersOverflow,  ///< the subscribe call was rejected because of maximum allowed subscribers would
                              ///< overflow
    kSlotOverflow,            ///< the subscribe call was rejected because of maximum slots would overflow
    kUpdateRetryFailure,      ///< the subscribe call was rejected because updating the atomic subscribe state via retry
                              ///< failed.
};

namespace detail_event_subscription_control
{

/// \brief EventSubscriptionControlImpl encapsulates subscription state of an Event/Field. It is stored in Shared
/// Memory.
///
/// \details Underlying EventSubscriptionControlImpl holds the subscription state (currently subscribed slots, current
///          number of subscribers) in an atomic member and also max slots and subscribers as constants. It provides
///          functionality to subscribe/unsubscribe in a lock-free manner.
///          template arg AtomicIndirectorType is used for testing to enable mocking of std::atomic functionality.
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class EventSubscriptionControlImpl final
{
    template <class EventSubscriptionControl>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "EventSubscriptionControlAttorney" class is a helper, which sets the internal state of
    // "EventSubscriptionControlImpl" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class lola::EventSubscriptionControlAttorney;

  public:
    /// \brief Represents the type for the number of sample slots - lola deployment is the master of this type.
    using SlotNumberType = LolaEventInstanceDeployment::SampleSlotCountType;
    /// \brief Represents the type for the number of sample slots - needs to be in sync with
    ///        LolaEventInstanceDeployment::max_subscribers_
    using SubscriberCountType = std::uint8_t;

    /// \brief Will construct EventSubscriptionControlImpl
    /// \param max_slot_count maximum/initial number of subscribable slots.
    /// \param max_subscribers maximum number of allowed subscribers.
    EventSubscriptionControlImpl(const SlotNumberType max_slot_count,
                                 const SubscriberCountType max_subscribers,
                                 const bool enforce_max_samples) noexcept;

    /// \brief Subscribe with given number of slots
    /// \param slot_count number of slots to subscribe for
    /// \return subscribe result
    SubscribeResult Subscribe(SlotNumberType slot_count) noexcept;

    /// \brief Unsubscribe with given number of slots
    /// \param slot_count number of slots to unsubscribe
    void Unsubscribe(SlotNumberType slot_count) noexcept;

  private:
    /// \brief holds the current number of subscribed slots and the number of current subscribers combined.
    std::atomic_uint32_t current_subscription_state_;
    const SlotNumberType max_subscribable_slots_;
    const SubscriberCountType max_subscribers_;
    const bool enforce_max_samples_;
};

}  // namespace detail_event_subscription_control

std::string_view ToString(SubscribeResult subscribe_result) noexcept;

using EventSubscriptionControl = detail_event_subscription_control::EventSubscriptionControlImpl<>;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SUBSCRIPTION_CONTROL_H
