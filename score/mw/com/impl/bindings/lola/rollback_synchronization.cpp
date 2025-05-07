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
std::pair<std::mutex&, bool> RollbackSynchronization::GetMutex(const ServiceDataControl* proxy_element_control)
{
    const std::lock_guard<std::mutex> lock{map_mutex_};
    auto search = synchronisation_data_map_.find(proxy_element_control);
    if (search != synchronisation_data_map_.end())
    {
        return {search->second, true};
    }
    auto [iterator, inserted] = synchronisation_data_map_.emplace(
        std::piecewise_construct, std::forward_as_tuple(proxy_element_control), std::forward_as_tuple());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted, "Error inserting mutex into synchronisation_data_map_");
    return {iterator->second, false};
}

std::ostream& operator<<(std::ostream& ostream_out, const RollbackSynchronization&) noexcept
{
    ostream_out << "RollbackSynchronization";
    return ostream_out;
}

}  // namespace score::mw::com::impl::lola
