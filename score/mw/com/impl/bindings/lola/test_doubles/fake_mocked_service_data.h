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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_DOUBLES_FAKE_MOCKED_SKELETON_DATA_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_DOUBLES_FAKE_MOCKED_SKELETON_DATA_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"

#include <memory>
#include <string>
#include <tuple>

namespace score::mw::com::impl::lola
{

/// \brief Allows to generate fake event data inside a shared memory region, akin to what a Lola skeleton would do.
struct FakeMockedServiceData
{
    /// \brief Create shared memory regions that will resemble data created by a Lola skeleton.
    FakeMockedServiceData(pid_t skeleton_process_pid_in);

    ServiceDataControl* data_control{nullptr};
    ServiceDataStorage* data_storage{nullptr};
    std::shared_ptr<memory::shared::SharedMemoryResourceHeapAllocatorMock> control_memory{nullptr};
    std::shared_ptr<memory::shared::SharedMemoryResourceHeapAllocatorMock> data_memory{nullptr};

    /// Add a new event to the event structures inside the shared memory regions.
    ///
    /// \tparam SampleType Data to be transmitted.
    /// \param id Event ID as used inside the Lola event structures.
    /// \param max_num_slots Number of data slots.
    /// \param max_subscribers maximum number of subscribers
    /// \return A tuple that points to the newly initialized event-specific data structures.
    template <typename SampleType>
    std::tuple<EventControl*, EventDataStorage<SampleType>*> AddEvent(ElementFqId id,
                                                                      SkeletonEventProperties event_properties);
};

template <typename SampleType>
inline std::tuple<EventControl*, EventDataStorage<SampleType>*> FakeMockedServiceData::AddEvent(
    const ElementFqId id,
    const SkeletonEventProperties event_properties)
{
    bool inserted;

    score::memory::shared::Map<ElementFqId, EventControl>::iterator inserted_control;
    std::tie(inserted_control, inserted) =
        data_control->event_controls_.emplace(std::piecewise_construct,
                                              std::forward_as_tuple(id),
                                              std::forward_as_tuple(event_properties.number_of_slots,
                                                                    event_properties.max_subscribers,
                                                                    event_properties.enforce_max_samples,
                                                                    control_memory->getMemoryResourceProxy()));
    auto& event_control = std::get<EventControl>(*inserted_control);

    EventDataStorage<SampleType>* event_data_slots = data_memory->construct<EventDataStorage<SampleType>>(
        event_properties.number_of_slots, data_memory->getMemoryResourceProxy());
    const memory::shared::OffsetPtr<void> rel_event_data_buffer{static_cast<void*>(event_data_slots)};
    data_storage->events_.emplace(id, rel_event_data_buffer);

    const DataTypeMetaInfo sample_meta_info{sizeof(SampleType), alignof(SampleType)};
    auto* event_data_raw_array = event_data_slots->data();
    const auto inserted_meta_info =
        data_storage->events_metainfo_.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(id),
                                               std::forward_as_tuple(sample_meta_info, event_data_raw_array));
    SCORE_LANGUAGE_FUTURECPP_ASSERT(inserted_meta_info.second);

    return std::make_tuple(&event_control, event_data_slots);
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_DOUBLES_FAKE_MOCKED_SKELETON_DATA_H
