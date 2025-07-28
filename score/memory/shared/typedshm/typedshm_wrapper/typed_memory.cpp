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
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#if defined(__QNX__) && defined(USE_TYPEDSHMD)
#include "platform/aas/intc/typedmemd/code/clientlib/typedsharedmemory.h"
#include "score/os/qnx/mman_impl.h"
#endif

#include <cerrno>
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
#include <variant>
#endif

namespace score::memory::shared
{

namespace
{

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist in the same
// file. This keeps both implementations close (i.e. within the same functions) which makes the code easier to read and
// maintain. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
score::tmd::UserPermissions GetUserPermissions(const permission::UserPermissions& permissions) noexcept
{
    if (std::holds_alternative<permission::WorldWritable>(permissions))
    {
        return score::tmd::AccessMode::kWorldWritable;
    }
    else if (std::holds_alternative<permission::WorldReadable>(permissions))
    {
        return score::tmd::AccessMode::kWorldReadable;
    }

    auto* const user_permissions = std::get_if<score::tmd::UserPermissionsMap>(&permissions);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(user_permissions != nullptr, "Could not get user permissions.");
    return *user_permissions;
}

// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif

}  // namespace
namespace internal
{

// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
TypedMemoryImpl::TypedMemoryImpl(std::unique_ptr<score::os::qnx::MmanQnx> mman,
                                 std::unique_ptr<score::tmd::ITypedSharedMemory> typed_shm_client)
    : TypedMemory(), mman_(std::move(mman)), typed_shm_client_(std::move(typed_shm_client))
{
}
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif

score::cpp::expected_blank<score::os::Error> TypedMemoryImpl::AllocateNamedTypedMemory(
    const std::size_t shm_size,
    const std::string shm_name,
    const permission::UserPermissions& permissions) const noexcept
{
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
    return typed_shm_client_->AllocateNamedTypedMemory(shm_size, shm_name, GetUserPermissions(permissions));
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#else
    static_cast<void>(shm_size);
    static_cast<void>(shm_name);
    static_cast<void>(permissions);
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
}

score::cpp::expected<int, score::os::Error> TypedMemoryImpl::AllocateAndOpenAnonymousTypedMemory(
    const std::uint64_t shm_size) const noexcept
{
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
    shm_handle_t shm_handle{};
    const auto allocate_anonymous_typed_memory_result =
        typed_shm_client_->AllocateHandleTypedMemory(shm_size, &shm_handle);
    if (!allocate_anonymous_typed_memory_result.has_value())
    {
        return score::cpp::make_unexpected(allocate_anonymous_typed_memory_result.error());
    }
    const auto shm_open_result = mman_->shm_open_handle(shm_handle, O_RDWR);
    if (!shm_open_result.has_value())
    {
        return score::cpp::make_unexpected(shm_open_result.error());
    }
    return {shm_open_result.value()};
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#else
    static_cast<void>(shm_size);
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
}

score::cpp::expected_blank<score::os::Error> TypedMemoryImpl::Unlink(const std::string shm_name) const noexcept
{
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
    return typed_shm_client_->Unlink(shm_name);
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#else
    static_cast<void>(shm_name);
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
}

}  // namespace internal

std::shared_ptr<score::memory::shared::TypedMemory> score::memory::shared::TypedMemory::Default() noexcept
{
    return std::make_shared<internal::TypedMemoryImpl>();
}

}  // namespace score::memory::shared
