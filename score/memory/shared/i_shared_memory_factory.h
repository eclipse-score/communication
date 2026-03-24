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
#ifndef SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_FACTORY_H
#define SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_FACTORY_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"
#include "score/os/utils/acl/access_control_list.h"

#include <score/span.hpp>

#include <memory>
#include <string>

namespace score::memory::shared
{

class ISharedMemoryFactory
{
  public:
    using InitializeCallback = ISharedMemoryResource::InitializeCallback;
    using UserPermissionsMap = ISharedMemoryResource::UserPermissionsMap;
    using UserPermissions = ISharedMemoryResource::UserPermissions;
    using AccessControl = ISharedMemoryResource::AccessControl;

    virtual std::shared_ptr<ISharedMemoryResource> Open(const std::string&,
                                                        const bool,
                                                        const std::optional<score::cpp::span<const uid_t>>&) noexcept = 0;

    virtual std::shared_ptr<ISharedMemoryResource> Create(std::string,
                                                          InitializeCallback,
                                                          const std::size_t,
                                                          const UserPermissions&,
                                                          const bool) noexcept = 0;

    virtual std::shared_ptr<ISharedMemoryResource> CreateAnonymous(std::uint64_t,
                                                                   InitializeCallback,
                                                                   const std::size_t,
                                                                   const UserPermissions&,
                                                                   const bool) noexcept = 0;

    virtual std::shared_ptr<ISharedMemoryResource> CreateOrOpen(std::string,
                                                                InitializeCallback,
                                                                const std::size_t,
                                                                const ISharedMemoryFactory::AccessControl,
                                                                const bool) noexcept = 0;

    virtual void Remove(const std::string&) noexcept = 0;

    virtual void RemoveStaleArtefacts(const std::string& path) noexcept = 0;

    virtual void SetTypedMemoryProvider(std::shared_ptr<score::memory::shared::TypedMemory>) noexcept = 0;

    virtual std::size_t GetControlBlockSize() noexcept = 0;

    virtual void Clear() noexcept = 0;

    ISharedMemoryFactory() noexcept = default;
    virtual ~ISharedMemoryFactory() = default;

  protected:
    ISharedMemoryFactory(const ISharedMemoryFactory&) noexcept = default;
    ISharedMemoryFactory(ISharedMemoryFactory&&) noexcept = default;
    ISharedMemoryFactory& operator=(const ISharedMemoryFactory&) noexcept = default;
    ISharedMemoryFactory& operator=(ISharedMemoryFactory&&) noexcept = default;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_FACTORY_H
