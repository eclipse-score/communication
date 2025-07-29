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
#include "score/memory/shared/shared_memory_factory_impl.h"
#include "score/os/errno_logging.h"
#include "score/mw/log/logging.h"

#include "score/os/mman.h"
#include "score/os/unistd.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <memory>
#include <utility>

namespace score::memory::shared
{

namespace
{

std::unique_ptr<score::os::IAccessControlList> CreateAccessControlList(
    ISharedMemoryResource::FileDescriptor file_descriptor)
{
    return std::make_unique<score::os::AccessControlList>(file_descriptor);
}

bool checkUidMatch(const uid_t providerUid, const score::cpp::v1::span<const uid_t> allowedProviders) noexcept
{
    if (providerUid == score::os::Unistd::instance().getuid())
    {
        return true;
    }
    for (const auto uid : allowedProviders)
    {
        if (providerUid == uid)
        {
            return true;
        }
    }
    return false;
}

void InsertResourceIntoMap(
    const std::string& path,
    const std::shared_ptr<score::memory::shared::SharedMemoryResource>& resource,
    std::unordered_map<std::string, std::weak_ptr<score::memory::shared::SharedMemoryResource>>& resource_map)
{
    const auto emplace_result = resource_map.emplace(path, resource);

    // This emplace should only be done the first time a memory resource is either opened or created in the process.
    // After that, it should be retrieved from by GetResourceIfAlreadyOpened().
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second, "Could not insert memory resource into map as it already exists.");
}

auto GetResourceIfAlreadyOpened(
    const std::string& path,
    std::unordered_map<std::string, std::weak_ptr<score::memory::shared::SharedMemoryResource>>& resource_map) noexcept
    -> std::shared_ptr<SharedMemoryResource>
{
    const auto iterator = resource_map.find(path);
    if (iterator == resource_map.cend())
    {
        return nullptr;
    }
    const std::shared_ptr<SharedMemoryResource> resource = iterator->second.lock();
    if (resource == nullptr)
    {
        score::cpp::ignore = resource_map.erase(iterator);
    }
    return resource;
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: score::cpp::expected value() can throw an exception if it's called without a value. Since we check has_value()
// before calling value(), an exception will never be called and therefore there will never be an implicit
// std::terminate call.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SharedMemoryFactoryImpl::Open(const std::string& path,
                                   const bool is_read_write,
                                   const std::optional<score::cpp::v1::span<const uid_t>>& allowedProviders) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    std::lock_guard<std::mutex> lock{mutex_};
    std::shared_ptr<SharedMemoryResource> resource = GetResourceIfAlreadyOpened(path, resources_);
    if (resource == nullptr)
    {
        const auto result = SharedMemoryResource::Open(path, is_read_write, &CreateAccessControlList, nullptr);
        if (!result.has_value())
        {
            score::mw::log::LogWarn("shm") << "Could not open Shared Memory " << path << ":" << result.error();
            return nullptr;
        }
        resource = result.value();
        InsertResourceIntoMap(path, resource, resources_);
    }

    const bool need_access_check = allowedProviders.has_value();
    if (need_access_check)
    {
        const auto ownerUid = resource->getOwnerUid();
        if (!checkUidMatch(ownerUid, allowedProviders.value()))
        {
            score::mw::log::LogWarn("shm") << "Could not open Shared Memory. Uid of " << ownerUid
                                         << " provided does not exist in allowedProviders. This is likely a "
                                            "misconfiguration of allowedProviders.";
            return nullptr;
        }
    }
    return std::static_pointer_cast<ISharedMemoryResource>(resource);
}

auto score::memory::shared::SharedMemoryFactoryImpl::Create(std::string path,
                                                          InitializeCallback cb,
                                                          const std::size_t user_space_to_reserve,
                                                          const UserPermissions& permissions,
                                                          const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    std::lock_guard<std::mutex> lock{mutex_};
    if (GetResourceIfAlreadyOpened(path, resources_) != nullptr)
    {
        score::mw::log::LogWarn("shm") << "Cannot recreated Shared Memory " << path << "as it's already been created.";
        return nullptr;
    }

    if (prefer_typed_memory && (typed_memory_ptr_ == nullptr))
    {
        score::mw::log::LogError("shm")
            << "Shared memory has to be created in typed memory but no typed memory instance has "
               "been provided using the public interface SetTypedMemoryProvider ";
        return nullptr;
    }

    const auto typed_memory_ptr = prefer_typed_memory ? typed_memory_ptr_ : nullptr;
    const auto result = SharedMemoryResource::Create(
        path, user_space_to_reserve, std::move(cb), permissions, &CreateAccessControlList, typed_memory_ptr);
    if (!result.has_value())
    {
        score::mw::log::LogWarn("shm") << "Could not create Shared Memory " << path << ":" << result.error();
        return nullptr;
    }
    InsertResourceIntoMap(path, result.value(), resources_);
    return std::static_pointer_cast<ISharedMemoryResource>(result.value());
}

auto score::memory::shared::SharedMemoryFactoryImpl::CreateAnonymous(std::uint64_t shared_memory_resource_id,
                                                                   InitializeCallback cb,
                                                                   const std::size_t user_space_to_reserve,
                                                                   const UserPermissions& permissions,
                                                                   const bool prefer_typed_memory) noexcept
    -> std::shared_ptr<ISharedMemoryResource>
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (prefer_typed_memory && (typed_memory_ptr_ == nullptr))
    {
        score::mw::log::LogError("shm")
            << "Shared memory has to be created in typed memory but no typed memory instance has "
               "been provided using the public interface SetTypedMemoryProvider ";
        return nullptr;
    }

    const auto typed_memory_ptr = prefer_typed_memory ? typed_memory_ptr_ : nullptr;
    const auto result = SharedMemoryResource::CreateAnonymous(shared_memory_resource_id,
                                                              user_space_to_reserve,
                                                              std::move(cb),
                                                              permissions,
                                                              &CreateAccessControlList,
                                                              typed_memory_ptr);
    // LCOV_EXCL_START (Defensive programming: CreateAnonymous either returns a valid result or terminates.)
    // LCOV_EXCL_BR_START (See line coverage suppression explanation)
    if (!result.has_value())
    {
        score::mw::log::LogWarn("shm") << "Could not create anonymous Shared Memory:" << result.error();
        return nullptr;
    }
    // LCOV_EXCL_BR_STOP
    // LCOV_EXCL_STOP
    return std::static_pointer_cast<ISharedMemoryResource>(result.value());
}

// coverity[autosar_cpp14_a15_5_3_violation : FALSE] See rationale for score::cpp::expected::value() above.
auto score::memory::shared::SharedMemoryFactoryImpl::CreateOrOpen(
    std::string path,
    InitializeCallback cb,
    const std::size_t user_space_to_reserve,
    const SharedMemoryResource::AccessControl access_control,
    const bool prefer_typed_memory) noexcept -> std::shared_ptr<ISharedMemoryResource>
{
    std::lock_guard<std::mutex> lock{mutex_};
    std::shared_ptr<SharedMemoryResource> resource = GetResourceIfAlreadyOpened(path, resources_);
    if (resource == nullptr)
    {
        if ((prefer_typed_memory) && (typed_memory_ptr_ == nullptr))
        {
            score::mw::log::LogError("shm")
                << "Shared memory has to be created in typed memory but no typed memory instance "
                   "has been provided using the public interface SetTypedMemoryProvider ";
            return nullptr;
        }

        const auto typed_memory_ptr = prefer_typed_memory ? typed_memory_ptr_ : nullptr;
        const auto result = SharedMemoryResource::CreateOrOpen(path,
                                                               user_space_to_reserve,
                                                               std::move(cb),
                                                               access_control.permissions_,
                                                               &CreateAccessControlList,
                                                               typed_memory_ptr);
        if (!result.has_value())
        {
            score::mw::log::LogWarn("shm") << __func__ << __LINE__ << "Could not create or open Shared Memory " << path
                                         << ":" << result.error();
            return nullptr;
        }
        resource = result.value();
        InsertResourceIntoMap(path, resource, resources_);
    }

    const bool need_access_check = access_control.allowedProviders_.has_value();
    if (need_access_check)
    {
        if (!checkUidMatch(resource->getOwnerUid(), access_control.allowedProviders_.value()))
        {
            score::mw::log::LogWarn("shm")
                << __func__ << __LINE__
                << "Shared memory object could be opened, but the creator/provider isn't in the "
                   "allowedProviders list. This is likely a misconfiguration of allowedProviders";
            return nullptr;
        }
    }
    return std::static_pointer_cast<ISharedMemoryResource>(resource);
}

auto SharedMemoryFactoryImpl::Remove(const std::string& path) noexcept -> void
{
    std::lock_guard<std::mutex> lock{mutex_};
    std::shared_ptr<SharedMemoryResource> resource = GetResourceIfAlreadyOpened(path, resources_);
    if (resource != nullptr)
    {
        const auto number_resources_removed = resources_.erase(path);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(number_resources_removed == 1U);
        resource->UnlinkFilesystemEntry();
    }
}

auto SharedMemoryFactoryImpl::RemoveStaleArtefacts(const std::string& path) noexcept -> void
{
    {
        std::lock_guard<std::mutex> lock{mutex_};
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            GetResourceIfAlreadyOpened(path, resources_) == nullptr,
            "RemoveStaleArtefacts must not be called when the path corresponds to a currently owned resource.");
    }
    const auto lock_file_path = SharedMemoryResource::GetLockFilePath(path);
    score::cpp::ignore = ::score::os::Unistd::instance().unlink(lock_file_path.data());

    // This requirement broken_link_c/issue/57467 directly excludes memory::shared (which is
    // part of mw::com) from the ban by listing it in the not relevant for field.
    // NOLINTNEXTLINE(score-banned-function): explanation on lines above.
    score::cpp::ignore = ::score::os::Mman::instance().shm_unlink(path.c_str());
}

auto SharedMemoryFactoryImpl::SetTypedMemoryProvider(std::shared_ptr<TypedMemory> typed_memory_ptr) noexcept -> void
{
    typed_memory_ptr_ = typed_memory_ptr;
}

}  // namespace score::memory::shared
