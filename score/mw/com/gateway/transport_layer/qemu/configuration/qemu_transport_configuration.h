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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_CONFIGURATION_QEMU_TRANSPORT_CONFIGURATION_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_CONFIGURATION_QEMU_TRANSPORT_CONFIGURATION_H

#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"

#include <cstdint>

namespace score::mw::com::gateway::qemu
{

class QemuTransportConfiguration
{
  public:
    QemuTransportConfiguration() = default;

    explicit QemuTransportConfiguration(std::uint32_t preferred_ivshmem_bar_num) noexcept
        : preferred_ivshmem_bar_num_{preferred_ivshmem_bar_num}
    {
    }

    std::uint32_t GetPreferredIvshmemBarNum() const noexcept
    {
        return preferred_ivshmem_bar_num_;
    }

  private:
    std::uint32_t preferred_ivshmem_bar_num_{ivshmem::kDefaultIvshmemBarNum};
};

}  // namespace score::mw::com::gateway::qemu

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_CONFIGURATION_QEMU_TRANSPORT_CONFIGURATION_H
