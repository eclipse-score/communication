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
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"

#include "score/mw/log/logging.h"

#include <cerrno>
#include <cstring>
#include <vector>

#if defined(__QNXNTO__)
extern "C" {
#include <pci/pci.h>
}
#include <sys/neutrino.h>  // ThreadCtl, _NTO_TCTL_IO
#endif

namespace score::mw::com::gateway::qemu::ivshmem
{

std::optional<IvshmemBar> SelectIvshmemBar(const std::vector<IvshmemBar>& bars, std::uint32_t required_bar_num) noexcept
{
    for (const auto& bar : bars)
    {
        if (bar.is_memory && bar.bar_num == required_bar_num)
        {
            return bar;
        }
    }
    return std::nullopt;
}

#if defined(__QNXNTO__)

// COV_JUSTIFIED_START ivshmem-bar-discovery-hardware-path
bool DiscoverIvshmemBar(std::uint64_t& paddr, std::uint64_t& size, std::uint32_t required_bar_num) noexcept
{
    constexpr pci_vid_t kVendor = 0x1af4U;
    constexpr pci_did_t kDevice = 0x1110U;

    if (::ThreadCtl(_NTO_TCTL_IO, nullptr) == -1)
    {
        ::score::mw::log::LogError() << "ThreadCtl(_NTO_TCTL_IO) failed: " << std::strerror(errno);
        return false;
    }

    const pci_bdf_t bdf = pci_device_find(0U, kVendor, kDevice, PCI_CCODE_ANY);
    if (bdf == PCI_BDF_NONE)
    {
        ::score::mw::log::LogError() << "ivshmem PCI device " << static_cast<unsigned int>(kVendor) << ":"
                                     << static_cast<unsigned int>(kDevice) << " not found";
        return false;
    }

    pci_err_t err = PCI_ERR_OK;
    const pci_devhdl_t hdl = pci_device_attach(bdf, pci_attachFlags_OWNER, &err);
    if (hdl == nullptr || err != PCI_ERR_OK)
    {
        ::score::mw::log::LogError() << "pci_device_attach failed (err=" << static_cast<int>(err) << ")";
        return false;
    }

    constexpr pci_cmd_t kMemSpaceEnable = static_cast<pci_cmd_t>(1U << 1);
    constexpr pci_cmd_t kBusMasterEnable = static_cast<pci_cmd_t>(1U << 2);
    pci_cmd_t cmd = 0U;
    err = pci_device_read_cmd(bdf, &cmd);
    if (err != PCI_ERR_OK)
    {
        (void)pci_device_detach(hdl);
        return false;
    }
    pci_cmd_t cmd_set = 0U;
    err = pci_device_write_cmd(hdl, static_cast<pci_cmd_t>(cmd | kMemSpaceEnable | kBusMasterEnable), &cmd_set);
    if (err != PCI_ERR_OK)
    {
        (void)pci_device_detach(hdl);
        return false;
    }

    pci_ba_t ba[7];
    int_t nba = static_cast<int_t>(sizeof(ba) / sizeof(ba[0]));
    err = pci_device_read_ba(hdl, &nba, ba, pci_reqType_e_UNSPECIFIED);
    if (err != PCI_ERR_OK)
    {
        (void)pci_device_detach(hdl);
        return false;
    }

    std::vector<IvshmemBar> bars;
    bars.reserve(static_cast<std::size_t>(nba));
    for (int_t i = 0; i < nba; ++i)
    {
        bars.push_back(IvshmemBar{static_cast<std::uint32_t>(ba[i].bar_num),
                                  static_cast<std::uint64_t>(ba[i].addr),
                                  static_cast<std::uint64_t>(ba[i].size),
                                  ba[i].type == pci_asType_e_MEM});
    }
    for (const auto& bar : bars)
    {
        ::score::mw::log::LogInfo() << "ivshmem: BAR" << static_cast<int>(bar.bar_num)
                                    << " type=" << (bar.is_memory ? "MEM" : "IO") << " addr=0x"
                                    << static_cast<std::uint64_t>(bar.addr) << " size=0x"
                                    << static_cast<std::uint64_t>(bar.size);
    }

    const auto shared = SelectIvshmemBar(bars, required_bar_num);
    if (!shared.has_value())
    {
        ::score::mw::log::LogError() << "required ivshmem BAR" << static_cast<unsigned int>(required_bar_num)
                                     << " not found on the ivshmem device";
        (void)pci_device_detach(hdl);
        return false;
    }

    ::score::mw::log::LogInfo() << "ivshmem: BAR" << static_cast<int>(shared->bar_num) << " paddr=0x"
                                << static_cast<std::uint64_t>(shared->addr) << " size=0x"
                                << static_cast<std::uint64_t>(shared->size);

    paddr = static_cast<std::uint64_t>(shared->addr);
    size = static_cast<std::uint64_t>(shared->size);
    return true;
}
// COV_JUSTIFIED_STOP

#else

bool DiscoverIvshmemBar(std::uint64_t& /*paddr*/, std::uint64_t& /*size*/, std::uint32_t /*required_bar_num*/) noexcept
{
    ::score::mw::log::LogError() << "DiscoverIvshmemBar is only supported on QNX";
    return false;
}

#endif

}  // namespace score::mw::com::gateway::qemu::ivshmem
