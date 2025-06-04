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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_COMMON_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_COMMON_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/slot_collector.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/string_view.hpp>
#include <score/utility.hpp>

#include <mutex>

namespace score::mw::com::impl::lola
{

/// \brief Type agnostic part of proxy event binding implementation for the Lola IPC binding
///
/// This class instantiates the SubscriptionStateMachine and forwards user calls to it. During subscription, the
/// state machine instantiates a SlotCollector whose ownership is then passed to this class. When the user calls
/// GetNewSamplesSlotIndices(), the call is forwarded to the SlotCollector.
class ProxyEventCommon final
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "ProxyEventCommonAttorney" class is a helper, which sets the internal state of "ProxyEventCommon" accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyEventCommonAttorney;

  public:
    ProxyEventCommon(Proxy& parent, const ElementFqId element_fq_id, const score::cpp::string_view event_name);
    ~ProxyEventCommon();

    ProxyEventCommon(const ProxyEventCommon&) = delete;
    ProxyEventCommon(ProxyEventCommon&&) noexcept = delete;
    ProxyEventCommon& operator=(const ProxyEventCommon&) = delete;
    ProxyEventCommon& operator=(ProxyEventCommon&&) noexcept = delete;

    ResultBlank Subscribe(const std::size_t max_sample_count);
    void Unsubscribe();

    SubscriptionState GetSubscriptionState() const noexcept;

    /// \brief Returns the number of new samples a call to GetNewSamples() would currently provide if the
    /// max_sample_count set in the Subscribe call and GetNewSamples call were both infinitely high.
    /// \see ProxyEvent::GetNumNewSamplesAvailable() for details.
    ///
    /// The call is dispatched to SlotCollector. It is the responsibility of the calling code to ensure that
    /// GetNumNewSamplesAvailable() is only called when the event is in the subscribed state.
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept;

    /// \brief Get the indicators of the slots containing samples that are pending for reception in ascending order.
    ///        I.e. returned SlotIndicators begin with the oldest slots/events (lowest timestamp) first and end at the
    ///        newest/youngest (largest timestamp) slots.
    ///
    /// The call is dispatched to SlotCollector. It is the responsibility of the calling code to ensure that
    /// GetNewSamplesSlotIndices() is only called when the event is in the subscribed state.
    SlotCollector::SlotIndicators GetNewSamplesSlotIndices(const std::size_t max_count) noexcept;

    ResultBlank SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler);
    ResultBlank UnsetReceiveHandler();

    pid_t GetEventSourcePid() const noexcept;
    ElementFqId GetElementFQId() const noexcept
    {
        return event_fq_id_;
    };
    EventControl& GetEventControl() const noexcept
    {
        return event_control_;
    };
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept;
    score::cpp::optional<TransactionLogSet::TransactionLogIndex> GetTransactionLogIndex() const noexcept;
    void NotifyServiceInstanceChangedAvailability(const bool is_available, const pid_t new_event_source_pid) noexcept;

  private:
    /// \brief Manually insert a slot collector. Only used for tests.
    // Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
    // namespace, or static function with internal linkage, or private member function shall be used.".
    // Used for testing purposes.
    // coverity[autosar_cpp14_a0_1_3_violation]
    void InjectSlotCollector(SlotCollector&& slot_collector)
    {
        score::cpp::ignore = test_slot_collector_.emplace(std::move(slot_collector));
    };
    score::cpp::optional<SlotCollector> test_slot_collector_;

    Proxy& parent_;
    ElementFqId event_fq_id_;
    const score::cpp::string_view event_name_;
    TransactionLogId transaction_log_id_;
    EventControl& event_control_;
    SubscriptionStateMachine subscription_event_state_machine_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_COMMON_H
