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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"
#include "score/mw/com/impl/runtime.h"

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/os/unistd.h"

#include <cstddef>

namespace score::mw::com::impl::lola
{

class ServiceDataStorage
{
  public:
    /// \brief associative container mapping a service-element (event/field) to the raw storage of its event-data slots.
    using EventDataStorageMap = LinearSearchMap<ElementFqId, score::memory::shared::OffsetPtr<void>>;
    /// \brief associative container mapping a service-element (event/field) to its (type-erased) meta-information.
    using EventMetaInfoMap = LinearSearchMap<ElementFqId, EventMetaInfo>;

    /// \brief Ctor for the ServiceDataStorage to place it in the given shared memory resource.
    /// \details ServiceDataStorage no longer uses dynamically allocating map-types. Instead it uses fixed-capacity
    ///          containers (LinearSearchMap) whose capacity has to be provided at construction time. The capacity
    ///          equals the number of service-elements (events + fields) of the service-instance, which is known
    ///          up-front. This makes the memory footprint of ServiceDataStorage deterministic and calculable without a
    ///          simulation run.
    /// \param number_of_service_elements maximum number of service-elements (events + fields) that will be stored.
    /// \param resource memory-resource to be used for the (single, up-front) allocation of the containers.
    ServiceDataStorage(const std::size_t number_of_service_elements,
                       score::memory::shared::ManagedMemoryResource& resource)
        : events_(number_of_service_elements, resource),
          events_metainfo_(number_of_service_elements, resource),
          skeleton_pid_{impl::GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetPid()},
          skeleton_uid_{os::Unistd::instance().getuid()}
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventDataStorageMap events_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventMetaInfoMap events_metainfo_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t skeleton_pid_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    uid_t skeleton_uid_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
