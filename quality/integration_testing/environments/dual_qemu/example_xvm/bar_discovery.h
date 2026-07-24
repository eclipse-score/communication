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
#ifndef QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_DISCOVERY_H
#define QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_DISCOVERY_H

#include <cstdint>

namespace qemu_shared_ivshmem
{

/// @brief Discovers the ivshmem shared-memory PCI device and returns its BAR physical address.
///
/// Locates the ivshmem device (QEMU ivshmem-plain or QNX-Hypervisor `vdev ivshmem`,
/// vendor 0x1af4 / device 0x1110) via pci-server, enables Memory-Space + Bus-Master
/// decoding, and reports the physical address and size of the largest MEM BAR.
///
/// The BAR is NOT mapped here; the physical address is intended for use with a
/// typed-memory provider that binds a shm name to it via @c shm_ctl(SHMCTL_PHYS),
/// allowing @c SharedMemoryFactory to map it through the normal lib-memory path.
///
/// The device handle is kept open for the lifetime of the process (released by the OS on exit).
///
/// @param[out] paddr  Physical base address of the shared-memory BAR.
/// @param[out] size   Size in bytes of the shared-memory BAR.
/// @return @c true on success; @c false if no ivshmem device is found or on any PCI error.
/// @note QNX-only — returns @c false unconditionally on other platforms.
bool DiscoverIvshmemBar(std::uint64_t& paddr, std::uint64_t& size) noexcept;

}  // namespace qemu_shared_ivshmem

#endif  // QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_DISCOVERY_H
