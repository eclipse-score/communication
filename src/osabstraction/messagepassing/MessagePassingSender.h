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

#ifndef LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSENDER_H
#define LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSENDER_H

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
#include <condition_variable>
#include <queue>
#include <string>

namespace lgsomeip {
namespace osabstraction {

class MessageData {
public:
    MessageData(const void* data, std::size_t size) {
        std::uint8_t* data_bytes = (std::uint8_t*)(data);
        data_.assign(data_bytes, data_bytes + size);
        size_ = size;
    }

    ~MessageData() {}

public:
    std::vector<std::uint8_t> data_;
    std::size_t size_;
};

class MessagePassingSender {
public:
    MessagePassingSender();
    ~MessagePassingSender();

    bool connect(const std::string& server_name);

    void disconnect();

    bool send(const void* buffer, std::size_t size);

private:
    void send_thread(const std::string& server_name);

private:
    int coid_;

    std::condition_variable messages_cv_;
    std::queue<std::shared_ptr<MessageData>> messages_;
    std::mutex mutex_messages_;

    std::thread* thread_send_;
    std::atomic_bool thread_send_run_;
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_MESSAGEPASSING_MESSAGEPASSINGSENDER_H
