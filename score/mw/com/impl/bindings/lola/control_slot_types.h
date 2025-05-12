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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDEX_TYPE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDEX_TYPE_H

#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <atomic>
#include <cstdint>

namespace score::mw::com::impl::lola
{

/// \brief type of slot index for our CONTROL and DATA slots
using SlotIndexType = std::uint16_t;

/// \brief type of control slot
using ControlSlotType = std::atomic<EventSlotStatus::value_type>;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDEX_TYPE_H
