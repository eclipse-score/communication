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

#ifndef LG_SOMEIP_APPLICATION_APPLICATION_CONSTANT_H
#define LG_SOMEIP_APPLICATION_APPLICATION_CONSTANT_H

#include <cstdint>

namespace lgsomeip {

// Application Register
const std::uint16_t SOMEIP_APPLICATION_DEREGISTERED = 0x0000;
const std::uint16_t SOMEIP_APPLICATION_REGISTERED = 0x0001;

// Service Information
const std::uint16_t SOMEIP_DEFAULT_ANY_SERVICE = 0xFFFF;
const std::uint16_t SOMEIP_DEFAULT_ANY_INSTANCE = 0xFFFF;
const std::uint16_t SOMEIP_DEFAULT_ANY_METHOD = 0xFFFF;
const std::uint16_t SOMEIP_DEFAULT_ANY_EVENT = 0xFFFF;
const std::uint16_t SOMEIP_DEFAULT_ANY_CLIENT = 0xFFFF;

const std::uint8_t SOMEIP_DEFAULT_ANY_MAJOR = 0xFF;
const std::uint32_t SOMEIP_DEFAULT_ANY_MINOR = 0xFFFFFFFF;
const std::uint8_t SOMEIP_DEFAULT_MAJOR = 0x00;
const std::uint32_t SOMEIP_DEFAULT_MINOR = 0x00000000;

const std::uint32_t SOMEIP_DEFAULT_TTL_ON = 0x00FFFFFF;
const std::uint32_t SOMEIP_DEFAULT_TTL_OFF = 0x0;

// Service Control Information
const std::uint8_t SOMEIP_SERVICE_NOT_INITIALIZED = 0x00;
const std::uint8_t SOMEIP_SERVICE_STOP_OFFER = 0x10;
const std::uint8_t SOMEIP_SERVICE_OFFER = 0x11;

const std::uint8_t SOMEIP_SERVICE_RELEASE = 0x13;
const std::uint8_t SOMEIP_SERVICE_REQUEST = 0x14;
const std::uint8_t SOMEIP_SERVICE_FIND = 0x15;
const std::uint8_t SOMEIP_SERVICE_AVAILABLE = 0x16;

// Service state Information
const std::uint32_t SOMEIP_SERVICE_STATE_INITIAL = 0x00000000;
const std::uint32_t SOMEIP_SERVICE_STATE_REPETITION = 0x00010000;
const std::uint32_t SOMEIP_SERVICE_STATE_MAIN = 0x00020000;

// Service Member(Method) Control Information
const std::uint8_t SOMEIP_METHOD_NOT_INITIALIZED = 0x00;
const std::uint8_t SOMEIP_METHOD_RELEASE = 0x21;
const std::uint8_t SOMEIP_METHOD_REQUEST = 0x22;

// Service Member(Event) Control Information
const std::uint8_t SOMEIP_EVENT_NOT_INITIALIZED = 0x00;
const std::uint8_t SOMEIP_EVENT_STOP_OFFER = 0x30;
const std::uint8_t SOMEIP_EVENT_OFFER = 0x31;
const std::uint8_t SOMEIP_EVENT_UNSUBSCRIBE = 0x32;
const std::uint8_t SOMEIP_EVENT_SUBSCRIBE = 0x33;
const std::uint8_t SOMEIP_EVENT_SUBSCRIBE_ACK = 0x34;
const std::uint8_t SOMEIP_EVENT_SUBSCRIBE_NACK = 0x35;
const std::uint8_t SOMEIP_EVENT_REQUEST = 0x37;
const std::uint8_t SOMEIP_EVENT_RELEASE = 0x38;

const std::uint16_t SOMEIP_EVENT_SUBSCRIBETYPE_DEFAULT = 0x11;
const std::uint16_t SOMEIP_EVENT_SUBSCRIBETYPE_RELIABLE = 0x01;
const std::uint16_t SOMEIP_EVENT_SUBSCRIBETYPE_PRE_RELIABLE = 0x02;
const std::uint16_t SOMEIP_EVENT_SUBSCRIBETYPE_UNRELIABLE = 0x10;
const std::uint16_t SOMEIP_EVENT_SUBSCRIBETYPE_PRE_UNRELIABLE = 0x20;

const std::int32_t SOMEIP_DEFAULT_TIMER_CYCLE = 100;

} // namespace lgsomeip

#endif // LG_SOMEIP_APPLICATION_APPLICATION_CONSTANT_H
