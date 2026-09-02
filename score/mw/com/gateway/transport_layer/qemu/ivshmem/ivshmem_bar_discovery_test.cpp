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

#include <gtest/gtest.h>

#include <cstdint>

namespace score::mw::com::gateway::qemu::ivshmem
{

namespace
{

TEST(IvshmemBarDiscoveryTest, DiscoverIvshmemBarReturnsFalseOnNonQnx)
{
    std::uint64_t paddr = 0U;
    std::uint64_t size = 0U;
    const bool result = DiscoverIvshmemBar(paddr, size);
    EXPECT_FALSE(result);
    EXPECT_EQ(paddr, 0U);
    EXPECT_EQ(size, 0U);
}

}  // namespace

}  // namespace score::mw::com::gateway::qemu::ivshmem
