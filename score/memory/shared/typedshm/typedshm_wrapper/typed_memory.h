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
#ifndef SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TYPED_MEMORY_H
#define SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TYPED_MEMORY_H

#include "score/memory/shared/user_permission.h"
#include "score/os/errno.h"

#if defined(__QNX__) && defined(USE_TYPEDSHMD)
#include "platform/aas/intc/typedmemd/code/clientlib/i_typedsharedmemory.h"
#include "platform/aas/intc/typedmemd/code/clientlib/typedsharedmemory.h"
#include "score/os/qnx/mman.h"
#include "score/os/qnx/mman_impl.h"
#endif

#include "score/expected.hpp"

#include <sys/mman.h>
#include <cstdint>
#include <memory>
#include <string>

namespace score::memory::shared
{

class TypedMemory
{
  public:
    virtual ~TypedMemory() = default;
    /*!
     * \brief Creates a new instance of the production implementation.
     * \details This is to enable the usage of OSAL without the Singleton instance().
     */
    static std::shared_ptr<TypedMemory> Default() noexcept;

    virtual score::cpp::expected_blank<score::os::Error> AllocateNamedTypedMemory(
        const std::size_t shm_size,
        const std::string shm_name,
        const permission::UserPermissions& permissions) const noexcept = 0;

    virtual score::cpp::expected<int, score::os::Error> AllocateAndOpenAnonymousTypedMemory(
        const std::uint64_t shm_size) const noexcept = 0;

    virtual score::cpp::expected_blank<score::os::Error> Unlink(const std::string shm_name) const noexcept = 0;

  protected:
    // Make all special member functions protected to prevent them ever being explicitly called (which can lead to
    // slicing errors) but can be called by child classes (so that they can automatically generate special member
    // functions)
    TypedMemory() noexcept = default;
    TypedMemory(const TypedMemory&) noexcept = default;
    TypedMemory& operator=(const TypedMemory&) noexcept = default;
    TypedMemory(TypedMemory&&) noexcept = default;
    TypedMemory& operator=(TypedMemory&&) noexcept = default;
};

namespace internal
{
class TypedMemoryImpl final : public TypedMemory
{
  public:
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist in the
// same file. This keeps both implementations close (i.e. within the same functions) which makes the code easier to
// read and maintain. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
    explicit TypedMemoryImpl(
        std::unique_ptr<score::os::qnx::MmanQnx> mman = std::make_unique<score::os::qnx::MmanQnxImpl>(),
        std::unique_ptr<score::tmd::ITypedSharedMemory> typed_shm_client =
            std::make_unique<score::tmd::TypedSharedMemory>());
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
    score::cpp::expected_blank<score::os::Error> AllocateNamedTypedMemory(
        const std::size_t shm_size,
        const std::string shm_name,
        const permission::UserPermissions& permissions) const noexcept override;

    score::cpp::expected<int, score::os::Error> AllocateAndOpenAnonymousTypedMemory(
        const std::uint64_t shm_size) const noexcept override;

    score::cpp::expected_blank<score::os::Error> Unlink(const std::string shm_name) const noexcept override;

// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
  private:
    std::unique_ptr<score::os::qnx::MmanQnx> mman_;
    std::unique_ptr<score::tmd::ITypedSharedMemory> typed_shm_client_;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
};

}  // namespace internal
}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TYPED_MEMORY_H
