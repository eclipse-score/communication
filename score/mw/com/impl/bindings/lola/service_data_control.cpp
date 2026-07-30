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
#include "score/mw/com/impl/bindings/lola/service_data_control.h"

#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping_entry.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>

namespace score::mw::com::impl::lola
{

namespace
{

// All alignments occurring within the shm-objects are <= alignof(std::max_align_t). The SharedMemoryResource is a
// monotonic (bump) allocator: the bytes it accounts for a single allocation are the requested size plus the padding
// needed to bring the current offset to the requested alignment. By rounding every individual allocation up to
// alignof(std::max_align_t) (via the shared CalculateAlignedSize() utility) we keep the running offset max-aligned and
// therefore obtain a size that is guaranteed to be sufficient (it is exact whenever the allocated sizes are multiples
// of the involved alignment, which is the common case).
constexpr std::size_t kMaxAlign = alignof(std::max_align_t);

}  // namespace

std::size_t CalculateServiceDataControlShmSize(
    const score::cpp::span<const ServiceElementControlSizeInfo> service_elements_size_info)
{
    // The number of service-elements determines the (fixed) capacity of the event_controls_ LinearSearchMap within the
    // ServiceDataControl. It equals the number of sizing entries handed over.
    const auto number_of_service_elements = service_elements_size_info.size();

    // (1) The ServiceDataControl object itself (including the inline bookkeeping of its event_controls_ LinearSearchMap
    // and of its application_id_pid_mapping_).
    std::size_t total_size = memory::shared::CalculateAlignedSize(sizeof(ServiceDataControl), kMaxAlign);

    // (2) The backing array of the event_controls_ LinearSearchMap (allocated once, with capacity ==
    // number_of_service_elements).
    total_size += memory::shared::CalculateAlignedSize(
        number_of_service_elements * sizeof(decltype(ServiceDataControl::event_controls_)::value_type), kMaxAlign);

    // (3) The backing array of the application_id_pid_mapping_ (a fixed-capacity DynamicArray with a capacity of
    // kMaxApplicationIdPidMappings). It is allocated once during ServiceDataControl construction, independent of the
    // number of service-elements.
    total_size += memory::shared::CalculateAlignedSize(
        static_cast<std::size_t>(ServiceDataControl::kMaxApplicationIdPidMappings) *
            sizeof(ApplicationIdPidMappingEntry),
        kMaxAlign);

    // (4) For each event/field: the (deeply) nested fixed-capacity DynamicArrays contained within its EventControl.
    // The EventControl object itself is stored inline within the event_controls_ backing array (accounted for in (2));
    // only the backing arrays of its nested DynamicArrays allocate separately and are accounted for here.
    for (const auto& service_element : service_elements_size_info)
    {
        const std::size_t number_of_slots = service_element.number_of_slots;
        const std::size_t max_subscribers = service_element.max_subscribers;

        // (4a) EventControl::data_control (EventDataControl): its state_slots_ is a DynamicArray<ControlSlotType>
        // with a capacity of number_of_slots.
        total_size += memory::shared::CalculateAlignedSize(
            number_of_slots * sizeof(EventDataControl::EventControlSlots::value_type), kMaxAlign);

        // (4b) EventControl::transaction_log_set_ (TransactionLogSet):
        //   - proxy_transaction_logs_: a DynamicArray<TransactionLogNode> with a capacity of max_subscribers.
        total_size += memory::shared::CalculateAlignedSize(
            max_subscribers * sizeof(TransactionLogSet::TransactionLogNode), kMaxAlign);

        //   - each TransactionLogNode holds a TransactionLog whose reference_count_slots_ is a
        //     DynamicArray<TransactionLogSlot> with a capacity of number_of_slots. The following separate
        //     allocations of this array are made during construction of the TransactionLogSet:
        //       * one per TransactionLogNode within proxy_transaction_logs_ (max_subscribers of them),
        //       * one for the temporary prototype TransactionLogNode used by the DynamicArray fill-constructor.
        //         Since the shared-memory resource is strictly monotonic (it never reclaims memory), this
        //         temporary allocation permanently occupies space and must be accounted for.
        //       * one for the inline skeleton_tracing_transaction_log_ member.
        const std::size_t transaction_log_slots_array_size = memory::shared::CalculateAlignedSize(
            number_of_slots * sizeof(TransactionLog::TransactionLogSlots::value_type), kMaxAlign);
        const std::size_t number_of_transaction_log_slot_arrays = max_subscribers + 2U;
        total_size += number_of_transaction_log_slot_arrays * transaction_log_slots_array_size;
    }

    return total_size;
}

}  // namespace score::mw::com::impl::lola
