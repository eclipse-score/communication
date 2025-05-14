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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_IMPL_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_IMPL_H

#include "score/memory/shared/i_shared_memory_factory.h"
#include "score/memory/shared/shared_memory_resource.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include <score/span.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace score::memory::shared
{

class SharedMemoryFactoryImpl final : public ISharedMemoryFactory
{
  public:
    std::shared_ptr<ISharedMemoryResource> Open(
        const std::string& path,
        const bool is_read_write,
        const std::optional<score::cpp::span<const uid_t>>& allowedProviders) noexcept override;

    std::shared_ptr<ISharedMemoryResource> Create(std::string path,
                                                  InitializeCallback cb,
                                                  const std::size_t user_space_to_reserve,
                                                  const UserPermissions& permissions,
                                                  const bool prefer_typed_memory) noexcept override;

    std::shared_ptr<ISharedMemoryResource> CreateAnonymous(std::uint64_t shared_memory_resource_id,
                                                           InitializeCallback cb,
                                                           const std::size_t user_space_to_reserve,
                                                           const UserPermissions& permissions,
                                                           const bool prefer_typed_memory) noexcept override;

    std::shared_ptr<ISharedMemoryResource> CreateOrOpen(std::string path,
                                                        InitializeCallback cb,
                                                        const std::size_t user_space_to_reserve,
                                                        const AccessControl access_control,
                                                        const bool prefer_typed_memory) noexcept override;

    void Remove(const std::string& path) noexcept override;

    void RemoveStaleArtefacts(const std::string& path) noexcept override;

    void SetTypedMemoryProvider(std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept override;

    std::size_t GetControlBlockSize() noexcept override
    {
        return sizeof(score::memory::shared::SharedMemoryResource::ControlBlock);
    };

    void Clear() noexcept override
    {
        resources_.clear();
    }

  private:
    std::mutex mutex_{};
    std::unordered_map<std::string, std::weak_ptr<score::memory::shared::SharedMemoryResource>> resources_{};
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr_{memory::shared::TypedMemory::Default()};
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_IMPL_H
