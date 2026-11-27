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
#ifndef SCORE_LIB_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H
#define SCORE_LIB_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H

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

    void* allocate(const std::size_t, const std::size_t = alignof(std::max_align_t)) const;
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
    ///          Reasoning for having bounds-checking on the level of the MemoryResourceProxy beside the bounds-checking
    ///          done by the OffsetPtr: The bounds-checking done in MemoryResourceProxy::allocate verifies, that no
    ///          memory gets allocated from a different memory resource (SharedMemoryResource). I.e. the
    ///          memory_identifier_ residing in shared-memory could be corrupted in a way, that it identifies a
    ///          different memory resource, which is valid/registered for the process calling allocate(). This could
    ///          then lead to the case, that an MemoryResourceProxy::allocate() call on a MemoryResourceProxy residing
    ///          in shared-mem A leads to an allocation within shared-mem B! While this issue would be detected, when
    ///          referencing the resulting OffsetPtr within shared-mem A (with OffsetPtr based bounds-checking) we would
    ///          already have allocated the memory from shared-mem B, thus shared-mem B available memory has been
    ///          reduced. I.e. the bounds-checking done in MemoryResourceProxy::allocate() is a measure, which avoids
    ///          availability issues in other shared-memory resources! The bounds-checking done by the OffsetPtr is a
    ///          measure to avoid memory corruption, thus it is much more important from safety perspective.
    /// \param enable true enables, false disables bounds-checking
    /// \return previous value
    static bool EnableBoundsChecking(const bool enable) noexcept;

  private:
    void PerformBoundsCheck(const std::uint64_t memory_identifier) const;

    const std::uint64_t memory_identifier_;
    /// \brief shall bounds-checking be enabled or not within the process.
    static bool bounds_checking_enabled_;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_MEMORY_RESOURCE_PROXY_H
