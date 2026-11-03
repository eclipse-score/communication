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
#include "score/mw/com/impl/bindings/lola/rollback_synchronization.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{
RollbackSynchronization::Entry& RollbackSynchronization::GetEntry(
    const SkeletonInstanceIdentifier skeleton_instance_identifier)
{
    const std::lock_guard<std::mutex> lock{map_mutex_};
    auto search = synchronisation_data_map_.find(skeleton_instance_identifier);
    if (search != synchronisation_data_map_.end())
    {
        return search->second;
    }
    auto [iterator, inserted] = synchronisation_data_map_.emplace(
        std::piecewise_construct, std::forward_as_tuple(skeleton_instance_identifier), std::forward_as_tuple());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted, "Error inserting entry into synchronisation_data_map_");
    return iterator->second;
}

std::ostream& operator<<(std::ostream& ostream_out, const RollbackSynchronization&) noexcept
{
    ostream_out << "RollbackSynchronization";
    return ostream_out;
}

}  // namespace score::mw::com::impl::lola
