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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_INTERNAL_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_INTERNAL_H

#include <cstdint>

namespace lgsomeip {

// Section: SOMEIP-SD ENTRY INFORMATION (INTERNAL) For Application Management
namespace SOMEIP_SD_ENTRY {

namespace INTERNAL {
const std::uint8_t TYPEID = APPLICATION_MGMT_ENTRY;
const std::uint16_t SOMEIP_SERVICE_ID = 0xFFFF;
const std::uint16_t SOMEIP_INSTANCE_ID = 0xFFFF;

// TTL FIELD VALUE
const std::uint32_t REGISTER = 0xFFFFFF;
const std::uint32_t DEREGISTER = 0x0;
const std::uint32_t CHECKALIVE = 0x1;

namespace POS {
const std::uint32_t TYPE = 0;
const std::uint32_t OPTION1 = 1;
const std::uint32_t OPTION2 = 2;
const std::uint32_t OPTIONNUM = 3;
const std::uint32_t SOMEIP_SERVICE_ID = 4;
const std::uint32_t SOMEIP_INSTANCE_ID = 6;
const std::uint32_t MAJOR = 8;
const std::uint32_t TTL = 9;

const std::uint32_t MINOR = 12;
} // namespace POS
} // namespace INTERNAL

} // namespace SOMEIP_SD_ENTRY

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_INTERNAL_H
