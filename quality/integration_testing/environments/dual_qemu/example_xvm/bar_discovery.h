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

// Discovers the ivshmem shared-memory PCI device (QEMU ivshmem-plain, and the
// QNX-Hypervisor `vdev ivshmem`, both present vendor 0x1af4 / device 0x1110) through
// pci-server, enables Memory-Space + Bus-Master decoding, and returns the *physical*
// address and size of its shared-memory BAR (the largest MEM BAR).
//
// Unlike the hand-rolled example, this does NOT map the BAR itself -- it only returns the
// physical address so that lib memory (SharedMemoryFactory) can map it via a typed-memory
// provider that binds a shm name to this physical address (shm_ctl(SHMCTL_PHYS)).
//
// The device is kept attached for the lifetime of the process (released by the OS on exit).
// QNX-only: on any other OS this returns false.
bool DiscoverIvshmemBar(std::uint64_t& paddr, std::uint64_t& size) noexcept;

}  // namespace qemu_shared_ivshmem

#endif  // QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_XVM_BAR_DISCOVERY_H
