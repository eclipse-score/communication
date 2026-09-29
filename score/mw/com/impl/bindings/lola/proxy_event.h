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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_H

#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/slot_collector.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/result/result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief Proxy event binding implementation for the Lola IPC binding.
///
/// All subscription operations are implemented in the separate class SubscriptionStateMachine and the associated
/// states. All type agnostic proxy event operations are dispatched to the class ProxyEventCommon.
///
class ProxyEvent final : public GenericProxyEventBinding
{
    // coverity[autosar_cpp14_a11_3_1_violation] friend to test class; is used to access meta_info_/InjectSlotCollector
    friend class ProxyEventAttorney;

  public:
    using typename ProxyEventBinding::Callback;

    ProxyEvent() = delete;
    /// Create a new instance that is bound to the specified ShmBindingInformation and ElementId.
    ///
    /// \param parent Parent proxy of the proxy event.
    /// \param element_fq_id The ID of the event inside the proxy type.
    /// \param event_name The name of the event inside the proxy type.
    ProxyEvent(Proxy& parent, const ElementFqId element_fq_id, const std::string_view event_name);

    ProxyEvent(const ProxyEvent&) = delete;
    ProxyEvent(ProxyEvent&&) noexcept = delete;
    ProxyEvent& operator=(const ProxyEvent&) = delete;
    ProxyEvent& operator=(ProxyEvent&&) noexcept = delete;

    ~ProxyEvent() noexcept override = default;

    Result<void> Subscribe(const std::size_t max_sample_count) noexcept override;
    void Unsubscribe() noexcept override;

    SubscriptionState GetSubscriptionState() const noexcept override;
    Result<std::size_t> GetNumNewSamplesAvailable() const override;
    Result<std::size_t> GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) noexcept override;

    Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept override;
    Result<void> UnsetReceiveHandler() noexcept override;
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept override;
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept override;
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override;
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    }

    /// \brief Notifies the event that the provider service instance that it is connected to (i.e. the
    ///        SkeletonEvent) has changed its availability.
    /// \param is_available true if the provider service instance has changed from being unavailable to available.
    ///        false if the providing service instance has changed from being available to unavailable.
    /// \param new_event_source_pid new pid of provider service instance.
    ///
    /// This is called by the lola::Proxy which begins a StartFindService search on construction for the provider
    /// service instance. When the service instance changes availability, it triggers a callback that calls
    /// NotifyServiceInstanceChangedAvailability for all service elements contained within the Proxy.
    ///
    /// \note This is not part of the binding-independent ProxyEventBinding/GenericProxyEventBinding interface, since
    /// identifying the (re-)connected provider service instance via its PID is a LoLa specific concept (e.g. a
    /// network based binding would never use an ECU local PID to identify a remote event source). It is therefore
    /// only used/called by lola::Proxy.
    void NotifyServiceInstanceChangedAvailability(bool is_available, pid_t new_event_source_pid) noexcept;

    memory::DataTypeSizeInfo GetDataTypeSizeInfo() const override
    {
        return meta_info_.data_type_info_;
    }
    bool HasSerializedFormat() const noexcept override
    {
        return false;
    }

    ElementFqId GetElementFQId() const noexcept
    {
        return event_fq_id_;
    }

  private:
    /// \brief Get the indicators of the slots containing samples that are pending for reception in ascending order.
    ///        I.e. returned SlotIndices begin with the oldest slots/events (lowest timestamp) first and end at the
    ///        newest/youngest (largest timestamp) slots.
    ///
    /// The call is dispatched to SlotCollector. It is the responsibility of the calling code to ensure that
    /// GetNewSamplesSlotIndices() is only called when the event is in the subscribed state.
    SlotCollector::SlotIndices GetNewSamplesSlotIndices(const std::size_t max_count);

    const EventMetaInfo& meta_info_;
    const EventDataStorage& event_data_storage_;

    /// \brief Manually insert a slot collector. Only used for tests.
    // Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
    // namespace, or static function with internal linkage, or private member function shall be used.".
    // Used for testing purposes.
    // coverity[autosar_cpp14_a0_1_3_violation]
    void InjectSlotCollector(SlotCollector&& slot_collector)
    {
        score::cpp::ignore = test_slot_collector_.emplace(std::move(slot_collector));
    };

    std::optional<SlotCollector> test_slot_collector_;

    Proxy& parent_;
    ElementFqId event_fq_id_;
    const std::string_view event_name_;
    TransactionLogId transaction_log_id_;
    ConsumerEventDataControlLocalView<> event_data_control_local_;
    std::reference_wrapper<EventSubscriptionControl<>> subscription_control_;
    std::reference_wrapper<TransactionLogSet> transaction_log_set_;
    SubscriptionStateMachine subscription_event_state_machine_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_H
