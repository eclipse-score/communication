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

// The cross-VM shared-memory DATA-PLANE example.
//
//   qemu_shared_ivshmem --id A    // VM-A: writes its own list, then reads + verifies VM-B's
//   qemu_shared_ivshmem --id B    // VM-B: writes its own list, then reads + verifies VM-A's
//
// By default each VM does BOTH in a single run: it writes an offset-pointer linked list
// into its own slot and then waits for the peer's slot to appear and verifies it. The two
// VMs therefore rendezvous, so they must run concurrently (each one waits for the other).
// You can still split the phases with --write / --read if you prefer to run them by hand:
//
//   qemu_shared_ivshmem --id A --write    // only write slot A
//   qemu_shared_ivshmem --id B --read     // only read + verify slot A (peer of B)
//
// Both VMs map the SAME shared region (the QEMU ivshmem-plain PCI device that both VMs
// back with one host file) but, crucially, at DIFFERENT virtual base addresses. The shared
// data structure and the write/read/verify logic live in offset_list.h; this file is just
// the QNX entry point. On QNX the program talks to pci-server, finds the ivshmem device
// (vendor 0x1af4, device 0x1110), and maps its shared-memory BAR directly. No /dev/ivshmem0
// driver is required -- QNX SDP ships no ivshmem resmgr, so the BAR is mapped in-process via
// pci-server + mmap_device_memory(). This is QNX-only.

#include "quality/integration_testing/environments/dual_qemu/example/offset_list.h"

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#if defined(__QNXNTO__)
// The QNX PCI server header declares plain C functions but does not guard them with
// extern "C", so wrap it to avoid C++ name mangling when linking against libpci.
extern "C" {
#include <pci/pci.h>
}
#include <sys/neutrino.h>  // ThreadCtl, _NTO_TCTL_IO
#endif

namespace
{
using qemu_shared_ivshmem::ReadPeerSlot;
using qemu_shared_ivshmem::Region;
using qemu_shared_ivshmem::WriteOwnSlot;

// Process exit codes (0 == OK).
constexpr int kExitMapFailed = 1;       // could not map the shared region
constexpr int kExitBadArgs = 2;         // missing/invalid command line
constexpr int kExitRegionTooSmall = 5;  // mapped region cannot hold a Region

// A mapped view of the shared region plus the bookkeeping needed to release it.
struct Mapping
{
    void* addr = nullptr;
    std::size_t size = 0U;
};

void Unmap(Mapping& m)
{
    if (m.addr == nullptr)
    {
        return;
    }
    ::munmap_device_memory(m.addr, m.size);
    m = Mapping{};
}

// Finds the QEMU ivshmem-plain device through pci-server and maps its shared-memory BAR.
bool MapFromPci(Mapping& out)
{
    constexpr pci_vid_t kVendor = 0x1af4U;  // Red Hat / virtio vendor used by ivshmem
    constexpr pci_did_t kDevice = 0x1110U;  // ivshmem device id

    // Reading/writing PCI configuration space from a user process is a privileged operation
    // on QNX. Without I/O privilege the in-process config access faults ("Memory fault").
    // ThreadCtl(_NTO_TCTL_IO) requests it; it succeeds because the example runs as root
    // (which holds PROCMGR_AID_IO).
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
    // normally turn on the device. pci-server assigns the BAR addresses but leaves memory
    // decoding off, so enable Memory Space (and Bus Master) ourselves -- otherwise the CPU
    // accesses to the mapped BAR are not decoded and fault ("Memory fault").
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

    // One concise line describing the region we are about to map.
    std::fprintf(stderr,
                 "ivshmem: BAR%d addr=0x%llx size=0x%llx\n",
                 static_cast<int>(shared->bar_num),
                 static_cast<unsigned long long>(shared->addr),
                 static_cast<unsigned long long>(shared->size));

    void* const addr = ::mmap_device_memory(nullptr, shared->size, PROT_READ | PROT_WRITE, 0, shared->addr);
    if (addr == MAP_FAILED)
    {
        std::perror("mmap_device_memory");
        (void)pci_device_detach(hdl);
        return false;
    }

    // Keep the device attached for the lifetime of the process; the OS releases it on exit
    // and the mapping remains valid until Unmap().
    out.addr = addr;
    out.size = static_cast<std::size_t>(shared->size);
    return true;
}

// Command-line options.
struct Options
{
    int id = -1;  // 0 = VM-A, 1 = VM-B
    bool want_write = false;
    bool want_read = false;
};

// Parses argv into opts. Returns false on error or a missing/invalid --id.
bool ParseArgs(int argc, char** argv, Options& opts)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--write")
        {
            opts.want_write = true;
        }
        else if (arg == "--read")
        {
            opts.want_read = true;
        }
        else if (arg == "--id" && (i + 1) < argc)
        {
            const std::string v = argv[++i];
            opts.id = (v == "A" || v == "a") ? 0 : ((v == "B" || v == "b") ? 1 : -1);
        }
        else
        {
            return false;
        }
    }
    return opts.id >= 0;
}
}  // namespace

int main(int argc, char** argv)
{
    Options opts;
    if (!ParseArgs(argc, argv, opts))
    {
        std::fprintf(stderr, "usage: %s --id <A|B> [--write] [--read]\n", argv[0]);
        return kExitBadArgs;
    }

    // Default: do BOTH in one run -- write our own slot, then read + verify the peer's.
    if (!opts.want_write && !opts.want_read)
    {
        opts.want_write = true;
        opts.want_read = true;
    }

    Mapping mapping;
    if (!MapFromPci(mapping))
    {
        return kExitMapFailed;
    }
    if (mapping.size < sizeof(Region))
    {
        std::fprintf(stderr, "shared region too small: %zu < %zu\n", mapping.size, sizeof(Region));
        Unmap(mapping);
        return kExitRegionTooSmall;
    }

    auto& region = *static_cast<Region*>(mapping.addr);
    const char* const name = (opts.id == 0) ? "VM-A" : "VM-B";

    int result = 0;
    if (opts.want_write)
    {
        WriteOwnSlot(region, opts.id, name, mapping.addr);
    }
    if (opts.want_read)
    {
        result = ReadPeerSlot(region, opts.id, name, mapping.addr);
    }

    Unmap(mapping);
    return result;
}
