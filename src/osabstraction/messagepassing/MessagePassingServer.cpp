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

#include <utils/log/logger.h>
#include <sched.h>
#include <exception/Exception.h>

#include "MessagePassingServer.h"

#include <utils/time/TimerManager.h>

namespace lgsomeip {
namespace osabstraction {

constexpr std::uint16_t kMessageData = _IO_MAX + 1;
constexpr std::uint16_t kMessageConnect = _IO_MAX + 2;

constexpr int kPulseCodeConnect = _PULSE_CODE_MINAVAIL + 1;
constexpr int kPulseCodeWakeup = _PULSE_CODE_MINAVAIL + 2;
constexpr int kPulseCodeTimer = _PULSE_CODE_MINAVAIL + 3;
constexpr int kPulseCodeDisconnect = _PULSE_CODE_DISCONNECT;

union MessagePassingIoHeader {
    std::uint16_t type;
    struct _pulse pulse;
};

MessagePassingServer::MessagePassingServer(const std::string& name)
    : attached_name_(nullptr), listener_(nullptr), thread_receive_(nullptr), thread_receive_run_(false) {
    attached_name_ = name_attach(NULL, name.data(), 0);
    if (attached_name_ == nullptr) {
        throw LSAR_RUNTIME_ERROR("MessagePassingServer::name_attach failed");
    }

    self_coid_ = ConnectAttach(0, 0, attached_name_->chid, _NTO_SIDE_CHANNEL, 0);
    if (self_coid_ == -1) {
        name_detach(attached_name_, 0);
        attached_name_ = nullptr;
        throw LSAR_RUNTIME_ERROR("MessagePassingServer::ConnectAttach failed");
    }
}

MessagePassingServer::~MessagePassingServer() {
    stop();
    if (self_coid_ != -1) {
        ConnectDetach(self_coid_);
        self_coid_ = -1;
    }
    if (attached_name_ != nullptr) {
        name_detach(attached_name_, 0);
        attached_name_ = nullptr;
    }
}

void MessagePassingServer::run(MessagePassingConnectionListener* listener) {
    if (thread_receive_run_ == false) {
        listener_ = listener;

        thread_receive_run_ = true;
        thread_receive_ = new std::thread([this]() {
            // For assigning thread name
            pthread_setname_np(pthread_self(), "SomeipMsgPsRcv");

            receive_thread();
        });
    }
}

void MessagePassingServer::stop() {
    if (thread_receive_run_ != false) {
        thread_receive_run_ = false;
        if (self_coid_ != -1) {
            MsgSendPulse(self_coid_, 0, kPulseCodeWakeup, 0);
        }
        thread_receive_->join();
        delete thread_receive_;
        thread_receive_ = nullptr;

        listener_ = nullptr;
    }
}

bool MessagePassingServer::set_data_listener(const MessagePassingConnectionInformation& info,
                                             MessagePassingDataListener* listener) {
    std::lock_guard<std::mutex> lock(mutex_listeners_);

    bool result = false;

    auto iterator = listeners_.find(info.scoid_);
    if (listener == nullptr) {
        if (iterator != listeners_.end()) {
            listeners_.erase(iterator);
        }
    } else {
        if (iterator == listeners_.end()) {
            listeners_.insert({info.scoid_, listener});
            result = true;
        }
    }

    return result;
}

void MessagePassingServer::set_timer(const std::int32_t id, const std::uint32_t interval_milliseconds,
                                     const bool periodic, MessagePassingTimerListener* listener) {
    LGSOMEIP_LOG_DEBUG << "MessagePassingServer::set_timer id = " << (int)id
                       << " interval = " << int(interval_milliseconds) << " periodic "
                       << (periodic ? "PERIODIC" : "ONESHOT");
    std::lock_guard<std::recursive_mutex> lock(mutex_timers_);

    struct sched_param param;
    struct sigevent event;
    timer_t timer;
    struct itimerspec itime;

    auto iterator = timers_.find(id);
    if (iterator == timers_.end()) {
        SchedGet(0, 0, &param);
        SIGEV_PULSE_INIT(&event, self_coid_, param.sched_priority, kPulseCodeTimer, id);

        timer_create(CLOCK_REALTIME, &event, &timer);

        itime.it_value.tv_sec = interval_milliseconds / 1000;
        itime.it_value.tv_nsec = (interval_milliseconds % 1000) * 1000 * 1000;

        if (periodic == false) {
            itime.it_interval.tv_sec = 0;
            itime.it_interval.tv_nsec = 0;
        } else {
            itime.it_interval.tv_sec = interval_milliseconds / 1000;
            itime.it_interval.tv_nsec = (interval_milliseconds % 1000) * 1000 * 1000;
        }

        MessagePassingTimer message_passing_timer;

        message_passing_timer.timer_ = timer;
        message_passing_timer.listener_ = listener;

        timers_[id] = message_passing_timer;

        timer_settime(timer, 0, &itime, NULL);
    }
}

void MessagePassingServer::kill_timer(const std::int32_t id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_timers_);

    auto iterator = timers_.find(id);
    if (iterator != timers_.end()) {
        MessagePassingTimer& message_passing_timer = iterator->second;
        timer_delete(message_passing_timer.timer_);

        timers_.erase(iterator);
    }
}

void MessagePassingServer::receive_thread() {
    MessagePassingIoHeader header;

    int receive_id;
    struct _msg_info message_info;

    while (thread_receive_run_ != false) {
        receive_id = MsgReceive(attached_name_->chid, &header, sizeof(header), &message_info);

        if (receive_id == 0) {
            switch (header.pulse.code) {
            case kPulseCodeConnect: {
                if (listener_ != nullptr) {
                    LGSOMEIP_LOG_INFO << "new connection id : " << header.pulse.scoid;

                    MessagePassingConnectionInformation info{header.pulse.scoid, message_info.pid};
                    listener_->on_connect(info);
                }

                break;
            }
            case kPulseCodeDisconnect: {
                LGSOMEIP_LOG_INFO << "delete connection id : " << header.pulse.scoid;

                MessagePassingConnectionInformation info{header.pulse.scoid, message_info.pid};
                listener_->on_disconnect(info);
                ConnectDetach(header.pulse.scoid);

                break;
            }
            case kPulseCodeTimer: {
                std::lock_guard<std::recursive_mutex> lock(mutex_timers_);

                auto iterator = timers_.find(header.pulse.value.sival_int);
                if (iterator != timers_.end()) {
                    MessagePassingTimer& message_passing_timer = iterator->second;
                    if (message_passing_timer.listener_ != nullptr) {
                        message_passing_timer.listener_->on_timer(header.pulse.value.sival_int);
                    }
                }

                break;
            }
            default:
                break;
            }
        } else if (0 < receive_id) {
            switch (header.type) {
            case kMessageConnect: {
                if (listener_ != nullptr) {
                    LGSOMEIP_LOG_INFO << "new connection id : " << message_info.scoid;

                    MessagePassingConnectionInformation info{message_info.scoid, message_info.pid};
                    listener_->on_connect(info);
                }

                MsgReply(receive_id, EOK, NULL, 0);

                break;
            }
            case kMessageData: {
                std::vector<std::uint8_t> receive_buffer_dynamic;
                std::uint32_t message_length = (std::uint32_t)(message_info.srcmsglen - sizeof(header.type));

                // Static allocatin for receiving the packet will be used to improve the performance
                // if the length of the packet is shorter than SOMEIP_UDP_MAX_PAYLOAD_SIZE (about 1400 bytes).
                receive_buffer_ = static_buffer_;

                // If the length of a packet is larger than SOMEIP_UDP_MAX_PAYLOAD_SIZE,
                // dynamic allocation for receiving the packet should be used.
                // The length of dynamic allocation will be SOMEIP_UDP_MAX_PAYLOAD_SIZE + 2.
                // 2 bytes will be used for storing the instance ID.
                if (message_length + 2 > SOMEIP_UDP_MAX_PAYLOAD_SIZE) {
                    receive_buffer_dynamic.assign(message_length + 2, 0x00);
                    receive_buffer_ = receive_buffer_dynamic.data();
                }

                MsgRead(receive_id, receive_buffer_, message_length, sizeof(header.type));
                MsgReply(receive_id, EOK, NULL, 0);

                std::lock_guard<std::mutex> lock(mutex_listeners_);
                auto iterator = listeners_.find(message_info.scoid);
                if (iterator != listeners_.end()) {
                    iterator->second->on_message(receive_buffer_, message_length);
                } else {
                    LGSOMEIP_LOG_WARN << "not found listener id : " << message_info.scoid;
                }

                break;
            }
            default:
                break;
            }
        } else {
        }

        if (thread_receive_run_) {
            TimerManager::get().iterate_loop();
        }
    }
}

} // namespace osabstraction
} // namespace lgsomeip
