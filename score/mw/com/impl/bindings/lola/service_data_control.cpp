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

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>
#include <vector>

namespace score::mw::com::impl::lola
{

std::size_t CalculateServiceDataControlShmSize(
    const score::cpp::span<const ServiceElementControlSizeInfo> events_and_fields_size_info)
{
    // The number of events + fields determines the (fixed) capacity of the event_controls_ LinearSearchMap within the
    // ServiceDataControl. It equals the number of sizing entries handed over.
    const auto number_of_events_and_fields = events_and_fields_size_info.size();

    // The real construction of a ServiceDataControl and its contained EventControls performs a fixed, deterministic
    // sequence of allocations from the (strictly monotonic) shared-memory resource, which itself always starts
    // allocating at a std::max_align_t aligned location. Since we know the exact size/alignment of every single one
    // of these allocations and the exact order in which they happen, we can reconstruct the exact sequence here and
    // let memory::shared::CalculateAlignedSizeOfSequence() compute the exact (not just worst-case) total size, taking
    // into account the exact alignment-padding between consecutive allocations.
    std::vector<score::memory::DataTypeSizeInfo> allocation_sequence{};

    // (1) The ServiceDataControl object itself (including the inline bookkeeping of its event_controls_ LinearSearchMap
    // and of its application_id_pid_mapping_).
    allocation_sequence.emplace_back(sizeof(ServiceDataControl), alignof(ServiceDataControl));

    // (2) The allocated array of the event_controls_ LinearSearchMap (allocated once, with capacity ==
    // number_of_events_and_fields).
    allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(decltype(ServiceDataControl::event_controls_)::value_type),
        alignof(decltype(ServiceDataControl::event_controls_)::value_type));

    // (3) The backing array of the application_id_pid_mapping_ (a fixed-capacity DynamicArray with a capacity of
    // kMaxApplicationIdPidMappings). It is allocated once during ServiceDataControl construction, independent of the
    // number of events + fields.
    allocation_sequence.emplace_back(static_cast<std::size_t>(ServiceDataControl::kMaxApplicationIdPidMappings) *
                                         sizeof(ApplicationIdPidMappingEntry),
                                     alignof(ApplicationIdPidMappingEntry));

    // (4) For each event/field: the (deeply) nested fixed-capacity DynamicArrays contained within its EventControl.
    // The EventControl object itself is stored inline within the event_controls_ backing array (accounted for in (2));
    // only the allocated arrays of its nested DynamicArrays allocate separately and are accounted for here.
    for (const auto& service_element : events_and_fields_size_info)
    {
        const std::size_t number_of_slots = service_element.number_of_slots;
        const std::size_t max_subscribers = service_element.max_subscribers;

        // (4a) EventControl::data_control (EventDataControl): its state_slots_ is a DynamicArray<ControlSlotType>
        // with a capacity of number_of_slots.
        allocation_sequence.emplace_back(number_of_slots * sizeof(EventDataControl::EventControlSlots::value_type),
                                         alignof(EventDataControl::EventControlSlots::value_type));

        // (4b) EventControl::transaction_log_set_ (TransactionLogSet):
        //   proxy_transaction_logs_(max_subscribers, TransactionLogNode{number_of_slots, resource}, resource) is
        //   constructed via the DynamicArray fill-constructor. The exact allocation order is:
        //     * one reference_count_slots_ array for the temporary prototype TransactionLogNode, which is
        //       constructed (as a function argument) before the DynamicArray constructor itself even runs. This
        //       constucts the TransactionLog for the given TransactionLogNode, which contains a DynamicArray of
        //       TransactionLogSlots representing the TransactionLog for slot-referencing.
        const std::size_t transaction_log_slots_array_raw_size =
            number_of_slots * sizeof(TransactionLog::TransactionLogSlots::value_type);
        const std::size_t transaction_log_slots_array_alignment =
            alignof(TransactionLog::TransactionLogSlots::value_type);
        allocation_sequence.emplace_back(transaction_log_slots_array_raw_size, transaction_log_slots_array_alignment);

        //     * then the backing array of proxy_transaction_logs_ itself (max_subscribers elements of
        //       TransactionLogNode), allocated in a single allocation.
        allocation_sequence.emplace_back(max_subscribers * sizeof(TransactionLogSet::TransactionLogNode),
                                         alignof(TransactionLogSet::TransactionLogNode));

        //     * then, one at a time (in order), each of the max_subscribers elements of proxy_transaction_logs_ gets
        //       copy-constructed from the prototype, each copy-construction allocating its own, separate
        //       reference_count_slots_ array.
        for (std::size_t i = 0U; i < max_subscribers; ++i)
        {
            allocation_sequence.emplace_back(transaction_log_slots_array_raw_size,
                                             transaction_log_slots_array_alignment);
        }

        //   - finally, the inline skeleton_tracing_transaction_log_ member is constructed, allocating one more
        //     reference_count_slots_ array.
        allocation_sequence.emplace_back(transaction_log_slots_array_raw_size, transaction_log_slots_array_alignment);
    }

    return score::memory::shared::CalculateAlignedSizeOfSequence(allocation_sequence);
}

}  // namespace score::mw::com::impl::lola
