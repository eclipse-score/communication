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
#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_typed_memory.h"

#include <fcntl.h>
#include <sys/mman.h>  // shm_open, shm_unlink, shm_ctl_special, SHMCTL_PHYS, SHMCTL_HAS_SPECIAL (QNX)
#include <unistd.h>

#include <cerrno>

#if defined(__QNXNTO__) && (defined(__X86_64__) || defined(__x86_64__))
// X86_64_PTE_MTYPE_WB: the "special" value that tells shm_ctl_special() to map the
// physical region Write-Back cacheable (the header documents these as being "for use
// with shm_ctl_special()"). ivshmem-plain is host-RAM backed, so WB is correct and lets
// normal loads/stores/atomics work on the BAR.
#include <x86_64/mmu.h>
#endif

namespace qemu_shared_ivshmem
{

BarTypedMemory::BarTypedMemory(std::uint64_t paddr, std::uint64_t size) noexcept : paddr_{paddr}, size_{size} {}

score::cpp::expected_blank<score::os::Error> BarTypedMemory::AllocateNamedTypedMemory(
    std::size_t /*shm_size*/,
    std::string shm_name,
    const score::memory::shared::permission::UserPermissions& /*permissions*/) const noexcept
{
#if defined(__QNXNTO__)
    // Create the named shared-memory object...
    const int fd = ::shm_open(shm_name.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno());
    }

    // ...and bind it to the ivshmem BAR physical address. After this, a plain
    // shm_open(shm_name)+mmap (which the factory does next) maps the BAR, not fresh RAM.
    //
    // We request Write-Back caching via shm_ctl_special(X86_64_PTE_MTYPE_WB) rather than a
    // plain shm_ctl(SHMCTL_PHYS) (which maps the BAR UNCACHEABLE): ivshmem-plain is host-RAM
    // backed and KVM keeps it hardware-cache-coherent across guests, so WB is correct and
    // lets ordinary loads/stores/atomics work on the BAR.
    //
    // NOTE (verified on target): WB is ACCEPTED (rc=0), but it is NOT sufficient to run the
    // production score::memory::shared umbrella over the BAR. That resource places a QNX
    // process-shared pthread mutex (SharedMemoryResource::ControlBlock) at the start of the
    // region and locks it; on SHMCTL_PHYS device-BAR memory that lock returns EINVAL(22) and
    // aborts -- a QNX limitation on process-shared sync objects over physically-mapped device
    // memory, independent of cache type. The mutex-free hand-rolled OffsetPtr demo in
    // ../example is the viable cross-VM path over ivshmem.
#if defined(__X86_64__) || defined(__x86_64__)
    int rc = ::shm_ctl_special(
        fd, SHMCTL_PHYS | SHMCTL_HAS_SPECIAL, paddr_, size_, static_cast<unsigned>(X86_64_PTE_MTYPE_WB));
    if (rc == -1)
    {
        // WB rejected (older/other QNX): fall back to the default uncacheable binding.
        rc = ::shm_ctl(fd, SHMCTL_PHYS, paddr_, size_);
    }
#else
    const int rc = ::shm_ctl(fd, SHMCTL_PHYS, paddr_, size_);
#endif
    if (rc == -1)
    {
        const int saved = errno;
        (void)::close(fd);
        (void)::shm_unlink(shm_name.c_str());
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(saved));
    }

    // The object persists (name + physical binding) until Unlink(); our fd is no longer
    // needed because the factory re-opens by name.
    (void)::close(fd);
    return {};
#else
    (void)shm_name;
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
#endif
}

score::cpp::expected<int, score::os::Error> BarTypedMemory::AllocateAndOpenAnonymousTypedMemory(
    std::uint64_t /*shm_size*/) const noexcept
{
    // A device BAR is a fixed, named physical region; anonymous allocation is meaningless.
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
}

score::cpp::expected_blank<score::os::Error> BarTypedMemory::Unlink(std::string_view shm_name) const noexcept
{
#if defined(__QNXNTO__)
    const std::string name{shm_name};
    if (::shm_unlink(name.c_str()) == -1)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno());
    }
    return {};
#else
    (void)shm_name;
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOSYS));
#endif
}

score::cpp::expected<uid_t, score::os::Error> BarTypedMemory::GetCreatorUid(
    std::string_view /*shm_name*/) const noexcept
{
    return ::getuid();
}

}  // namespace qemu_shared_ivshmem
