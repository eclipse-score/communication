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

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"

#include "score/memory/shared/map.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"

#include "score/os/unistd.h"

namespace score::mw::com::impl::lola
{

class ServiceDataStorage
{
  public:
    explicit ServiceDataStorage(const score::memory::shared::MemoryResourceProxy* const proxy)
        : events_(proxy), events_metainfo_(proxy), skeleton_pid_{score::os::Unistd::instance().getpid()}
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, score::memory::shared::OffsetPtr<void>> events_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, EventMetaInfo> events_metainfo_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t skeleton_pid_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
