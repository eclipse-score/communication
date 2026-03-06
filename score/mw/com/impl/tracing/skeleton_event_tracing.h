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
#ifndef SCORE_MW_COM_IMPL_SKELETON_EVENT_TRACING_H
#define SCORE_MW_COM_IMPL_SKELETON_EVENT_TRACING_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/lola/sample_ptr.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_allocatee_ptr.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/common_event_tracing.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include <score/optional.hpp>

#include <cstdint>
#include <string_view>

namespace score::mw::com::impl::tracing
{

namespace detail_skeleton_event_tracing
{

class TracingData
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule finding. This rule states: "Member data in non-POD class types shall be
    // private.". There is no need for too much overhead of having getter and setter for the private members, and
    // nothing violated by defining it as public.
    // coverity[autosar_cpp14_m11_0_1_violation]
    impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    const std::pair<const void*, std::size_t> shm_data_chunk{};
};

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TracingData ExtractBindingTracingData(const impl::SampleAllocateePtr<SampleType>& sample_data_ptr) noexcept
{
    const auto& binding_ptr_variant = SampleAllocateePtrView{sample_data_ptr}.GetUnderlyingVariant();
    auto visitor = score::cpp::overload(
        [](const lola::SampleAllocateePtr<SampleType>& lola_ptr) -> TracingData {
            const auto& event_data_control_composite =
                lola::SampleAllocateePtrView{lola_ptr}.GetEventDataControlComposite();
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_data_control_composite.has_value());
            const auto referenced_slot = lola_ptr.GetReferencedSlot();
            const auto sample_timestamp =
                event_data_control_composite->GetEventSlotTimestamp(referenced_slot.GetIndex());
            static_assert(
                sizeof(lola::EventSlotStatus::EventTimeStamp) ==
                    sizeof(impl::tracing::ITracingRuntime::TracePointDataId),
                "Event timestamp is used for the trace point data id, therefore, the types should be the same.");

            const auto trace_point_data_id =
                static_cast<impl::tracing::ITracingRuntime::TracePointDataId>(sample_timestamp);

            return {trace_point_data_id, {lola_ptr.get(), sizeof(SampleType)}};
        },
        // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
        // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
        // the function replaces the managed object".
        // Here we can't use a raw pointer / reference since we're using score::cpp::overload, and the function is not
        // replaceing the managed object, so this should be a const reference.
        // coverity[autosar_cpp14_a8_4_12_violation]
        [](const mock_binding::SampleAllocateePtr<SampleType>& ptr) -> TracingData {
            return {0U, {ptr.get(), sizeof(SampleType)}};
        },
        [](const score::cpp::blank&) noexcept -> TracingData {
            std::terminate();
        });
    return std::visit(visitor, binding_ptr_variant);
}

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
TypeErasedSamplePtr CreateTypeErasedSamplePtr(impl::SampleAllocateePtr<SampleType>& sample_data_ptr) noexcept
{
    auto& binding_ptr_variant = SampleAllocateePtrMutableView{sample_data_ptr}.GetUnderlyingVariant();
    auto visitor = score::cpp::overload(
        [](lola::SampleAllocateePtr<SampleType>& lola_ptr) -> TypeErasedSamplePtr {
            auto& event_data_control_composite_result =
                lola::SampleAllocateePtrMutableView{lola_ptr}.GetEventDataControlComposite();
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_data_control_composite_result.has_value());

            auto& event_data_control = event_data_control_composite_result->GetQmEventDataControl();

            auto slot_indicator = lola_ptr.GetReferencedSlot();
            event_data_control.ReferenceSpecificEvent(slot_indicator.GetIndex(),
                                                      lola::TransactionLogSet::kSkeletonIndexSentinel);
            const auto* const managed_object = lola::SampleAllocateePtrView{lola_ptr}.GetManagedObject();

            lola::SamplePtr<SampleType> sample_ptr{
                managed_object,
                event_data_control,
                lola::ControlSlotIndicator(slot_indicator.GetIndex(), slot_indicator.GetSlotQM()),
                lola::TransactionLogSet::kSkeletonIndexSentinel};
            return impl::tracing::TypeErasedSamplePtr{std::move(sample_ptr)};
        },
        [](mock_binding::SampleAllocateePtr<SampleType>& ptr) -> TypeErasedSamplePtr {
            impl::tracing::TypeErasedSamplePtr type_erased_sample_ptr{std::make_unique<SampleType>(*ptr)};
            return type_erased_sample_ptr;
        },
        // LCOV_EXCL_START (Defensive programming: CreateTypeErasedSamplePtr is always called after
        // ExtractBindingTracingData. If the SampleAllocateePtr contains a blank binding, then ExtractBindingTracingData
        // will terminate. Therefore, we will never reach this branch.
        [](score::cpp::blank&) noexcept -> TypeErasedSamplePtr {
            std::terminate();
        });
    // LCOV_EXCL_STOP

    return std::visit(visitor, binding_ptr_variant);
}

void UpdateTracingDataFromTraceResult(const ResultBlank trace_result,
                                      SkeletonEventTracingData& skeleton_event_tracing_data,
                                      bool& skeleton_event_trace_point) noexcept;

}  // namespace detail_skeleton_event_tracing

tracing::SkeletonEventTracingData GenerateSkeletonTracingStructFromEventConfig(
    const InstanceIdentifier& instance_identifier,
    const BindingType binding_type,
    const std::string_view event_name) noexcept;
tracing::SkeletonEventTracingData GenerateSkeletonTracingStructFromFieldConfig(
    const InstanceIdentifier& instance_identifier,
    const BindingType binding_type,
    const std::string_view field_name) noexcept;

template <typename SampleType>
void TraceSend(SkeletonEventTracingData& skeleton_event_tracing_data,
               const SkeletonEventBindingBase& skeleton_event_binding_base,
               impl::SampleAllocateePtr<SampleType>& sample_data_ptr) noexcept
{
    if (skeleton_event_tracing_data.enable_send)
    {
        const auto service_element_instance_identifier =
            skeleton_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;
        tracing::TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = tracing::SkeletonEventTracePointType::SEND;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = tracing::SkeletonFieldTracePointType::UPDATE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto tracing_data = detail_skeleton_event_tracing::ExtractBindingTracingData(sample_data_ptr);
        auto type_erased_sample_ptr = detail_skeleton_event_tracing::CreateTypeErasedSamplePtr(sample_data_ptr);

        const auto binding_type = skeleton_event_binding_base.GetBindingType();
        const auto service_element_tracing_data = skeleton_event_tracing_data.service_element_tracing_data;
        const auto trace_result = TraceShmData(binding_type,
                                               service_element_tracing_data,
                                               service_element_instance_identifier,
                                               trace_point,
                                               tracing_data.trace_point_data_id,
                                               std::move(type_erased_sample_ptr),
                                               tracing_data.shm_data_chunk);
        detail_skeleton_event_tracing::UpdateTracingDataFromTraceResult(
            trace_result, skeleton_event_tracing_data, skeleton_event_tracing_data.enable_send);
    }
}

template <typename SampleType>
void TraceSendWithAllocate(SkeletonEventTracingData& skeleton_event_tracing_data,
                           const SkeletonEventBindingBase& skeleton_event_binding_base,
                           impl::SampleAllocateePtr<SampleType>& sample_data_ptr) noexcept
{
    if (skeleton_event_tracing_data.enable_send_with_allocate)
    {
        const auto service_element_instance_identifier =
            skeleton_event_tracing_data.service_element_instance_identifier_view;
        const auto service_element_type =
            service_element_instance_identifier.service_element_identifier_view.service_element_type;
        tracing::TracingRuntime::TracePointType trace_point{};
        if (service_element_type == ServiceElementType::EVENT)
        {
            trace_point = tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE;
        }
        else if (service_element_type == ServiceElementType::FIELD)
        {
            trace_point = tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE;
        }
        else
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is false positive, the enum has more fields than EVENT and FIELD so we might reach this branch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Service element type must be EVENT or FIELD");
        }

        const auto tracing_data = detail_skeleton_event_tracing::ExtractBindingTracingData(sample_data_ptr);
        auto type_erased_sample_ptr = detail_skeleton_event_tracing::CreateTypeErasedSamplePtr(sample_data_ptr);

        const auto binding_type = skeleton_event_binding_base.GetBindingType();
        const auto service_element_tracing_data = skeleton_event_tracing_data.service_element_tracing_data;
        const auto trace_result = TraceShmData(binding_type,
                                               service_element_tracing_data,
                                               service_element_instance_identifier,
                                               trace_point,
                                               tracing_data.trace_point_data_id,
                                               std::move(type_erased_sample_ptr),
                                               tracing_data.shm_data_chunk);
        detail_skeleton_event_tracing::UpdateTracingDataFromTraceResult(
            trace_result, skeleton_event_tracing_data, skeleton_event_tracing_data.enable_send_with_allocate);
    }
}

template <typename SampleType>
auto CreateTracingSendCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                               const SkeletonEventBindingBase& skeleton_event_binding_base) noexcept
    -> score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>
{
    score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> tracing_handler{};
    if (skeleton_event_tracing_data.enable_send)
    {
        tracing_handler = [&skeleton_event_tracing_data, &skeleton_event_binding_base](
                              impl::SampleAllocateePtr<SampleType>& sample_data_ptr) mutable noexcept {
            TraceSend<SampleType>(skeleton_event_tracing_data, skeleton_event_binding_base, sample_data_ptr);
        };
    }
    return tracing_handler;
}

template <typename SampleType>
auto CreateTracingSendWithAllocateCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                                           const SkeletonEventBindingBase& skeleton_event_binding_base) noexcept
    -> score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback>
{
    score::cpp::optional<typename SkeletonEventBinding<SampleType>::SendTraceCallback> tracing_handler{};
    if (skeleton_event_tracing_data.enable_send_with_allocate)
    {
        tracing_handler = [&skeleton_event_tracing_data, &skeleton_event_binding_base](
                              impl::SampleAllocateePtr<SampleType>& sample_data_ptr) mutable noexcept {
            TraceSendWithAllocate<SampleType>(
                skeleton_event_tracing_data, skeleton_event_binding_base, sample_data_ptr);
        };
    }
    return tracing_handler;
}

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_TRACING_H
