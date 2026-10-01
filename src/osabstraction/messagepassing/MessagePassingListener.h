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

#ifndef LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGLISTENER_H
#define LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGLISTENER_H

#include <cstdint>

namespace lgsomeip {
namespace osabstraction {

struct MessagePassingConnectionInformation {
    int scoid_;
    pid_t pid_;
};

class MessagePassingConnectionListener {
public:
    virtual void on_connect(const MessagePassingConnectionInformation& info) = 0;

    virtual void on_disconnect(const MessagePassingConnectionInformation& info) = 0;
};

class MessagePassingDataListener {
public:
    virtual void on_message(std::uint8_t* data, std::size_t size) = 0;

    virtual void on_error() = 0;
};

class MessagePassingTimerListener {
public:
    virtual void on_timer(const std::int32_t id) = 0;
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGLISTENER_H
