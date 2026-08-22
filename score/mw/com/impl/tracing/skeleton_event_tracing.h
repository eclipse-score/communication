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
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/lola/sample_ptr.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/common_event_tracing.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/result/result.h"

#include <score/overload.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace score::mw::com::impl::tracing
{

tracing::SkeletonEventTracingData GenerateSkeletonTracingStructFromEventConfig(
    const InstanceIdentifier& instance_identifier,
    const BindingType binding_type,
    const std::string_view event_name);
tracing::SkeletonEventTracingData GenerateSkeletonTracingStructFromFieldConfig(
    const InstanceIdentifier& instance_identifier,
    const BindingType binding_type,
    const std::string_view field_name);

auto CreateTracingSendCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                               memory::DataTypeSizeInfo skeleton_event_size_info,
                               const SkeletonEventBinding& skeleton_event_binding)
    -> std::optional<typename SkeletonEventBinding::SendTraceCallback>;

auto CreateTracingSendWithAllocateCallback(SkeletonEventTracingData& skeleton_event_tracing_data,
                                           memory::DataTypeSizeInfo skeleton_event_size_info,
                                           const SkeletonEventBinding& skeleton_event_binding)
    -> std::optional<typename SkeletonEventBinding::SendTraceCallback>;

void TraceSendWithAllocate(SkeletonEventTracingData& skeleton_event_tracing_data,
                           memory::DataTypeSizeInfo skeleton_event_size_info,
                           const SkeletonEventBinding& skeleton_event_binding_base,
                           impl::SampleAllocateePtr<void>& sample_data_ptr);

void TraceSend(SkeletonEventTracingData& skeleton_event_tracing_data,
               memory::DataTypeSizeInfo skeleton_event_size_info,
               const SkeletonEventBinding& skeleton_event_binding_base,
               impl::SampleAllocateePtr<void>& sample_data_ptr);

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_TRACING_H
