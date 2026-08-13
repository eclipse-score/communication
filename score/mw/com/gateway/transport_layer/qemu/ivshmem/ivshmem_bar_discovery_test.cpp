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
#include <vector>

namespace score::mw::com::gateway::qemu::ivshmem
{

namespace
{

TEST(IvshmemBarDiscoveryTest, SelectIvshmemBarReturnsRequiredBar)
{
    const std::vector<IvshmemBar> bars{
        IvshmemBar{0U, 0x1000U, 0x2000U, true},
        IvshmemBar{2U, 0x3000U, 0x4000U, true},
        IvshmemBar{3U, 0x5000U, 0x8000U, true},
    };

    const auto selected = SelectIvshmemBar(bars);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->bar_num, kDefaultIvshmemBarNum);
    EXPECT_EQ(selected->addr, 0x3000U);
    EXPECT_EQ(selected->size, 0x4000U);
}

TEST(IvshmemBarDiscoveryTest, SelectIvshmemBarReturnsCustomRequiredBar)
{
    const std::vector<IvshmemBar> bars{
        IvshmemBar{0U, 0x1000U, 0x2000U, true},
        IvshmemBar{2U, 0x3000U, 0x4000U, true},
        IvshmemBar{3U, 0x5000U, 0x8000U, true},
    };

    const auto selected = SelectIvshmemBar(bars, 3U);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->bar_num, 3U);
    EXPECT_EQ(selected->addr, 0x5000U);
    EXPECT_EQ(selected->size, 0x8000U);
}

TEST(IvshmemBarDiscoveryTest, SelectIvshmemBarReturnsNulloptWhenRequiredBarMissing)
{
    const std::vector<IvshmemBar> bars{
        IvshmemBar{0U, 0x1000U, 0x2000U, true},
        IvshmemBar{3U, 0x5000U, 0x8000U, true},
    };

    const auto selected = SelectIvshmemBar(bars);
    EXPECT_FALSE(selected.has_value());
}

TEST(IvshmemBarDiscoveryTest, SelectIvshmemBarIgnoresIoBars)
{
    // Given bars where the BAR at kDefaultIvshmemBarNum exists but is I/O-space, not memory-space.
    // An I/O BAR cannot be mmap'd for shared memory, so SelectIvshmemBar must reject it even
    // though the bar_num matches.
    const std::vector<IvshmemBar> bars{
        IvshmemBar{kDefaultIvshmemBarNum, 0x1000U, 0x2000U, /*is_memory=*/false},
    };

    const auto selected = SelectIvshmemBar(bars);
    EXPECT_FALSE(selected.has_value());
}

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
