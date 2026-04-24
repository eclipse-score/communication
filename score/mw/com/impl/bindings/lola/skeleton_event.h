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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief Represents a binding specific instance (LoLa) of an event within a skeleton. It can be used to send events
/// via Shared Memory. It will be created via a Factory Method, that will instantiate this class based on deployment
/// values.
///
/// This class is _not_ user-facing.
///
/// All operations on this class are _not_ thread-safe, in a manner that they shall not be invoked in parallel by
/// different threads.
template <typename SampleType>
class SkeletonEvent final : public SkeletonEventBinding<SampleType>
{
    template <typename T>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonEventAttorney;

  public:
    using typename SkeletonEventBindingBase::SubscribeTraceCallback;
    using typename SkeletonEventBindingBase::UnsubscribeTraceCallback;

    SkeletonEvent(Skeleton& parent,
                  const ElementFqId element_fq_id,
                  const std::string_view event_name,
                  const SkeletonEventProperties properties,
                  impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data = {}) noexcept;

    SkeletonEvent(const SkeletonEvent&) = delete;
    SkeletonEvent(SkeletonEvent&&) noexcept = delete;
    SkeletonEvent& operator=(const SkeletonEvent&) & = delete;
    SkeletonEvent& operator=(SkeletonEvent&&) & noexcept = delete;

    ~SkeletonEvent() override = default;

    /// \brief Sends a value by _copy_ towards a consumer. It will allocate the necessary space and then copy the value
    /// into Shared Memory.
    Result<void> Send(const SampleType& value,
                      std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>
                          send_trace_callback) noexcept override;

    Result<void> Send(impl::SampleAllocateePtr<SampleType> sample,
                      std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>
                          send_trace_callback) noexcept override;

    Result<impl::SampleAllocateePtr<SampleType>> Allocate() noexcept override;

    /// @requirement SWS_CM_00700
    Result<void> PrepareOffer() noexcept override;

    void PrepareStopOffer() noexcept override;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kLoLa;
    }

    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        skeleton_event_common_.SetSkeletonEventTracingData(tracing_data);
    }

  private:
    EventDataStorage<SampleType>* event_data_storage_;
    SkeletonEventCommon<SampleType> skeleton_event_common_;
};

template <typename SampleType>
SkeletonEvent<SampleType>::SkeletonEvent(Skeleton& parent,
                                         const ElementFqId element_fq_id,
                                         const std::string_view event_name,
                                         const SkeletonEventProperties properties,
                                         impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data) noexcept
    : SkeletonEventBinding<SampleType>{},
      event_data_storage_{nullptr},
      skeleton_event_common_{parent, event_name, properties, element_fq_id, skeleton_event_tracing_data}
{
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'allocated_slot_result.value()' in case the
// 'allocated_slot_result' doesn't have value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<void> SkeletonEvent<SampleType>::Send(
    const SampleType& value,
    std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> send_trace_callback) noexcept
{
    auto allocated_slot_result = Allocate();
    if (!(allocated_slot_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kSampleAllocationFailure, "Could not allocate slot");
    }
    auto allocated_slot = std::move(allocated_slot_result).value();
    *allocated_slot = value;

    return Send(std::move(allocated_slot), std::move(send_trace_callback));
}

template <typename SampleType>
Result<void> SkeletonEvent<SampleType>::Send(
    impl::SampleAllocateePtr<SampleType> sample,
    std::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> send_trace_callback) noexcept
{
    const auto send_result = skeleton_event_common_.Send(sample);
    if (!send_result.has_value())
    {
        return MakeUnexpected<void>(send_result.error());
    }

    if (send_trace_callback.has_value())
    {
        (*send_trace_callback)(sample);
    }
    return send_result;
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from '.value()' in case it doesn't have value but as we check
// before with 'has_value()' so no way for throwing std::bad_optional_access, which leads to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<impl::SampleAllocateePtr<SampleType>> SkeletonEvent<SampleType>::Allocate() noexcept
{
    const auto allocated_slot_result = skeleton_event_common_.AllocateSlot();
    if (!allocated_slot_result.has_value())
    {
        return MakeUnexpected<impl::SampleAllocateePtr<SampleType>>(allocated_slot_result.error());
    }

    const auto slot_index = allocated_slot_result.value();
    return MakeSampleAllocateePtr(
        SampleAllocateePtr<SampleType>(&event_data_storage_->at(static_cast<std::uint64_t>(slot_index)),
                                       skeleton_event_common_.GetEventDataControlComposite(),
                                       slot_index));
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<void> SkeletonEvent<SampleType>::PrepareOffer() noexcept
{
    const auto registration_result = skeleton_event_common_.GetParent().template Register<SampleType>(
        skeleton_event_common_.GetElementFQId(), skeleton_event_common_.GetEventProperties());
    event_data_storage_ = &registration_result.event_data_storage;

    skeleton_event_common_.PrepareOfferCommon(registration_result.event_data_control_composite);

    return {};
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonEvent<SampleType>::PrepareStopOffer() noexcept
{
    skeleton_event_common_.PrepareStopOfferCommon();
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_H
