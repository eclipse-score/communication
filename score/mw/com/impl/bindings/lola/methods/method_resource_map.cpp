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
#include "score/mw/com/impl/bindings/lola/methods/method_resource_map.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

bool MethodResourceMap::Contains(const ProxyInstanceIdentifier proxy_instance_identifier, const pid_t proxy_pid) const
{
    const auto resources_it = resource_map_.find(proxy_instance_identifier.process_identifier);
    if (resources_it == resource_map_.cend())
    {
        return false;
    }

    const auto& [pid, resources] = resources_it->second;
    if (pid != proxy_pid)
    {
        return false;
    }

    return resources.count(proxy_instance_identifier.proxy_instance_counter) != 0U;
}

auto MethodResourceMap::InsertAndCleanUpOldRegions(
    const ProxyInstanceIdentifier proxy_instance_identifier,
    const pid_t proxy_pid,
    const std::shared_ptr<memory::shared::ISharedMemoryResource>& methods_shm_resource)
    -> std::pair<iterator, CleanUpResult>
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(!Contains(proxy_instance_identifier, proxy_pid),
                                 "Contains() should be checked before trying to insert element: The same memory region "
                                 "should not be created / inserted twice.");
    const auto cleanup_result =
        EraseRegionsFromCrashedProcesses(proxy_instance_identifier.process_identifier, proxy_pid);

    // Create a new inner map if one doesn't exist. Otherwise, return the existing one.
    auto inner_pair_it = resource_map_.insert({proxy_instance_identifier.process_identifier, {}});

    auto& process_specific_resource_map = inner_pair_it.first->second;
    process_specific_resource_map.pid = proxy_pid;
    const auto [inserted_resource_it, _] = process_specific_resource_map.inner_resource_map.insert(
        {proxy_instance_identifier.proxy_instance_counter, methods_shm_resource});
    return {inserted_resource_it, cleanup_result};
}

auto MethodResourceMap::EraseRegionsFromCrashedProcesses(const GlobalConfiguration::ApplicationId proxy_app_id,
                                                         const pid_t proxy_pid) -> CleanUpResult
{
    // If no entry currently exists for the provided application ID, then there were no entries from a crashed process
    // for this service so we can return without cleaning anything up.
    auto resources_it = resource_map_.find(proxy_app_id);
    if (resources_it == resource_map_.cend())
    {
        return CleanUpResult::NO_REGIONS_REMOVED;
    }

    // If the found memory region has the same PID as the process which is trying to insert a new region, then any
    // existing regions are from the currently running process and therefore should not be cleaned up!
    const auto& [old_pid, resources] = resources_it->second;
    if (old_pid != proxy_pid)
    {
        score::mw::log::LogDebug("lola") << "Removing old methods shared memory regions with ApplicationID:"
                                       << proxy_app_id << "and PID:" << old_pid;
        resource_map_.erase(resources_it);
        return CleanUpResult::OLD_REGIONS_REMOVED;
    }
    return CleanUpResult::NO_REGIONS_REMOVED;
}

auto MethodResourceMap::Clear() -> void
{
    resource_map_.clear();
}

}  // namespace score::mw::com::impl::lola
