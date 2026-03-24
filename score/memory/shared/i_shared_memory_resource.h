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
#ifndef SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_RESOURCE_H
#define SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_RESOURCE_H

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/user_permission.h"

#include "score/os/acl.h"
#include "score/os/utils/acl/i_access_control_list.h"

#include <score/callback.hpp>
#include <score/span.hpp>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace score::memory::shared
{

class SharedMemoryResource;

class ISharedMemoryResource : public ManagedMemoryResource
{
  public:
    using WorldReadable = permission::WorldReadable;
    using WorldWritable = permission::WorldWritable;
    using UserPermissionsMap = permission::UserPermissionsMap;
    using UserPermissions = permission::UserPermissions;

    // size of stored callback should be the base size of amp callback and a unique_ptr
    // this way the user can pass any information to the callback through the pointer.
    static constexpr std::size_t kCallbackSize{
        sizeof(score::cpp::callback<void()>) +
        std::max(score::cpp::callback<void()>::alignment_t::value, sizeof(std::unique_ptr<void>))};
    using InitializeCallback = score::cpp::callback<void(std::shared_ptr<ISharedMemoryResource>), kCallbackSize>;

    using FileDescriptor = os::Acl::FileDescriptor;

    using AccessControlListFactory = score::cpp::callback<std::unique_ptr<score::os::IAccessControlList>(FileDescriptor)>;

    class AccessControl
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.".
        // Rationale: There are no class invariants to maintain which could be violated by directly accessing these
        // member variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        const UserPermissions& permissions_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const std::optional<score::cpp::span<const uid_t>> allowedProviders_;
    };

    ISharedMemoryResource() noexcept = default;
    ~ISharedMemoryResource() noexcept override = default;

    virtual const std::string* getPath() const noexcept = 0;
    virtual void UnlinkFilesystemEntry() const noexcept = 0;
    virtual FileDescriptor GetFileDescriptor() const noexcept = 0;
    virtual bool IsShmInTypedMemory() const noexcept = 0;
    virtual std::string_view GetIdentifier() const noexcept = 0;

  protected:
    ISharedMemoryResource(const ISharedMemoryResource&) noexcept = default;
    ISharedMemoryResource(ISharedMemoryResource&&) noexcept = default;
    ISharedMemoryResource& operator=(const ISharedMemoryResource&) noexcept = default;
    ISharedMemoryResource& operator=(ISharedMemoryResource&&) noexcept = default;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_I_SHARED_MEMORY_RESOURCE_H
