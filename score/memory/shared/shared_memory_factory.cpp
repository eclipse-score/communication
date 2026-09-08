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
#include "score/memory/shared/i_shared_memory_factory.h"
#include "score/memory/shared/shared_memory_factory_impl.h"

#include "score/utils/static_destruction_guard.h"

#include <score/span.hpp>

#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace score::memory::shared
{

ISharedMemoryFactory* SharedMemoryFactory::mock_{nullptr};

auto score::memory::shared::SharedMemoryFactory::Open(
    const std::string& path,
    const bool is_read_write,
    const std::optional<score::cpp::span<const uid_t>>& allowedProviders) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
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
    return instance().Create(std::move(path), std::move(cb), user_space_to_reserve, permissions, prefer_typed_memory);
}

auto score::memory::shared::SharedMemoryFactory::CreateAnonymous(std::uint64_t shared_memory_resource_id,
                                                                 InitializeCallback cb,
                                                                 const std::size_t user_space_to_reserve,
                                                                 const UserPermissions& permissions,
                                                                 const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().CreateAnonymous(
        shared_memory_resource_id, std::move(cb), user_space_to_reserve, permissions, prefer_typed_memory);
}

auto SharedMemoryFactory::CreateOrOpen(std::string path,
                                       InitializeCallback cb,
                                       const std::size_t user_space_to_reserve,
                                       const SharedMemoryResource::AccessControl access_control,
                                       const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    return instance().CreateOrOpen(
        std::move(path), std::move(cb), user_space_to_reserve, access_control, prefer_typed_memory);
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
    instance().SetTypedMemoryProvider(std::move(typed_memory_ptr));
}

auto SharedMemoryFactory::SetInterVMMemoryProvider(std::shared_ptr<TypedMemory> intervm_memory_ptr) noexcept -> void
{
    instance().SetInterVMMemoryProvider(std::move(intervm_memory_ptr));
}

auto SharedMemoryFactory::instance() noexcept -> ISharedMemoryFactory&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // SharedMemoryFactoryImpl is kept alive via the nifty-counter idiom (see
    // detail::nifty_counter_shared_memory_factory_impl) rather than a plain
    // function-local static (Meyer's singleton), for the same static destruction order reasons as
    // MemoryResourceRegistry.
    return ::score::utils::StaticDestructionGuard<SharedMemoryFactoryImpl>::GetStorage();
}

auto SharedMemoryFactory::InjectMock(ISharedMemoryFactory* mock) noexcept -> void
{
    mock_ = mock;
}

}  // namespace score::memory::shared
