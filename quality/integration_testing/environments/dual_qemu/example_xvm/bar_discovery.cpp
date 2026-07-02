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
#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_discovery.h"

#include <cstdio>

#if defined(__QNXNTO__)
// The QNX PCI server header declares plain C functions but does not guard them with
// extern "C", so wrap it to avoid C++ name mangling when linking against libpci.
extern "C" {
#include <pci/pci.h>
}
#include <sys/neutrino.h>  // ThreadCtl, _NTO_TCTL_IO
#endif

namespace qemu_shared_ivshmem
{

#if defined(__QNXNTO__)

bool DiscoverIvshmemBar(std::uint64_t& paddr, std::uint64_t& size) noexcept
{
    constexpr pci_vid_t kVendor = 0x1af4U;  // Red Hat / virtio vendor used by ivshmem
    constexpr pci_did_t kDevice = 0x1110U;  // ivshmem device id

    // Reading/writing PCI configuration space from a user process is privileged on QNX.
    // ThreadCtl(_NTO_TCTL_IO) requests I/O privilege; it succeeds when running as root.
    if (::ThreadCtl(_NTO_TCTL_IO, nullptr) == -1)
    {
        std::perror("ThreadCtl(_NTO_TCTL_IO)");
        return false;
    }

    const pci_bdf_t bdf = pci_device_find(0U, kVendor, kDevice, PCI_CCODE_ANY);
    if (bdf == PCI_BDF_NONE)
    {
        std::fprintf(stderr, "ivshmem PCI device %04x:%04x not found (is pci-server running?)\n", kVendor, kDevice);
        return false;
    }

    pci_err_t err = PCI_ERR_OK;
    const pci_devhdl_t hdl = pci_device_attach(bdf, pci_attachFlags_OWNER, &err);
    if (hdl == nullptr || err != PCI_ERR_OK)
    {
        std::fprintf(stderr, "pci_device_attach failed (err=%d)\n", static_cast<int>(err));
        return false;
    }

    // Booting the QNX IFS directly with `-kernel` skips the BIOS PCI enumeration that would
    // normally turn the device on, so enable Memory Space + Bus Master ourselves. Without
    // it, CPU accesses to the physical BAR are not decoded and fault ("Memory fault").
    constexpr pci_cmd_t kMemSpaceEnable = static_cast<pci_cmd_t>(1U << 1);
    constexpr pci_cmd_t kBusMasterEnable = static_cast<pci_cmd_t>(1U << 2);
    pci_cmd_t cmd = 0U;
    err = pci_device_read_cmd(bdf, &cmd);
    if (err != PCI_ERR_OK)
    {
        std::fprintf(stderr, "pci_device_read_cmd failed (err=%d)\n", static_cast<int>(err));
        (void)pci_device_detach(hdl);
        return false;
    }
    pci_cmd_t cmd_set = 0U;
    err = pci_device_write_cmd(hdl, static_cast<pci_cmd_t>(cmd | kMemSpaceEnable | kBusMasterEnable), &cmd_set);
    if (err != PCI_ERR_OK)
    {
        std::fprintf(stderr, "pci_device_write_cmd failed (err=%d)\n", static_cast<int>(err));
        (void)pci_device_detach(hdl);
        return false;
    }

    pci_ba_t ba[7];
    int_t nba = static_cast<int_t>(sizeof(ba) / sizeof(ba[0]));
    err = pci_device_read_ba(hdl, &nba, ba, pci_reqType_e_UNSPECIFIED);
    if (err != PCI_ERR_OK)
    {
        std::fprintf(stderr, "pci_device_read_ba failed (err=%d)\n", static_cast<int>(err));
        (void)pci_device_detach(hdl);
        return false;
    }

    // The shared-memory BAR is the largest memory region (BAR0 holds the small register
    // block, the shared RAM lives in BAR2).
    const pci_ba_t* shared = nullptr;
    for (int_t i = 0; i < nba; ++i)
    {
        if (ba[i].type == pci_asType_e_MEM && (shared == nullptr || ba[i].size > shared->size))
        {
            shared = &ba[i];
        }
    }
    if (shared == nullptr)
    {
        std::fprintf(stderr, "no memory BAR found on the ivshmem device\n");
        (void)pci_device_detach(hdl);
        return false;
    }

    std::fprintf(stderr,
                 "ivshmem: BAR%d paddr=0x%llx size=0x%llx\n",
                 static_cast<int>(shared->bar_num),
                 static_cast<unsigned long long>(shared->addr),
                 static_cast<unsigned long long>(shared->size));

    // Keep the device attached for the process lifetime; the OS releases it on exit.
    paddr = static_cast<std::uint64_t>(shared->addr);
    size = static_cast<std::uint64_t>(shared->size);
    return true;
}

#else  // !__QNXNTO__

bool DiscoverIvshmemBar(std::uint64_t& /*paddr*/, std::uint64_t& /*size*/) noexcept
{
    std::fprintf(stderr, "DiscoverIvshmemBar is only supported on QNX\n");
    return false;
}

#endif  // __QNXNTO__

}  // namespace qemu_shared_ivshmem
