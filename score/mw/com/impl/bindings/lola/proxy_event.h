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
#include "score/mw/com/impl/bindings/lola/proxy_event_common.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/string_view.hpp>

#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>
#include <variant>

namespace score::mw::com::impl::lola
{

/// \brief Proxy event binding implementation for the Lola IPC binding.
///
/// All subscription operations are implemented in the separate class SubscriptionStateMachine and the associated
/// states. All type agnostic proxy event operations are dispatched to the class ProxyEventCommon.
///
/// \tparam SampleType Data type that is transmitted
template <typename SampleType>
class ProxyEvent final : public ProxyEventBinding<SampleType>
{
    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation] friend to test class; is used to access proxy_event_common_
    friend class ProxyEventAttorney;

  public:
    using typename ProxyEventBinding<SampleType>::Callback;

    ProxyEvent() = delete;
    /// Create a new instance that is bound to the specified ShmBindingInformation and ElementId.
    ///
    /// \param parent Parent proxy of the proxy event.
    /// \param element_fq_id The ID of the event inside the proxy type.
    /// \param event_name The name of the event inside the proxy type.
    ProxyEvent(Proxy& parent, const ElementFqId element_fq_id, const score::cpp::string_view event_name)
        : ProxyEventBinding<SampleType>{},
          proxy_event_common_{parent, element_fq_id, event_name},
          samples_{parent.GetEventDataStorage<SampleType>(element_fq_id)}
    {
    }

    ProxyEvent(const ProxyEvent&) = delete;
    ProxyEvent(ProxyEvent&&) noexcept = delete;
    ProxyEvent& operator=(const ProxyEvent&) = delete;
    ProxyEvent& operator=(ProxyEvent&&) noexcept = delete;

    ~ProxyEvent() noexcept override = default;

    ResultBlank Subscribe(const std::size_t max_sample_count) noexcept override
    {
        return proxy_event_common_.Subscribe(max_sample_count);
    }
    void Unsubscribe() noexcept override
    {
        proxy_event_common_.Unsubscribe();
    }

    SubscriptionState GetSubscriptionState() const noexcept override
    {
        return proxy_event_common_.GetSubscriptionState();
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept override;
    Result<std::size_t> GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) noexcept override;

    ResultBlank SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept override
    {
        return proxy_event_common_.SetReceiveHandler(std::move(handler));
    }
    ResultBlank UnsetReceiveHandler() noexcept override
    {
        return proxy_event_common_.UnsetReceiveHandler();
    }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override
    {
        return proxy_event_common_.GetMaxSampleCount();
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    }
    void NotifyServiceInstanceChangedAvailability(bool is_available, pid_t new_event_source_pid) noexcept override
    {
        proxy_event_common_.NotifyServiceInstanceChangedAvailability(is_available, new_event_source_pid);
    }

    pid_t GetEventSourcePid() const noexcept
    {
        return proxy_event_common_.GetEventSourcePid();
    }
    ElementFqId GetElementFQId() const noexcept
    {
        return proxy_event_common_.GetElementFQId();
    };

  private:
    Result<std::size_t> GetNewSamplesImpl(Callback&& receiver, TrackerGuardFactory& tracker) noexcept;
    Result<std::size_t> GetNumNewSamplesAvailableImpl() const noexcept;

    ProxyEventCommon proxy_event_common_;
    const EventDataStorage<SampleType>& samples_;
};

template <typename SampleType>
inline Result<std::size_t> ProxyEvent<SampleType>::GetNumNewSamplesAvailable() const noexcept
{
    /// \todo See comment in GetNewSamples() -> we can also dispatch to GetNumNewSamplesAvailableImpl() in case of
    ///       kSubscriptionPending.
    const auto subscription_state = proxy_event_common_.GetSubscriptionState();
    if (subscription_state == SubscriptionState::kSubscribed)
    {
        return GetNumNewSamplesAvailableImpl();
    }
    else
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNumNewSamplesAvailable without successful subscription.");
    }
}

template <typename SampleType>
inline Result<std::size_t> ProxyEvent<SampleType>::GetNumNewSamplesAvailableImpl() const noexcept
{
    return proxy_event_common_.GetNumNewSamplesAvailable();
}

template <typename SampleType>
inline Result<std::size_t> ProxyEvent<SampleType>::GetNewSamples(Callback&& receiver,
                                                                 TrackerGuardFactory& tracker) noexcept
{
    /// \todo In case of LoLa binding we can also dispatch to GetNewSamplesImpl() in case of kSubscriptionPending!
    ///       Because a pre-condition to kSubscriptionPending is, that we once had a successful subscribe.
    ///       ... and then we can always access the samples even if the provider went down.
    const auto subscription_state = proxy_event_common_.GetSubscriptionState();
    if (subscription_state == SubscriptionState::kSubscribed)
    {
        return GetNewSamplesImpl(std::move(receiver), tracker);
    }
    else
    {
        return MakeUnexpected(ComErrc::kNotSubscribed,
                              "Attempt to call GetNewSamples without successful subscription.");
    }
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 M3-2-2" rule finding. This rule declares: "The One Definition Rule shall not be
// violated.". False-positive, template method is defined only once.
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_m3_2_2_violation : FALSE]
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
inline Result<std::size_t> ProxyEvent<SampleType>::GetNewSamplesImpl(Callback&& receiver,
                                                                     TrackerGuardFactory& tracker) noexcept
{
    const auto max_sample_count = tracker.GetNumAvailableGuards();
    const auto slot_indices = proxy_event_common_.GetNewSamplesSlotIndices(max_sample_count);

    auto& event_control = proxy_event_common_.GetEventControl();
    auto transaction_log_index = proxy_event_common_.GetTransactionLogIndex();
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(transaction_log_index.has_value(),
                                 "GetNewSamplesImpl should only be called after a TransactionLog has been registered.");

    for (auto slot = slot_indices.begin; slot != slot_indices.end; ++slot)
    {
        const SampleType& sample_data{samples_.at(static_cast<std::size_t>(*slot))};
        const EventSlotStatus event_slot_status{event_control.data_control[*slot]};
        const EventSlotStatus::EventTimeStamp sample_timestamp{event_slot_status.GetTimeStamp()};

        SamplePtr<SampleType> sample{&sample_data, event_control.data_control, *slot, transaction_log_index.value()};

        auto guard = std::move(*tracker.TakeGuard());
        auto sample_binding_independent = this->MakeSamplePtr(std::move(sample), std::move(guard));

        static_assert(
            sizeof(EventSlotStatus::EventTimeStamp) == sizeof(impl::tracing::ITracingRuntime::TracePointDataId),
            "Event timestamp is used for the trace point data id, therefore, the types should be the same.");
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "I a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // we can't add noexcept to score::cpp::callback signature.
        // coverity[autosar_cpp14_a15_4_2_violation]
        receiver(std::move(sample_binding_independent),
                 static_cast<impl::tracing::ITracingRuntime::TracePointDataId>(sample_timestamp));
    }

    const auto num_collected_slots = static_cast<std::size_t>(std::distance(slot_indices.begin, slot_indices.end));
    return num_collected_slots;
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_H
