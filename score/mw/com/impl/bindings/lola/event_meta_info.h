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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_META_INFO_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_META_INFO_H

#include "score/memory/shared/offset_ptr.h"
#include "score/mw/com/impl/bindings/lola/data_type_meta_info.h"

namespace score::mw::com::impl::lola
{

/// \brief meta-information about an event/its type.
/// \details Normally proxies/skeletons or "user code" dealing with an event, know its properties. This info is
///          provided and placed into shared-memory for the GenericProxy use-case, where a proxy connects to a provided
///          service based on only deployment info, NOT having any knowledge about the exact data type of the event.
class EventMetaInfo
{
  public:
    EventMetaInfo(const DataTypeMetaInfo data_type_info, const memory::shared::OffsetPtr<void> event_slots_raw_array)
        : data_type_info_(data_type_info), event_slots_raw_array_(event_slots_raw_array)
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    DataTypeMetaInfo data_type_info_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    memory::shared::OffsetPtr<void> event_slots_raw_array_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_META_INFO_H
