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

#ifndef LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSERVER_H
#define LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSERVER_H

#include <cstdint>
#include <atomic>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <thread>
#include <map>
#include <queue>
#include <string>

#include "MessagePassingListener.h"

namespace lgsomeip {
namespace osabstraction {

struct MessagePassingTimer {
    timer_t timer_;
    MessagePassingTimerListener* listener_;
};

class MessagePassingServer {
public:
    MessagePassingServer(const std::string& name);
    ~MessagePassingServer();

    void run(MessagePassingConnectionListener* listener);
    void stop();

    bool set_data_listener(const MessagePassingConnectionInformation& info, MessagePassingDataListener* listener);

    void set_timer(const std::int32_t id, const std::uint32_t interval_milliseconds, const bool periodic,
                   MessagePassingTimerListener* listener);

    void kill_timer(const std::int32_t id);

private:
    void receive_thread();

private:
    name_attach_t* attached_name_;
    int self_coid_{-1};

    uint8_t static_buffer_[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    uint8_t* receive_buffer_;

    MessagePassingConnectionListener* listener_;

    std::thread* thread_receive_;
    std::atomic_bool thread_receive_run_;

    std::map<int, MessagePassingDataListener*> listeners_;
    std::mutex mutex_listeners_;

    std::map<std::int32_t, MessagePassingTimer> timers_;
    std::recursive_mutex mutex_timers_;
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSERVER_H
