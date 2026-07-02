/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_TYPED_MEMORY_H
#define QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_TYPED_MEMORY_H

#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include <score/expected.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace qemu_shared_ivshmem
{

// A custom lib-memory "typed memory" provider that backs a named shared-memory object with
// the ivshmem BAR instead of ordinary /dev/shmem RAM.
//
// This is the single seam that lets *unmodified* score::memory::shared map onto the BAR:
// when injected via SharedMemoryFactory::SetTypedMemoryProvider(), a subsequent
// Create(path, ..., prefer_typed_memory=true) delegates the allocation here. We bind the
// shm name to the BAR's physical address with shm_ctl(fd, SHMCTL_PHYS, paddr, size); the
// factory then does its usual shm_open(path)+mmap, which now lands on the BAR (and skips
// ftruncate/seal/permissions).
//
// Both VMs mapping the same BAR at different virtual addresses is fine because lib memory
// stores every pointer as an OffsetPtr (self-relative offset).
class BarTypedMemory final : public score::memory::shared::TypedMemory
{
  public:
    // paddr/size come from DiscoverIvshmemBar().
    BarTypedMemory(std::uint64_t paddr, std::uint64_t size) noexcept;

    // Binds `shm_name` to the ivshmem BAR physical address so a later shm_open()+mmap of
    // that name maps the BAR. This is the only method the factory actually needs.
    score::cpp::expected_blank<score::os::Error> AllocateNamedTypedMemory(
        std::size_t shm_size,
        std::string shm_name,
        const score::memory::shared::permission::UserPermissions& permissions) const noexcept override;

    // Anonymous typed memory is not supported for a fixed device BAR.
    score::cpp::expected<int, score::os::Error> AllocateAndOpenAnonymousTypedMemory(
        std::uint64_t shm_size) const noexcept override;

    score::cpp::expected_blank<score::os::Error> Unlink(std::string_view shm_name) const noexcept override;

    score::cpp::expected<uid_t, score::os::Error> GetCreatorUid(std::string_view shm_name) const noexcept override;

  private:
    std::uint64_t paddr_;
    std::uint64_t size_;
};

}  // namespace qemu_shared_ivshmem

#endif  // QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_TYPED_MEMORY_H
