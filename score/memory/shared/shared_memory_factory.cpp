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
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_factory_impl.h"
#include "score/os/errno_logging.h"
#include "score/mw/log/logging.h"

#include "score/os/unistd.h"

#include <exception>
#include <utility>

namespace score::memory::shared
{

ISharedMemoryFactory* SharedMemoryFactory::mock_{nullptr};

auto score::memory::shared::SharedMemoryFactory::Open(
    const std::string& path,
    const bool is_read_write,
    const std::optional<score::cpp::span<const uid_t>>& allowedProviders) noexcept -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().Open(path, is_read_write, allowedProviders);
}

auto score::memory::shared::SharedMemoryFactory::Create(std::string path,
                                                      InitializeCallback cb,
                                                      const std::size_t user_space_to_reserve,
                                                      const UserPermissions& permissions,
                                                      const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().Create(path, cb, user_space_to_reserve, permissions, prefer_typed_memory);
}

auto score::memory::shared::SharedMemoryFactory::CreateAnonymous(std::uint64_t shared_memory_resource_id,
                                                               InitializeCallback cb,
                                                               const std::size_t user_space_to_reserve,
                                                               const UserPermissions& permissions,
                                                               const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().CreateAnonymous(
        shared_memory_resource_id, cb, user_space_to_reserve, permissions, prefer_typed_memory);
}

auto SharedMemoryFactory::CreateOrOpen(std::string path,
                                       InitializeCallback cb,
                                       const std::size_t user_space_to_reserve,
                                       const SharedMemoryResource::AccessControl access_control,
                                       const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().CreateOrOpen(path, cb, user_space_to_reserve, access_control, prefer_typed_memory);
}

auto SharedMemoryFactory::Remove(const std::string& path) noexcept -> void
{
    instance().Remove(path);
}

auto SharedMemoryFactory::RemoveStaleArtefacts(const std::string& path) noexcept -> void
{
    instance().RemoveStaleArtefacts(path);
}

auto SharedMemoryFactory::SetTypedMemoryProvider(std::shared_ptr<TypedMemory> typed_memory_ptr) noexcept -> void
{
    instance().SetTypedMemoryProvider(typed_memory_ptr);
}

auto SharedMemoryFactory::instance() noexcept -> ISharedMemoryFactory&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // Rationale: SharedMemoryFactoryImpl does not have a constexpr constructor.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static SharedMemoryFactoryImpl instance{};
    return instance;
}

auto SharedMemoryFactory::InjectMock(ISharedMemoryFactory* mock) noexcept -> void
{
    mock_ = mock;
}

}  // namespace score::memory::shared
