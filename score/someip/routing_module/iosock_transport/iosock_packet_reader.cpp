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
#include "score/someip/routing_module/iosock_transport/iosock_packet_reader.h"

namespace score::someip::routing_module
{

IoSockPacketReader::IoSockPacketReader(const void* const packet, const unsigned int size) noexcept
    : packet_{packet}, size_{size}
{
}

}  // namespace score::someip::routing_module
