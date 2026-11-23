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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_PACKET_READER_H
#define SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_PACKET_READER_H

#include "score/someip/routing_module/iosock_transport/iosock_transport.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::someip::routing_module
{

/// Reads a received packet in place through the io-sock module; valid only during someip_routing_datagram().
class IoSockPacketReader final
{
  public:
    IoSockPacketReader(const void* packet, unsigned int size) noexcept;

    [[nodiscard]] std::size_t Size() const noexcept
    {
        return size_;
    }

    /// Returns false without reading if the range does not lie within the packet.
    [[nodiscard]] bool Copy(const std::size_t offset, const score::cpp::span<std::uint8_t> destination) const noexcept
    {
        if ((offset > size_) || (destination.size() > (size_ - offset)))
        {
            return false;
        }
        someip_iosock_copy_packet(packet_,
                                  static_cast<unsigned int>(offset),
                                  static_cast<unsigned int>(destination.size()),
                                  destination.data());
        return true;
    }

  private:
    const void* packet_;
    std::size_t size_;
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_PACKET_READER_H
