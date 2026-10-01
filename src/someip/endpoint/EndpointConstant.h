/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINT_CONSTANT_H
#define LG_SOMEIP_ENDPOINT_ENDPOINT_CONSTANT_H

#include <cstdint>

namespace lgsomeip {

#define SOMEIP_SOCKET_PATH "/tmp/someip/"
#define SOMEIP_SOCKET_PREFIX "/tmp/someip/someip-"

#if defined(ENABLE_QNX_MESSAGE_PASSING)
#define SOMEIP_MESSAGEPASSING_CHANNEL_NAME_PREFIX "someip/someip-"
#endif // ENABLE_QNX_MESSAGE_PASSING

const std::uint8_t kServerMagicCookie[] = {0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00, 0x08,
                                           0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x01, 0x02, 0x00};
const std::uint8_t kClientMagicCookie[] = {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
                                           0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x01, 0x01, 0x00};
const std::size_t kLenMagicCookie = 16;

} // namespace lgsomeip

#endif // LG_SOMEIP_ENDPOINT_ENDPOINT_CONSTANT_H
