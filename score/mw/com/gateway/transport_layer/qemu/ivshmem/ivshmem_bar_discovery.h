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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_BAR_DISCOVERY_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_BAR_DISCOVERY_H

#include <cstdint>
#include <optional>
#include <vector>

namespace score::mw::com::gateway::qemu::ivshmem
{

/// Descriptor for one PCI BAR returned by the selector helper.
struct IvshmemBar
{
    std::uint32_t bar_num;
    std::uint64_t addr;
    std::uint64_t size;
    bool is_memory;
};

/// Default BAR number used by QEMU ivshmem-plain for the shared-memory region.
static constexpr std::uint32_t kDefaultIvshmemBarNum = 2U;

/// Selects the ivshmem BAR that matches the required BAR number.
///
/// The selector is intentionally strict: it returns the exact BAR that QEMU exposes for the
/// shared-memory region, rather than guessing by size.
std::optional<IvshmemBar> SelectIvshmemBar(const std::vector<IvshmemBar>& bars,
                                           std::uint32_t required_bar_num = kDefaultIvshmemBarNum) noexcept;

/// \brief Discovers the ivshmem shared-memory PCI device through pci-server.
///
/// Looks up the QEMU ivshmem-plain PCI device (vendor 0x1af4 / device 0x1110), enables
/// Memory-Space and Bus-Master decoding, and returns the physical address and size of the
/// configured shared-memory BAR (BAR2 by default).
///
/// The device remains attached for the lifetime of the process and is released by the OS on
/// exit. This function is supported only on QNX; on other operating systems it returns false.
///
/// \param paddr Receives the physical base address of the discovered shared-memory BAR.
/// \param size Receives the size in bytes of the discovered shared-memory BAR.
/// \param required_bar_num The BAR number to select.
/// \return true if the ivshmem BAR was discovered successfully; otherwise false.
bool DiscoverIvshmemBar(std::uint64_t& paddr,
                        std::uint64_t& size,
                        std::uint32_t required_bar_num = kDefaultIvshmemBarNum) noexcept;

}  // namespace score::mw::com::gateway::qemu::ivshmem

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_BAR_DISCOVERY_H
