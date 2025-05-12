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
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/mw/log/logging.h"

namespace score::mw::com::impl::lola
{
namespace detail_event_subscription_control
{
namespace
{

inline EventSubscriptionControlImpl<>::SubscriberCountType GetSubscribersFromState(
    std::uint32_t subscription_state) noexcept
{
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is an in purpose casting to get the subscribers from the subscription state.
    // coverity[autosar_cpp14_a4_7_1_violation]
    return static_cast<EventSubscriptionControlImpl<>::SubscriberCountType>(subscription_state >> 16U);
}

inline EventSubscriptionControlImpl<>::SlotNumberType GetSubscribedSamplesFromState(
    std::uint32_t subscription_state) noexcept
{
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is an in purpose casting to get the subscribed samples from the subscription state.
    // coverity[autosar_cpp14_a4_7_1_violation]
    return static_cast<std::uint16_t>(subscription_state & 0x0000FFFFU);
}

inline std::uint32_t CreateState(EventSubscriptionControlImpl<>::SubscriberCountType subscriber_count,
                                 EventSubscriptionControlImpl<>::SlotNumberType subscribed_slots)
{
    std::uint32_t result{subscriber_count};
    result = result << 16U;
    result += subscribed_slots;
    return result;
}

}  // namespace

template <template <class> class AtomicIndirectorType>
EventSubscriptionControlImpl<AtomicIndirectorType>::EventSubscriptionControlImpl(
    const SlotNumberType max_slot_count,
    const SubscriberCountType max_subscribers,
    const bool enforce_max_samples) noexcept
    : current_subscription_state_{0U},
      max_subscribable_slots_{max_slot_count},
      max_subscribers_{max_subscribers},
      enforce_max_samples_{enforce_max_samples}
{
}

template <template <class> class AtomicIndirectorType>
auto EventSubscriptionControlImpl<AtomicIndirectorType>::Subscribe(SlotNumberType slot_count) noexcept
    -> SubscribeResult
{
    // Suppress "AUTOSAR C++14 M5-0-8" rule finding. This rule declares: "An explicit integral or
    // floating-point conversion shall not increase the size of the underlying type of a cvalue expression.".
    // This is false positive, no increase in size happened.
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is false positive, This will not led to data loss.
    // coverity[autosar_cpp14_m5_0_8_violation : FALSE]
    // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
    const auto max_retries = static_cast<std::uint16_t>(2U * max_subscribers_);

    for (auto current_retry = 0U; current_retry < max_retries; ++current_retry)
    {

        auto current_state = current_subscription_state_.load();
        const auto current_subscribers = GetSubscribersFromState(current_state);
        if (current_subscribers >= max_subscribers_)
        {
            mw::log::LogInfo("lola")
                << "EventSubscriptionControlImpl::Subscribe() rejected as already max_subscribers_ are subscribed.";
            return SubscribeResult::kMaxSubscribersOverflow;
        }
        SlotNumberType current_subscribed_slots = GetSubscribedSamplesFromState(current_state);
        if (enforce_max_samples_ && (current_subscribed_slots + slot_count > max_subscribable_slots_))
        {
            mw::log::LogInfo("lola")
                << "EventSubscriptionControlImpl::Subscribe() rejected as max_subscribable_slots_ would overflow.";
            return SubscribeResult::kSlotOverflow;
        }

        std::uint32_t new_state = CreateState(static_cast<SubscriberCountType>(current_subscribers + 1U),
                                              // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An
                                              // integer expression shall not lead to data loss.". The check above
                                              // ensures that the addition result will not exceed its maximum value for
                                              // std::uint16_t type.
                                              // coverity[autosar_cpp14_a4_7_1_violation]
                                              static_cast<SlotNumberType>(current_subscribed_slots + slot_count));
        auto success = AtomicIndirectorType<std::uint32_t>::compare_exchange_weak(
            current_subscription_state_, current_state, new_state, std::memory_order_acq_rel);
        if (success)
        {
            return SubscribeResult::kSuccess;
        }
    }
    return SubscribeResult::kUpdateRetryFailure;
}

template <template <class> class AtomicIndirectorType>
auto EventSubscriptionControlImpl<AtomicIndirectorType>::Unsubscribe(SlotNumberType slot_count) noexcept -> void
{
    /// some heuristics for retry count: we take into account max_subscribers_ as one dimension of the likelihood of
    /// a concurrent try to change the atomic state. The factor in front is resembling the "activity" of this subscriber
    /// on the subscription state, reflecting the frequency of calling subscribe/unsubscribe.
    // Suppress "AUTOSAR C++14 M5-0-8" rule finding. This rule declares: "An explicit integral or
    // floating-point conversion shall not increase the size of the underlying type of a cvalue expression.".
    // This is false positive, no increase in size happened.
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
    // not lead to data loss.".
    // This is false positive, This will not led to data loss.
    // coverity[autosar_cpp14_m5_0_8_violation : FALSE]
    // coverity[autosar_cpp14_a4_7_1_violation : FALSE]
    const auto max_retries = static_cast<std::uint16_t>(3U * max_subscribers_);

    for (auto current_retry = 0U; current_retry < max_retries; ++current_retry)
    {

        auto current_state = current_subscription_state_.load();
        auto current_subscribers = GetSubscribersFromState(current_state);
        if (current_subscribers == 0U)
        {
            mw::log::LogFatal("lola")
                << "EventSubscriptionControlImpl::Unsubscribe() Current subscriber count is already 0!";
            std::terminate();
        }
        SlotNumberType current_subscribed_slots = GetSubscribedSamplesFromState(current_state);
        if (current_subscribed_slots < slot_count)
        {
            mw::log::LogFatal("lola")
                << "EventSubscriptionControlImpl::Unsubscribe() rejected as currently subscribed slots "
                   "are smaller than slot_count.";
            std::terminate();
        }

        std::uint32_t new_state = CreateState(static_cast<SubscriberCountType>(current_subscribers - 1U),
                                              static_cast<SlotNumberType>(current_subscribed_slots - slot_count));
        auto success = AtomicIndirectorType<std::uint32_t>::compare_exchange_weak(
            current_subscription_state_, current_state, new_state, std::memory_order_acq_rel);
        if (success)
        {
            return;
        }
    }
    mw::log::LogFatal("lola")
        << "EventSubscriptionControlImpl::Unsubscribe() retry limit exceeded, couldn't unsubscribe!";
    std::terminate();
}

template class EventSubscriptionControlImpl<memory::shared::AtomicIndirectorReal>;
template class EventSubscriptionControlImpl<memory::shared::AtomicIndirectorMock>;

}  // namespace detail_event_subscription_control

std::string_view ToString(SubscribeResult subscribe_result) noexcept
{
    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the end of default case as we use return.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (subscribe_result)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case SubscribeResult::kSuccess:
            return "success";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case SubscribeResult::kMaxSubscribersOverflow:
            return "Max subcribers overflow";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case SubscribeResult::kSlotOverflow:
            return "Slot overflow";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        case SubscribeResult::kUpdateRetryFailure:
            return "Update retry failure";
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause.
        default:
            return "Unknown SubscribeResult value";
    }
}

}  // namespace score::mw::com::impl::lola
