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
#include "score/someip/routing_module/udp_transport/udp_packet_reader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{

/// Stands in for a received mbuf chain: the datagram bytes split into segments.
struct FakeMbufChain
{
    std::vector<std::vector<std::uint8_t>> segments;
    mutable std::size_t copy_calls{0U};
};

}  // namespace

/// Test double of the platform packet copy.
extern "C" void someip_transport_copy_packet(const void* const packet,
                                             const unsigned int offset,
                                             const unsigned int size,
                                             void* const destination)
{
    const auto& chain = *static_cast<const FakeMbufChain*>(packet);
    ++chain.copy_calls;
    const score::cpp::span<std::uint8_t> output{static_cast<std::uint8_t*>(destination), size};
    std::size_t skip{offset};
    std::size_t copied{0U};
    for (const auto& segment : chain.segments)
    {
        if (skip >= segment.size())
        {
            skip -= segment.size();
            continue;
        }
        const std::size_t count = std::min(output.size() - copied, segment.size() - skip);
        std::copy_n(segment.cbegin() + static_cast<std::ptrdiff_t>(skip),
                    count,
                    output.begin() + static_cast<std::ptrdiff_t>(copied));
        copied += count;
        skip = 0U;
    }
}

namespace score::someip::routing_module
{
namespace
{

constexpr unsigned int kPacketSize{20U};

FakeMbufChain MakeChain(const std::size_t segment_size)
{
    FakeMbufChain chain{};
    for (std::size_t start = 0U; start < kPacketSize; start += segment_size)
    {
        std::vector<std::uint8_t> segment{};
        for (std::size_t index = start; index < std::min<std::size_t>(start + segment_size, kPacketSize); ++index)
        {
            segment.push_back(static_cast<std::uint8_t>(0xA0U + index));
        }
        chain.segments.push_back(segment);
    }
    return chain;
}

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

TEST(UdpPacketReaderTest, CopiesARangeAcrossSegmentBoundaries)
{
    Trace("RoutingModule.CR_RoutingModule_DirectReceiveCopy,RoutingModule.CR_RoutingModule_PayloadPreservation",
          "a received 20-byte packet stored in 8-byte mbuf segments",
          "12 bytes starting inside the first segment are copied into a destination",
          "the destination holds exactly those packet bytes, copied by one transport copy");
    const FakeMbufChain chain = MakeChain(8U);
    const UdpPacketReader reader{&chain, kPacketSize};
    std::array<std::uint8_t, 12U> destination{};

    ASSERT_TRUE(reader.Copy(6U, destination));

    EXPECT_EQ(reader.Size(), kPacketSize);
    EXPECT_EQ(chain.copy_calls, 1U);
    for (std::size_t index = 0U; index < destination.size(); ++index)
    {
        EXPECT_EQ(destination.at(index), 0xA0U + 6U + index);
    }
}

TEST(UdpPacketReaderTest, AcceptsRangesEndingAtThePacketEnd)
{
    Trace("RoutingModule.CR_RoutingModule_EmptyPayload",
          "a received 20-byte packet stored in 8-byte mbuf segments",
          "the last four bytes and an empty range at the packet end are copied",
          "both copies succeed and the last four packet bytes are copied");
    const FakeMbufChain chain = MakeChain(8U);
    const UdpPacketReader reader{&chain, kPacketSize};
    std::array<std::uint8_t, 4U> tail{};

    ASSERT_TRUE(reader.Copy(kPacketSize - tail.size(), tail));
    EXPECT_TRUE(reader.Copy(kPacketSize, score::cpp::span<std::uint8_t>{}));

    EXPECT_EQ(tail, (std::array<std::uint8_t, 4U>{0xB0U, 0xB1U, 0xB2U, 0xB3U}));
}

TEST(UdpPacketReaderTest, RejectsRangesOutsideThePacketWithoutReading)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a received 20-byte packet",
          "ranges that start beyond the packet or extend past its end are copied",
          "each copy fails without reading the packet through the transport");
    const FakeMbufChain chain = MakeChain(8U);
    const UdpPacketReader reader{&chain, kPacketSize};
    std::array<std::uint8_t, 3U> destination{};

    EXPECT_FALSE(reader.Copy(kPacketSize + 1U, score::cpp::span<std::uint8_t>{}));
    EXPECT_FALSE(reader.Copy(kPacketSize - 2U, destination));
    EXPECT_FALSE(reader.Copy(std::numeric_limits<std::size_t>::max(), destination));

    EXPECT_EQ(chain.copy_calls, 0U);
}

}  // namespace
}  // namespace score::someip::routing_module
