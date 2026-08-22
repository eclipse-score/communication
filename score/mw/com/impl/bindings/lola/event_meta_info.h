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

#include "score/memory/data_type_size_info.h"

namespace score::mw::com::impl::lola
{

/// \brief meta-information about an event/its type.
/// \details Normally proxies/skeletons or "user code" dealing with an event, know its properties. This info is
///          provided and placed into shared-memory for the GenericProxy use-case, where a proxy connects to a provided
///          service based on only deployment info, NOT having any knowledge about the exact data type of the event.
///          Currently, the only "meta-info" needed is DataTypeSizeInfo. However, we wrap it into EventMetaInfo to be
///          prepared for future extensions.
class EventMetaInfo
{
  public:
    EventMetaInfo(const memory::DataTypeSizeInfo data_type_info) : data_type_info_(data_type_info) {}

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    memory::DataTypeSizeInfo data_type_info_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_META_INFO_H
