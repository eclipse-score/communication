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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_NEW_DELETE_DELEGATE_RESOURCE_H
#define BASELIBS_SCORE_MEMORY_SHARED_NEW_DELETE_DELEGATE_RESOURCE_H

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/memory_resource_proxy.h"

#include <score/memory_resource.hpp>

#include <cstdint>
#include <functional>
#include <map>

namespace score::memory::shared
{

/// \brief This class can be used as a "dummy" memory resource to log/monitor allocation needs of certain code-paths.
///
/// \details When creating a shared memory object, you need to give it a size. Later changing the size (ftruncate) and
///          then re-mapping it is often impossible. So you need a valid estimation of the needed space before.
///          This can be solved by a so called dry-run, where all the code, which would initialize the related
///          shm-object not on executed against the real SharedMemoryResource, but
///          on this NewDeleteDelegateMemoryResource, which logs/monitors all allocate-calls, so that after the
///          initialization code has been run on this NewDeleteDelegateMemoryResource, one can read out, how much memory
///          was effectively allocated. Then one can use this value to create the real shm-object/SharedMemoryResource
///          with the correct size!
class NewDeleteDelegateMemoryResource : public score::memory::shared::ManagedMemoryResource
{
  public:
    using InitializeCallback = std::function<void()>;
    /// \brief ctor of NewDeleteDelegateMemoryResource
    /// \param mem_res_id unique identifier for the resource under which it will get registered in the registry.
    explicit NewDeleteDelegateMemoryResource(
        const std::uint64_t mem_res_id,
        score::cpp::pmr::memory_resource* upstream_resource = score::cpp::pmr::new_delete_resource()) noexcept;

    NewDeleteDelegateMemoryResource(const NewDeleteDelegateMemoryResource&) = delete;
    NewDeleteDelegateMemoryResource(const NewDeleteDelegateMemoryResource&&) noexcept = delete;
    NewDeleteDelegateMemoryResource& operator=(const NewDeleteDelegateMemoryResource&) = delete;
    NewDeleteDelegateMemoryResource& operator=(const NewDeleteDelegateMemoryResource&&) noexcept = delete;

    ~NewDeleteDelegateMemoryResource() override;

    const MemoryResourceProxy* getMemoryResourceProxy() noexcept override;
    void* getBaseAddress() const noexcept override;
    void* getUsableBaseAddress() const noexcept override;

    std::size_t GetUserAllocatedBytes() const noexcept override;
    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return true;
    }

  private:
    void* do_allocate(const std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const ::score::cpp::pmr::memory_resource& other) const noexcept override;
    const void* getEndAddress() const noexcept override;

    score::cpp::pmr::memory_resource* upstream_resource_;
    std::uint64_t memory_resource_id_;
    score::memory::shared::MemoryResourceProxy proxy_;
    std::size_t sum_allocated_bytes_;

    class AllocateInfo
    {
      public:
        AllocateInfo(std::size_t b, std::size_t a) : bytes{b}, alignment{a} {};

        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.".
        // Rationale: There are no class invariants to maintain which could be violated by directly accessing these
        // member variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::size_t bytes;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::size_t alignment;
    };
    std::map<void*, AllocateInfo> current_upstream_allocations_;
};

}  // namespace score::memory::shared
#endif  // BASELIBS_SCORE_MEMORY_SHARED_NEW_DELETE_DELEGATE_RESOURCE_H
