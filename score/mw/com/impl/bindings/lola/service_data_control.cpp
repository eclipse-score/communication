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
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/transaction_log.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>
#include <tuple>
#include <vector>

namespace score::mw::com::impl::lola
{

std::size_t CalculateServiceDataControlShmSize(
    const score::cpp::span<const ServiceElementControlSizeInfo> events_and_fields_size_info,
    const score::cpp::span<const ServiceElementControlSizeInfo> coherent_events_and_fields_size_info,
    const score::cpp::span<const ServiceElementControlSizeInfo> mixed_events_and_fields_size_info)
{
    // The number of events + fields determines the (fixed) capacity of the event_controls_ LinearSearchMap within the
    // ServiceDataControl. It equals the number of sizing entries handed over.
    const auto number_of_events_and_fields = events_and_fields_size_info.size();
    const auto number_of_coherent_events_and_fields = coherent_events_and_fields_size_info.size();
    const auto number_of_mixed_events_and_fields = mixed_events_and_fields_size_info.size();

    // The real construction of a ServiceDataControl and its contained EventControls performs a fixed, deterministic
    // sequence of allocations from the (strictly monotonic) shared-memory resource, which itself always starts
    // allocating at a std::max_align_t aligned location. Since we know the exact size/alignment of every single one
    // of these allocations and the exact order in which they happen, we can reconstruct the exact sequence here and
    // let memory::shared::CalculateAlignedSizeOfSequence() compute the exact (not just worst-case) total size, taking
    // into account the exact alignment-padding between consecutive allocations.
    std::vector<score::memory::DataTypeSizeInfo> allocation_sequence{};

    // (1) The ServiceDataControl object itself (including the inline bookkeeping of its event_controls_,
    // event_controls_coherent_ and event_controls_mixed_ LinearSearchMaps and of its application_id_pid_mapping_).
    std::ignore = allocation_sequence.emplace_back(sizeof(ServiceDataControl), alignof(ServiceDataControl));

    // (2) The allocated array of the event_controls_ LinearSearchMap (allocated once, with capacity ==
    // number_of_events_and_fields).
    std::ignore = allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(decltype(ServiceDataControl::event_controls_)::value_type),
        alignof(decltype(ServiceDataControl::event_controls_)::value_type));

    // (3) The allocated array of the event_controls_coherent_ LinearSearchMap (allocated once, with capacity ==
    // number_of_coherent_events_and_fields), constructed right after event_controls_.
    std::ignore =
        allocation_sequence.emplace_back(number_of_coherent_events_and_fields *
                                             sizeof(decltype(ServiceDataControl::event_controls_coherent_)::value_type),
                                         alignof(decltype(ServiceDataControl::event_controls_coherent_)::value_type));

    // (4) The allocated array of the event_controls_mixed_ LinearSearchMap (allocated once, with capacity ==
    // number_of_mixed_events_and_fields), constructed right after event_controls_coherent_.
    std::ignore = allocation_sequence.emplace_back(
        number_of_mixed_events_and_fields * sizeof(decltype(ServiceDataControl::event_controls_mixed_)::value_type),
        alignof(decltype(ServiceDataControl::event_controls_mixed_)::value_type));

    // (5) The backing array of the application_id_pid_mapping_ (a fixed-capacity DynamicArray with a capacity of
    // kMaxApplicationIdPidMappings). It is allocated once during ServiceDataControl construction, independent of the
    // number of events + fields.
    std::ignore =
        allocation_sequence.emplace_back(static_cast<std::size_t>(ServiceDataControl::kMaxApplicationIdPidMappings) *
                                             sizeof(ApplicationIdPidMappingEntry),
                                         alignof(ApplicationIdPidMappingEntry));

    // (6) For each CoherencyType::kNonCoherent event/field: the (deeply) nested fixed-capacity DynamicArrays
    // contained within its EventControl. The EventControl object itself is stored inline within the event_controls_
    // backing array (accounted for in (2)); only the allocated arrays of its nested DynamicArrays allocate separately
    // and are accounted for here.
    for (const auto& service_element : events_and_fields_size_info)
    {
        const std::size_t number_of_slots = service_element.number_of_slots;
        const std::size_t max_subscribers = service_element.max_subscribers;

        // (6a) EventControl::data_control (EventDataControl): its state_slots_ is a DynamicArray<ControlSlotType>
        // with a capacity of number_of_slots.
        std::ignore =
            allocation_sequence.emplace_back(number_of_slots * sizeof(EventDataControl::EventControlSlots::value_type),
                                             alignof(EventDataControl::EventControlSlots::value_type));

        // (6b) EventControl::transaction_log_set_ (TransactionLogSet):
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
        std::ignore = allocation_sequence.emplace_back(transaction_log_slots_array_raw_size,
                                                       transaction_log_slots_array_alignment);

        //     * then the backing array of proxy_transaction_logs_ itself (max_subscribers elements of
        //       TransactionLogNode), allocated in a single allocation.
        std::ignore = allocation_sequence.emplace_back(max_subscribers * sizeof(TransactionLogSet::TransactionLogNode),
                                                       alignof(TransactionLogSet::TransactionLogNode));

        //     * then, one at a time (in order), each of the max_subscribers elements of proxy_transaction_logs_ gets
        //       copy-constructed from the prototype, each copy-construction allocating its own, separate
        //       reference_count_slots_ array.
        for (std::size_t i = 0U; i < max_subscribers; ++i)
        {
            std::ignore = allocation_sequence.emplace_back(transaction_log_slots_array_raw_size,
                                                           transaction_log_slots_array_alignment);
        }

        //   - finally, the inline skeleton_tracing_transaction_log_ member is constructed, allocating one more
        //     reference_count_slots_ array.
        std::ignore = allocation_sequence.emplace_back(transaction_log_slots_array_raw_size,
                                                       transaction_log_slots_array_alignment);
    }

    // (7) For each CoherencyType::kCoherent event/field: the nested fixed-capacity DynamicArrays contained within its
    // EventControlCoherent. The EventControlCoherent object itself is stored inline within the
    // event_controls_coherent_ backing array (accounted for in (3)); only the allocated arrays of its nested
    // DynamicArrays allocate separately and are accounted for here.
    for (const auto& service_element : coherent_events_and_fields_size_info)
    {
        const std::size_t number_of_slots = service_element.number_of_slots;
        const std::size_t max_subscribers = service_element.max_subscribers;

        // EventControlCoherent::data_control (EventDataControlCoherent): its state_slots_ is a
        // DynamicArray<CoherentControlSlotType> with a capacity of number_of_slots.
        std::ignore = allocation_sequence.emplace_back(
            number_of_slots * sizeof(EventDataControlCoherent::EventControlSlots::value_type),
            alignof(EventDataControlCoherent::EventControlSlots::value_type));

        // EventControlCoherent::transaction_log_set_ (TransactionLogSet): constructed the same way as described in
        // (6b) above, but with 0 slots (CoherencyType::kCoherent events need no per-slot transaction-log based
        // crash-recovery, see EventControlCoherent); only Subscribe/Unsubscribe transaction logging (sized by
        // max_subscribers) is required, hence max_subscribers is still relevant for sizing here.
        constexpr std::size_t coherent_transaction_log_slots_array_raw_size = 0U;
        const std::size_t coherent_transaction_log_slots_array_alignment =
            alignof(TransactionLog::TransactionLogSlots::value_type);
        std::ignore = allocation_sequence.emplace_back(coherent_transaction_log_slots_array_raw_size,
                                                       coherent_transaction_log_slots_array_alignment);

        std::ignore = allocation_sequence.emplace_back(max_subscribers * sizeof(TransactionLogSet::TransactionLogNode),
                                                       alignof(TransactionLogSet::TransactionLogNode));

        for (std::size_t i = 0U; i < max_subscribers; ++i)
        {
            std::ignore = allocation_sequence.emplace_back(coherent_transaction_log_slots_array_raw_size,
                                                           coherent_transaction_log_slots_array_alignment);
        }

        std::ignore = allocation_sequence.emplace_back(coherent_transaction_log_slots_array_raw_size,
                                                       coherent_transaction_log_slots_array_alignment);
    }

    // (8) For each CoherencyType::kMixed event/field: the nested fixed-capacity DynamicArrays contained within its
    // EventControlMixed. The EventControlMixed object itself is stored inline within the event_controls_mixed_
    // backing array (accounted for in (4)); only the allocated arrays of its nested DynamicArrays allocate separately
    // and are accounted for here.
    for (const auto& service_element : mixed_events_and_fields_size_info)
    {
        const std::size_t number_of_slots = service_element.number_of_slots;
        const std::size_t max_subscribers = service_element.max_subscribers;

        // EventControlMixed::data_control (EventDataControlMixed): its embedded data_control_ (EventDataControl)'s
        // state_slots_ is a DynamicArray<ControlSlotType> with a capacity of number_of_slots (newest_index_ is a
        // plain member of EventDataControlMixed and does not allocate separately).
        std::ignore =
            allocation_sequence.emplace_back(number_of_slots * sizeof(EventDataControl::EventControlSlots::value_type),
                                             alignof(EventDataControl::EventControlSlots::value_type));

        // EventControlMixed::transaction_log_set_ (TransactionLogSet): constructed the same way as described in (6b)
        // above, with the full number_of_slots (CoherencyType::kMixed events' non-coherent consumers alter per-slot
        // state exactly as for CoherencyType::kNonCoherent, and therefore require the same per-slot transaction-log
        // based crash-recovery).
        const std::size_t mixed_transaction_log_slots_array_raw_size =
            number_of_slots * sizeof(TransactionLog::TransactionLogSlots::value_type);
        const std::size_t mixed_transaction_log_slots_array_alignment =
            alignof(TransactionLog::TransactionLogSlots::value_type);
        std::ignore = allocation_sequence.emplace_back(mixed_transaction_log_slots_array_raw_size,
                                                       mixed_transaction_log_slots_array_alignment);

        std::ignore = allocation_sequence.emplace_back(max_subscribers * sizeof(TransactionLogSet::TransactionLogNode),
                                                       alignof(TransactionLogSet::TransactionLogNode));

        for (std::size_t i = 0U; i < max_subscribers; ++i)
        {
            std::ignore = allocation_sequence.emplace_back(mixed_transaction_log_slots_array_raw_size,
                                                           mixed_transaction_log_slots_array_alignment);
        }

        std::ignore = allocation_sequence.emplace_back(mixed_transaction_log_slots_array_raw_size,
                                                       mixed_transaction_log_slots_array_alignment);
    }

    return score::memory::shared::CalculateAlignedSizeOfSequence(allocation_sequence);
}

}  // namespace score::mw::com::impl::lola
