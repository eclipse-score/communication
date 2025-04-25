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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H
#define BASELIBS_SCORE_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H

#include <cstddef>
#include <cstdint>

namespace score::memory::shared
{

/**
 * \brief The `MemoryResourceProxy` implements a simple dispatching to a process specific `ManagedMemoryResource`.
 *
 *        Since the `ManagedMemoryResource` will be specific to a process and cannot be shared over e.g. shared memory
 *        (since it might hold for example a process specific file descriptor) the `MemoryResourceProxy` will use
 *        the process specific `MemoryResourceRegistry` to dispatch any memory allocation / de-allocation calls
 *        to the correction resource.
 *
 *        Bounds checking: The `MemoryResourceProxy` must reside in the memory region allocated by the
 *        `ManagedMemoryResource` that the `MemoryResourceProxy` points to via the `MemoryResourceRegistry`. This allows
 *        us to check that the memory identifier stored in the `MemoryResourceProxy` refers to the correct memory in the
 *        `MemoryResourceRegistry`. It does this by checking whether the `MemoryResourceProxy` lies within that region
 *        and calling std::terminate if it doesn't.
 *
 *        It should be ensured that any new ManagedMemoryResource constructs the `MemoryResourceProxy` within its
 *        managed memory region. The copy / move constructors and operators have been deleted so that the
 *        `MemoryResourceProxy` must stay where it is originally allocated and can therefore ensure that the bounds
 *        check will always be valid.
 *
 *        Each `PolymorphicOffsetPtrAllocator` contains an OffsetPtr to a `MemoryResourceProxy` which allows it
 *        to lookup the corresponding `ManagedMemoryResource` in the `MemoryResourceRegistry`.
 */

// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. MemoryResourceProxy is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class MemoryResourceProxy
{
  public:
    /// \brief ctor for MemoryResourceProxy
    /// \param identifier unique identifier representing a memory resource. In case of a shared-memory resource all
    ///        participants (processes) have to use the same identifier for the same shared-memory object.
    explicit MemoryResourceProxy(const std::uint64_t identifier);
    ~MemoryResourceProxy() = default;
    MemoryResourceProxy(const MemoryResourceProxy&) = delete;
    MemoryResourceProxy& operator=(const MemoryResourceProxy&) = delete;
    MemoryResourceProxy(MemoryResourceProxy&&) noexcept = delete;
    MemoryResourceProxy& operator=(MemoryResourceProxy&&) noexcept = delete;

    void* allocate(const std::size_t, const std::size_t = alignof(std::max_align_t)) const noexcept;
    void deallocate(void* const, const std::size_t) const;

    friend bool operator==(const MemoryResourceProxy& lhs, const MemoryResourceProxy& rhs) noexcept;
    friend bool operator!=(const MemoryResourceProxy& lhs, const MemoryResourceProxy& rhs) noexcept;

    /// \brief Enables/disables bounds-checking done during allocate().
    /// \details For performance reason in QM environment, the bounds-checking can be disabled. Note, that it is
    ///          important to have this as a process global static setting and NOT as an instance specific setting!
    ///          Instances of MemoryResourceProxy are stored in shared-memory and therefore potentially accessible/
    ///          mutable by QM processes. If a QM process would alter the value of bounds_checking_enabled_ (in case of
    ///          it being a member variable), it could corrupt the behaviour of an ASIL-B process (disabling its
    ///          bounds-checking)!
    /// \param enable true enables, false disables bounds-checking
    /// \return previous value
    static bool EnableBoundsChecking(const bool enable) noexcept;

  private:
    void PerformBoundsCheck(const std::uint64_t memory_identifier) const noexcept;

    const std::uint64_t memory_identifier_;
    /// \brief shall bounds-checking be enabled or not within the process.
    static bool bounds_checking_enabled_;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H
