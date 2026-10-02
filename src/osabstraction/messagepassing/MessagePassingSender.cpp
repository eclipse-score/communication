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

#include <libgen.h>
#include <utils/log/logger.h>

#include "MessagePassingSender.h"

namespace lgsomeip {
namespace osabstraction {

constexpr std::uint16_t kMessageData = _IO_MAX + 1;
constexpr std::uint16_t kMessageConnect = _IO_MAX + 2;

union MessagePassingIoHeader {
    std::uint16_t type;
    struct _pulse pulse;
};

MessagePassingSender::MessagePassingSender() : coid_(-1), thread_send_(nullptr), thread_send_run_(false) {}

MessagePassingSender::~MessagePassingSender() {
    disconnect();
}

bool MessagePassingSender::connect(const std::string& server_name) {
    struct _server_info server_info;
    bool result = false;

    std::string name_path = "/dev/name/local/";
    name_path += server_name;

    LGSOMEIP_LOG_DEBUG << "MessagePassingSender::connect() wait for path: " << server_name;

    struct stat stat_buffer;
    if (stat(name_path.c_str(), &stat_buffer) != -1) {
        LGSOMEIP_LOG_DEBUG << "MessagePassingSender::connect() completed checking path: " << server_name;

        coid_ = name_open(server_name.data(), 0);

        if (coid_ != -1) {
            ConnectServerInfo(0, coid_, &server_info);

            if (thread_send_run_ == false) {
                thread_send_run_ = true;
                thread_send_ = new std::thread([this, server_name] {
                    // Assign a descriptive name to this worker thread.
                    pthread_setname_np(pthread_self(), "SomeipMsgPsSnd");

                    send_thread(server_name);
                });

                result = true;
            }
        }
    }

    return result;
}

void MessagePassingSender::disconnect() {
    if (thread_send_run_ != false) {
        thread_send_run_ = false;
        messages_cv_.notify_one();
        thread_send_->join();
        delete thread_send_;
        thread_send_ = nullptr;
    }

    if (coid_ != -1) {
        name_close(coid_);
        coid_ = -1;
    }

    std::lock_guard<std::mutex> lock(mutex_messages_);
    while (messages_.empty() == false) {
        messages_.pop();
    }
}

bool MessagePassingSender::send(const void* buffer, std::size_t size) {
    std::lock_guard<std::mutex> lock(mutex_messages_);

    std::shared_ptr<MessageData> message = std::make_shared<MessageData>(buffer, size);

    messages_.push(message);

    messages_cv_.notify_one();

    return true;
}

void MessagePassingSender::send_thread(const std::string& server_name) {
    MessagePassingIoHeader header;
    iov_t iov[2];

    std::shared_ptr<MessageData> message_data;

    {
        // Before being connected to the receiver of MessagePassing,
        // the sender of MessagePassing should not try to send messages.
        // So, mutex_messages_ is also used to prevent the sender from sending messages.
        std::lock_guard<std::mutex> lock(mutex_messages_);

        SETIOV(&iov[0], &header, sizeof(header.type));

        header.type = kMessageConnect;
        MsgSendv(coid_, iov, 1, NULL, 0);

        LGSOMEIP_LOG_INFO << "MessagePassingSender::send_thread() completed connection: " << server_name;
    }

    while (thread_send_run_ != false) {
        std::unique_lock<std::mutex> lock(mutex_messages_);
        messages_cv_.wait(lock, [this]() { return ((messages_.empty() == false) || (thread_send_run_ == false)); });

        if (thread_send_run_ == false) {
            break;
        }

        message_data = messages_.front();
        messages_.pop();
        lock.unlock();

        SETIOV(&iov[0], &header, sizeof(header.type));
        SETIOV(&iov[1], (void*)(message_data->data_.data()), message_data->size_);

        header.type = kMessageData;
        MsgSendv(coid_, iov, 2, NULL, 0);
    }
}

} // namespace osabstraction
} // namespace lgsomeip
