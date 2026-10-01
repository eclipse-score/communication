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

#ifndef LG_SOMEIP_EVENT_MANAGER_H
#define LG_SOMEIP_EVENT_MANAGER_H

#include <cstdint>

#include <set>
#include <memory>
#include <vector>
#include <thread>
#include <condition_variable>
#include <functional>
#include <config/Configuration.h>
#include <message/Message.h>
#include <runtime/ApplicationConstant.h>

namespace lgsomeip {

class ApplicationManager;
class OfferedEvent;

enum class EventStrategy { CyclicUpdate = 1, UpdateOnChange = 2, EpsilonChange = 3 };

class EventManager {
public:
    EventManager(ApplicationManager* host);
    ~EventManager();

    void start();
    void stop();
    void join();

private:
    void run();
    ApplicationManager* host_;

    std::shared_ptr<std::thread> thread_{nullptr};
    bool running_ = false;
    std::condition_variable running_condition_;
    std::condition_variable condition_;
    std::mutex thread_mutex_;

public:
    void add_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                   std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR, bool is_field = false,
                   std::uint32_t cycle = 0, epsilon_change_func_t epsilon_change_function = nullptr);
    void remove_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id);

    void set_service_enabled(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                             bool state);

    void notify(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                std::shared_ptr<Payload> payload, std::uint16_t client = 0, bool force = false, bool flush = false);
    void notify_initial_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id);

private:
    std::vector<std::shared_ptr<OfferedEvent>> offered_events_;
    std::uint32_t base_interval_{50};
};

class OfferedEvent {
    friend class EventManager;

public:
    OfferedEvent(ApplicationManager* host, std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                 std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR, bool is_field = false, std::uint32_t cycle = 0,
                 epsilon_change_func_t epsilon_change_function = nullptr);
    void set_service_enabled(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                             bool state);
    void set_payload(Payload& payload, std::uint16_t client = 0, bool force = false, bool flush = false);
    void send(std::uint32_t interval = 0);
    void send_initial_event();

    inline bool is_initialized() {
        return initialized_;
    }
    inline std::uint32_t get_update_cycle() {
        return update_cycle_;
    }
    inline bool is_field() {
        return is_field_;
    }

private:
    void init_event(ServiceInfo* config_event);
    void send_notification();

    ApplicationManager* host_ = nullptr;
    std::mutex mutex_;
    bool service_enabled_ = false;

    std::uint16_t service_id_{0};
    std::uint16_t instance_id_{0};
    std::uint16_t event_id_{0};
    std::uint8_t major_version_{0};
    std::uint32_t request_id_{0};

    bool initialized_ = false;
    std::shared_ptr<Message> notification_{nullptr};

    bool is_field_ = false;
    std::uint32_t update_cycle_ = 0;
    std::uint32_t remaining_update_ = 0;
    EventStrategy strategy_{EventStrategy::UpdateOnChange};
    epsilon_change_func_t epsilon_change_function_;

    bool print_not_initialized_message_ = false;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_EVENT_MANAGER_H
