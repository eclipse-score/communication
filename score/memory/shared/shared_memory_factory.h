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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_H

#include "score/memory/shared/i_shared_memory_factory.h"
#include "score/memory/shared/i_shared_memory_resource.h"

#include <memory>
#include <string>

namespace score::memory::shared
{

/**
 * @brief The SharedMemoryResource shall be instantiated only once per process. This is necessary
 *        since this object will mmap the shared memory into the process. If we would open the same
 *        shared memory twice, we would mmap it twice into the same process, which could cause
 *        odd behaviour.
 *        In order to overcome this issue, we decided to implement a Factory, which will ensure
 *        that we don't open the same shared memory twice.
 *
 * @details Open, Create, CreateOrOpen and Remove can be safely called concurrently. For each instance of
 *          SharedMemoryFactory (i.e. per process), only once will one of the calls (Open, Create or CreateOrOpen) of
 *          the underlying SharedMemoryResource be called. All other calls to Open, Create or CreateOrOpen will return a
 *          memory resource from the internal map.
 *          The parameters (except for allowedProviders) of the calls to Open, Create, and CreateOrOpen
 *          for the same path shall be consistent within the same process, as they will likely be ignored if the already
 *          existing instance is returned from the map.
 */

class SharedMemoryFactoryMock;

class SharedMemoryFactory
{
  public:
    using InitializeCallback = ISharedMemoryResource::InitializeCallback;
    using WorldReadable = ISharedMemoryResource::WorldReadable;
    using WorldWritable = ISharedMemoryResource::WorldWritable;
    using UserPermissionsMap = ISharedMemoryResource::UserPermissionsMap;
    using UserPermissions = ISharedMemoryResource::UserPermissions;
    using AccessControl = ISharedMemoryResource::AccessControl;

    /// \brief Obtain a memory resource for an existing memory region. The whole region will be mmapped.
    /// \param path name of the memory region to open: a string consisting of an initial
    ///        slash, followed by one or more characters, none of which are slashes
    /// \param is_read_write true if memory resource is to be opened with write access
    /// \param allowedProviders list of the UIDs of allowed creators of the memory region (see details)
    /// \return smart pointer to the memory resource from the internal map. Nullptr if no such memory region exists.
    /// \details for safety-related reasons, when accessing the memory region potentially created by another
    ///          process (Open and CreateOrOpen calls), one can specify the list of UIDs (allowedProviders) that can be
    ///          the creators of the memory region. If the actual creator is not from the list, the call returns
    ///          nullptr. The caller's UID is automatically whitelisted (allowed) for consistency with creation;
    ///          an empty optional (i.e. no value) means no restriction at all; if the optional has a value, but the
    ///          value is an empty list/span the call returns a nullptr. Otherwise the UIDs in the spawn are checked
    ///          against the UID of the creator. If there is NO match a nullptr is returned.
    ///          The restrictions apply on per call basis; multiple calls for the same
    ///          SharedMemoryResource can return either nullptr or the SharedMemoryResource in the internal map,
    ///          depending on the allowedProviders list specified in each call.
    static std::shared_ptr<ISharedMemoryResource> Open(
        const std::string& path,
        const bool is_read_write,
        const std::optional<score::cpp::v1::span<const uid_t>>& allowedProviders = std::nullopt) noexcept;

    /// \brief Obtain a memory resource for a newly created memory region.
    /// \param path name of the memory region to create: a string consisting of an initial
    ///        slash, followed by one or more characters, none of which are slashes
    /// \param cb callback to initialize the created memory region
    /// \param user_space_to_reserve amount of address space (in bytes) to map. Create call might internally add some
    ///        bytes for its own control structures, it needs to add, so the mmaped address space might be larger.
    /// \param permissions variant specifying the access rights to the created memory region.
    ///        WorldReadable tag type if the region is read/writable to the user and readable to the rest.
    ///        WorldWritable tag type if the region is read/writable to the user and read/writable to the rest.
    ///        otherwise UserPermissionsMap containing the ACL (also, read/writable to the user)
    /// \param prefer_typed_memory specifying the preferred location of the shared-memory object: whether it
    ///        has to be allocated in typed memory or in the os system memory.
    /// \return a smart pointer to the shared-memory resource from the internal map. Nullptr if such a memory region
    ///         already exists or the shared-memory resource could not be created.
    static std::shared_ptr<ISharedMemoryResource> Create(std::string path,
                                                         InitializeCallback cb,
                                                         const std::size_t user_space_to_reserve,
                                                         const UserPermissions& permissions = UserPermissionsMap{},
                                                         const bool prefer_typed_memory = false) noexcept;

    /// \brief Obtain a memory resource for a newly created anonymous memory region.
    /// \attention This implementation only works in QNX environment because typed memory is only implemented for QNX
    /// and anonymous shared memory allocation in system memory uses QNX specific shm_open() parameters. In non-QNX
    /// environment this function will return nullptr.
    /// \param shared_memory_resource_id a number that identifies the created memory region. This number must be unique
    ///        ECU wide. This could be achieved e.g. by combining the PID with a process wide unique identifier and
    ///        hash it:
    ///        \code hash(pid + process_wide_unique_identifier) \endcode
    /// \param cb callback to initialize the created memory region
    /// \param user_space_to_reserve amount of address space (in bytes) to map. Create call might internally add some
    ///        bytes for its own control structures, it needs to add, so the mmaped address space might be larger.
    /// \param permissions variant specifying the access rights to the created memory region.
    ///        WorldReadable tag type if the region is read/writable to the user and readable to the rest.
    ///        WorldWritable tag type if the region is read/writable to the user and read/writable to the rest.
    ///        otherwise UserPermissionsMap containing the ACL (also, read/writable to the user)
    /// \param prefer_typed_memory specifying the preferred location of the shared-memory object: whether it
    ///        has to be allocated in typed memory or in the os system memory.
    /// \return a smart pointer to the shared-memory resource from the internal map. Nullptr if such a memory region
    ///         already exists or the shared-memory resource could not be created.
    static std::shared_ptr<ISharedMemoryResource> CreateAnonymous(
        std::uint64_t shared_memory_resource_id,
        InitializeCallback cb,
        const std::size_t user_space_to_reserve,
        const UserPermissions& permissions = UserPermissionsMap{},
        const bool prefer_typed_memory = false) noexcept;

    /// \brief Obtain a memory resource for an existing or newly created memory region.
    /// \param path name of the memory region to open or create: a string consisting of an initial
    ///        slash, followed by one or more characters, none of which are slashes
    /// \param cb callback to initialize the newly created memory region
    /// \param user_space_to_reserve amount of address space (in bytes) to map. This is only taken into account in case
    ///        a create happens! See param doc in SharedMemoryFactory::Open()
    /// \param access_control a data structure which contains two elements:
    ///        1- permissions_ variant specifying the access rights to the created memory region (see details at Create)
    ///        2- allowedProviders_ list of the UIDs of allowed creators of the memory region (see details at Open)
    /// \param prefer_typed_memory specifying the preferred location of the shared-memory object whether it
    ///        has to be allocated in typed memory or in the os system memory.
    /// \return a smart pointer to the shared-memory resource from the internal map.
    ///         If memory resource doesn't yet exist and creation failed, it returns a nullptr.
    static std::shared_ptr<ISharedMemoryResource> CreateOrOpen(std::string path,
                                                               InitializeCallback cb,
                                                               const std::size_t user_space_to_reserve,
                                                               const AccessControl access_control = {{}, {}},
                                                               const bool prefer_typed_memory = false) noexcept;

    /// \brief Removes any SharedMemoryResource corresponding to path from the SharedMemoryFactory
    /// \param path name of the memory region to open or create: a string consisting of an initial
    ///        slash, followed by one or more characters, none of which are slashes
    ///
    /// This function will first check if any SharedMemoryResource corresponding to path is currently stored within the
    /// SharedMemoryFactory. If it is, then it is removed from the internal storage and Remove is called on that
    /// resource.
    static void Remove(const std::string& path) noexcept;

    /// \brief Removes stale shared memory artefacts from the filesystem in case a process crashed while creating a
    ///        SharedMemoryResource.
    /// \param path name of the memory region to open or create: a string consisting of an initial slash, followed by
    /// one or more characters, none of which are slashes
    ///
    /// This function must not be called with a path that has been Created or Opened by this instance of the
    /// SharedMemoryFactory. It should only be used to cleanup left over artefacts from a previously crashed process.
    /// After calling RemoveStaleArtefacts with a given path, that path can then be created again with
    /// SharedMemoryFactory::Create().
    static void RemoveStaleArtefacts(const std::string& path) noexcept;

    static void SetTypedMemoryProvider(std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

    static std::size_t GetControlBlockSize() noexcept
    {
        return instance().GetControlBlockSize();
    };

    static void Clear() noexcept
    {
        instance().Clear();
    }

    /// \brief Injects mock.
    /// \param mock pointer to a Mock object. If it is not nullptr
    ///        all calls are redirected to provided object.
    static void InjectMock(ISharedMemoryFactory* mock) noexcept;

  private:
    static ISharedMemoryFactory& instance() noexcept;
    static ISharedMemoryFactory* mock_;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_H
