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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_RESOURCE_MAP_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_RESOURCE_MAP_H

#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/configuration/global_configuration.h"

#include "score/memory/shared/i_shared_memory_resource.h"

#include <sched.h>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief Map which stores method shared memory regions which removes old regions (i.e. regions that were created by a
///        Proxy which crashed and has restarted) on insertion.
///
/// A detailed explanation of the how we handle partial restart in the context of methods and why this map is needed is
/// in `platform/aas/docs/features/ipc/lola/method/README.md` (Specifically in the section about "Cleaning up old
/// method shared memory resources").
class MethodResourceMap
{
    struct ProcessSpecificResourceMap
    {
        pid_t pid;
        std::unordered_map<ProxyInstanceIdentifier::ProxyInstanceCounter,
                           std::shared_ptr<memory::shared::ISharedMemoryResource>>
            inner_resource_map;
    };
    using ResourceMap = std::unordered_map<GlobalConfiguration::ApplicationId, ProcessSpecificResourceMap>;

  public:
    /// \brief Iterator of resource map pointing to the ISharedMemoryResource (whose key is ProxyInstanceCounter)
    using iterator = decltype(ProcessSpecificResourceMap::inner_resource_map)::iterator;

    enum class CleanUpResult
    {
        OLD_REGIONS_REMOVED,
        NO_REGIONS_REMOVED
    };

    /// \brief Checks whether an ISharedMemoryResource is stored within the map corresponding to the provided
    ///        ProxyInstanceIdentifier AND pid.
    bool Contains(const ProxyInstanceIdentifier proxy_instance_identifier, const pid_t proxy_pid) const;

    /// \brief Inserts a new ISharedMemoryResource and cleans up any resources corresponding to the provided
    ///        ApplicationId but different pid.
    ///
    /// This function inserts a newly created region while cleaning up any resources corresponding to the same Skeleton
    /// instance which previously crashed and has restarted (i.e. it has the ApplicationId as the old Skeleton instance
    /// but different pid since the process restarted).
    auto InsertAndCleanUpOldRegions(const ProxyInstanceIdentifier proxy_instance_identifier,
                                    const pid_t proxy_pid,
                                    const std::shared_ptr<memory::shared::ISharedMemoryResource>& methods_shm_resource)
        -> std::pair<iterator, CleanUpResult>;

  private:
    auto EraseRegionsFromCrashedProcesses(const GlobalConfiguration::ApplicationId proxy_app_id, const pid_t proxy_pid)
        -> CleanUpResult;

    ResourceMap resource_map_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_RESOURCE_MAP_H
