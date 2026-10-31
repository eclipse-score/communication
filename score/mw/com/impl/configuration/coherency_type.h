/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_COHERENCY_TYPE_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_COHERENCY_TYPE_H

#include <cstdint>
#include <ostream>

namespace score::mw::com::impl
{

/// \brief Describes which CONTROL segment data structure / synchronization strategy is used for an event or field.
///
/// \details
/// - kNonCoherent: the classic LoLa behaviour with a slot-search and atomic EventSlotStatus per slot. Producer and
///   consumers may run concurrently and need to synchronize via the CONTROL segment.
/// - kCoherent: producer and consumers are guaranteed (by the caller, e.g. via scheduling constraints) to never run
///   concurrently. No atomics/slot-search are needed; the CONTROL segment only tracks a timestamp per slot plus the
///   index of the newest slot.
/// - kMixed: provider side only. Combines the kNonCoherent CONTROL segment layout (needed to serve kNonCoherent
///   consumers) with the "newest slot index" optimization of kCoherent (to speed up kCoherent consumers).
enum class CoherencyType : std::uint8_t
{
    kNonCoherent,
    kCoherent,
    kMixed,
};

std::ostream& operator<<(std::ostream& ostream_out, const CoherencyType& coherency_type);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_COHERENCY_TYPE_H
