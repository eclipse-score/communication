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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_H

#include <cstdint>
#include <message/MessageConstantInternal.h>

namespace lgsomeip {

// -----------------------------------------------------------------------------
// SOMEIP HEADER INFORMATION
// -----------------------------------------------------------------------------
namespace SOMEIP_HEADER {
const std::uint32_t SIZE = 16;
namespace POS {
const std::uint32_t MESSAGEID = 0;
const std::uint32_t LENGTH = 4;
const std::uint32_t REQUESTID = 8;
const std::uint32_t PROTOCOL = 12;
const std::uint32_t INTERFACE = 13;
const std::uint32_t MESSAGETYPE = 14;
const std::uint32_t RETURNCODE = 15;
} // namespace POS

namespace LEN {
const std::uint32_t MESSAGEID = 4;
const std::uint32_t LENGTH = 4;
const std::uint32_t REQUESTID = 4;
const std::uint32_t PROTOCOL = 1;
const std::uint32_t INTERFACE = 1;
const std::uint32_t MESSAGETYPE = 1;
const std::uint32_t RETURNCODE = 1;
} // namespace LEN

namespace ACCUM_LEN {
const std::uint32_t MESSAGEID = 4;
const std::uint32_t LENGTH = 8;
const std::uint32_t REQUESTID = 12;
const std::uint32_t PROTOCOL = 13;
const std::uint32_t INTERFACE = 14;
const std::uint32_t MESSAGETYPE = 15;
const std::uint32_t RETURNCODE = 16;
} // namespace ACCUM_LEN
} // namespace SOMEIP_HEADER

namespace SOMEIP_MESSAGE_TYPE {
const std::uint8_t REQUEST = 0x00;
const std::uint8_t REQUEST_NO_RETURN = 0x01;
const std::uint8_t NOTIFICATION = 0x02;
const std::uint8_t RESPONSE = 0x80;
const std::uint8_t ERROR = 0x81;
const std::uint8_t TP_REQUEST = 0x20;
const std::uint8_t TP_REQUEST_NO_RETURN = 0x21;
const std::uint8_t TP_NOTIFICATION = 0x22;
const std::uint8_t TP_RESPONSE = 0xa0;
const std::uint8_t TP_ERROR = 0xa1;
} // namespace SOMEIP_MESSAGE_TYPE

//  Return Code(4.2.6.1)
namespace SOMEIP_RETURN_CODE {
const std::uint8_t E_OK = 0x00;
const std::uint8_t E_NOT_OK = 0x01;
const std::uint8_t E_UNKNOWN_SERVICE = 0x02;
const std::uint8_t E_UNKNOWN_METHOD = 0x03;
const std::uint8_t E_NOT_READY = 0x04;
const std::uint8_t E_NOT_REACHABLE = 0x05;
const std::uint8_t E_TIMEOUT = 0x06;
const std::uint8_t E_WRONG_PROTOCOL_VERSION = 0x07;
const std::uint8_t E_WRONG_INTERFACE_VERSION = 0x08;
const std::uint8_t E_MALFORMED_MESSAGE = 0x09;
const std::uint8_t E_WRONG_MESSAGE_TYPE = 0x0A;
const std::uint8_t E_E2E_REPEATED = 0x0B;
const std::uint8_t E_E2E_WRONG_SEQUENCE = 0x0C;
const std::uint8_t E_E2E = 0x0D;
const std::uint8_t E_E2E_NOT_AVAILABLE = 0x0E;
const std::uint8_t E_E2E_NO_NEW_DATA = 0x0F;
} // namespace SOMEIP_RETURN_CODE

// -----------------------------------------------------------------------------
// SOMEIP-SD HEADER INFORMATION (4BYTE)
// -----------------------------------------------------------------------------
namespace SOMEIP_SD_HEADER {
const std::uint32_t SIZE = 4;
namespace POS {
const std::uint32_t FLAGS = 0;
}
} // namespace SOMEIP_SD_HEADER

// -----------------------------------------------------------------------------
// SOMEIP-SD ENTRY INFORMATION
// -----------------------------------------------------------------------------
namespace SOMEIP_SD_ENTRY {
const std::uint32_t SIZE = 16;

namespace FINDSERVICE {
const std::uint8_t TYPEID = 0x00;
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
} // namespace FINDSERVICE

namespace OFFERSERVICE {
const std::uint8_t TYPEID = 0x01;
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
} // namespace OFFERSERVICE

namespace SUBSCRIBE {
const std::uint8_t TYPEID = 0x06;
namespace POS {
const std::uint32_t TYPE = 0;
const std::uint32_t OPTION1 = 1;
const std::uint32_t OPTION2 = 2;
const std::uint32_t OPTIONNUM = 3;
const std::uint32_t SOMEIP_SERVICE_ID = 4;
const std::uint32_t SOMEIP_INSTANCE_ID = 6;
const std::uint32_t MAJOR = 8;
const std::uint32_t TTL = 9;

const std::uint32_t FLAGS = 13;
const std::uint32_t EVENTGROUPID = 14;
} // namespace POS
} // namespace SUBSCRIBE

namespace SUBSCRIBEACK {
const std::uint8_t TYPEID = 0x07;
namespace POS {
const std::uint32_t TYPE = 0;
const std::uint32_t OPTION1 = 1;
const std::uint32_t OPTION2 = 2;
const std::uint32_t OPTIONNUM = 3;
const std::uint32_t SOMEIP_SERVICE_ID = 4;
const std::uint32_t SOMEIP_INSTANCE_ID = 6;
const std::uint32_t MAJOR = 8;
const std::uint32_t TTL = 9;

const std::uint32_t FLAGS = 13;
const std::uint32_t EVENTGROUPID = 14;
} // namespace POS
} // namespace SUBSCRIBEACK
} // namespace SOMEIP_SD_ENTRY

// -----------------------------------------------------------------------------
// SOMEIP-SD OPTION INFORMATION
// -----------------------------------------------------------------------------
namespace SOMEIP_SD_OPTION {
namespace HEADER {
const std::uint32_t SIZE = 3;
namespace POS {
const std::uint32_t LENGTH = 0;
const std::uint32_t TYPE = 2;
} // namespace POS
} // namespace HEADER

namespace CONFIGURATION {
const std::uint32_t SIZE = 0xFFFFFFFF;
const std::uint16_t TYPEID = 0x01;
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x10
const std::uint32_t TYPE = 2;
const std::uint32_t OPTIONS = 4;
} // namespace POS
} // namespace CONFIGURATION

namespace LOADBALANCING {
const std::uint32_t SIZE = 8;
const std::uint16_t TYPEID = 0x02;
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x05
const std::uint32_t TYPE = 2;
const std::uint32_t PRIORITY = 4;
const std::uint32_t WEIGHT = 6;
} // namespace POS
} // namespace LOADBALANCING

namespace IP4 {
const std::uint32_t SIZE = 12;
const std::uint16_t TYPEID = 0x04; // IPv4 UNICAST 0x04
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x09
const std::uint32_t TYPE = 2;
const std::uint32_t ADDRESS = 4;
const std::uint32_t PROTOCOL = 9;
const std::uint32_t PORT = 10;
} // namespace POS
} // namespace IP4

namespace IP4MULTI {
const std::uint32_t SIZE = 12;
const std::uint16_t TYPEID = 0x14; // IPv4 MULTICAST 0x14
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x09
const std::uint32_t TYPE = 2;
const std::uint32_t ADDRESS = 4;
const std::uint32_t PROTOCOL = 9;
const std::uint32_t PORT = 10;
} // namespace POS
} // namespace IP4MULTI

namespace IP6 {
const std::uint32_t SIZE = 24;
const std::uint16_t TYPEID = 0x06; // IPv6 UNICAST 0x06
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x15
const std::uint32_t TYPE = 2;
const std::uint32_t ADDRESS = 4;
const std::uint32_t PROTOCOL = 21;
const std::uint32_t PORT = 22;
} // namespace POS
} // namespace IP6

namespace IP6MULTI {
const std::uint32_t SIZE = 24;
const std::uint16_t TYPEID = 0x16; // IPv6 MULTICAST 0x16
namespace POS {
const std::uint32_t LENGTH = 0; // STATIC VALUE = 0x15
const std::uint32_t TYPE = 2;
const std::uint32_t ADDRESS = 4;
const std::uint32_t PROTOCOL = 21;
const std::uint32_t PORT = 22;
} // namespace POS
} // namespace IP6MULTI
} // namespace SOMEIP_SD_OPTION

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGECONSTANT_H
