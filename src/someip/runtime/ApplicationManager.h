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

#ifndef LG_SOMEIP_APPLICATION_MANAGER_H
#define LG_SOMEIP_APPLICATION_MANAGER_H

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <atomic>
#include <algorithm>
#include <vector>

#include <config/Configuration.h>
#include <message/Message.h>
#include <utils/thread/ThreadPool.h>
#include <runtime/ApplicationCommonTypes.h>
#include <runtime/ApplicationConstant.h>

#define LGSOMEIP_NUM_OF_THREADS 5

namespace lgsomeip {

class ApplicationRouter;
class EventManager;
class OfferedEvent;
class ThreadPool;

class ApplicationManager {
public:
    ApplicationManager(std::string name, std::string config_path = "",
                       std::shared_ptr<ApplicationRouter> packet_router = nullptr);
    ~ApplicationManager();

    void init();
    void start();
    void join();
    void stop();

    std::uint16_t get_application_id();
    const std::string& get_application_name();
    std::shared_ptr<Configuration> get_configuration() {
        return configuration_;
    }

    void clear_all_handler();

private:
    std::string application_name_;
    std::uint16_t application_id_;

    std::shared_ptr<ApplicationRouter> packet_router_;
    std::shared_ptr<Configuration> configuration_;
    std::shared_ptr<ThreadPool> thread_pool_;
    // Service Management
public:
    void offer_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version = SOMEIP_DEFAULT_MAJOR,
                       std::uint32_t minor_version = SOMEIP_DEFAULT_MINOR);

    void stop_offer_service(std::uint16_t service, std::uint16_t instance,
                            std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                            std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);

    void find_service(std::uint16_t service, std::uint16_t instance,
                      std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                      std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);

    void request_service(std::uint16_t service, std::uint16_t instance,
                         std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                         std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);

    void release_service(std::uint16_t service, std::uint16_t instance);

    bool is_service_available(std::uint16_t service, std::uint16_t instance,
                              std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                              std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR) const;
    bool are_service_available(available_t& available, std::uint16_t service = SOMEIP_DEFAULT_ANY_SERVICE,
                               std::uint16_t instance = SOMEIP_DEFAULT_ANY_INSTANCE,
                               std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                               std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR) const;

    void on_offer_service(const std::shared_ptr<MessageSD> message);

    void on_find_service(std::shared_ptr<MessageSD> message);

#if defined(ENABLE_SOMEIP_IPC)
    void do_service_availability_handler(const std::shared_ptr<MessageSD> message);
    bool is_internal_message_for_ipc(std::uint16_t service_id, std::uint16_t instance_id);
#endif // ENABLE_SOMEIP_IPC
private:
    void set_requested_service_state(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                     std::uint32_t minor_version, std::uint8_t state);
    std::uint8_t get_requested_service_state(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                             std::uint32_t minor_version) const;

    void send_offer_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                            std::uint32_t minor_version, std::uint32_t ttl);

    void send_find_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                           std::uint32_t minor_version, std::uint32_t ttl);

    void send_error(std::shared_ptr<MessageSOMEIP> message, uint8_t return_code, std::uint8_t* error_message = nullptr);

    mutable std::recursive_mutex offer_service_list_mutex_;
    mutable std::mutex service_management_mutex_;
    servicelist_t offer_service_list_;
    servicelist_t request_service_list_;

    // Event Management
public:
    void offer_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id,
                     const std::set<std::uint16_t>& event_groups, bool is_field = false, std::uint32_t cycle = 0,
                     epsilon_change_func_t epsilon_change_function = nullptr);
    void stop_offer_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id);

private:
    // Event Message Management
    mutable std::mutex offer_event_list_mutex_;
    mutable std::mutex event_manager_mutex_;
    servicelist_t offer_event_list_;
    std::shared_ptr<EventManager> event_manager_;

public:
    void request_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id,
                       const std::set<std::uint16_t>& event_groups);

    void release_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id);

    void subscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                   std::uint8_t major_version = SOMEIP_DEFAULT_MAJOR,
                   std::uint16_t event_id = SOMEIP_DEFAULT_ANY_EVENT);

    void unsubscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group);

    void on_subscribe_eventgroup(std::shared_ptr<MessageSD> message);
    void on_subscribe_eventgroup_ack(std::shared_ptr<MessageSD> message);

private:
    void send_subscribe_eventgroup(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                   std::uint16_t event_group, std::uint32_t ttl);
    void send_initial_event(std::uint16_t service, std::uint16_t instance, const std::uint16_t event_group);

    mutable std::mutex subscribe_event_list_mutex_;
    mutable std::mutex requested_event_list_mutex_;
    eventlist_t subscribe_event_list_;
    reqeventlist_t requested_event_list_;

    // Handler Management
public:
    void register_application_state_handler(application_state_handler_t handler);
    void unregister_application_state_handler();

    void register_message_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t method_id,
                                  message_handler_t handler, bool is_provider = false);
    void unregister_message_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t method_id,
                                    bool is_provider = false);

    void register_async_subscription_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                                             async_subscription_handler_t handler);
    void unregister_async_subscription_handler(std::uint16_t service, std::uint16_t instance,
                                               std::uint16_t event_group);

    void register_subscription_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                                       subscription_handler_t handler);
    void unregister_subscription_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group);

    void register_subscription_error_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                                             error_handler_t handler);
    void unregister_subscription_error_handler(std::uint16_t service, std::uint16_t instance,
                                               std::uint16_t event_group);

    void register_subscription_status_handler(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                                              std::uint16_t event_id, subscription_status_handler_t handler,
                                              bool selective);

    void unregister_subscription_status_handler(std::uint16_t service, std::uint16_t instance,
                                                std::uint16_t event_group, std::uint16_t event_id);

    void register_availability_handler(std::uint16_t service, std::uint16_t instance,
                                       service_availability_handler_t handler,
                                       std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                                       std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);
    void unregister_availability_handler(std::uint16_t service, std::uint16_t instance,
                                         std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                                         std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);

    void on_application_state(bool connected);

private:
    template <typename HANDLER> // subscription_handler_t, error_handler_t
    using HandlerSetType = std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint16_t, HANDLER>>>;

    template <typename HANDLER> // subscription_status_handler_t
    using HandlerSetType2 = std::map<std::uint16_t /*serviceid*/,
                                     std::map<std::uint16_t /*instanceid*/,
                                              std::map<std::uint16_t /*eventgroupid*/,
                                                       std::map<std::uint16_t /*eventid*/, std::pair<HANDLER, bool>>>>>;

    template <typename HANDLER> // message_handler_t
    using HandlerSetType3 =
        std::map<std::uint16_t, std::map<std::uint16_t, std::map<std::uint16_t, std::map<bool, std::vector<HANDLER>>>>>;

    using serviceAvailabilityHandler = std::map<
        std::uint16_t,
        std::map<std::uint16_t, std::map<std::uint8_t, std::map<std::uint32_t, service_availability_handler_t>>>>;

    using subscriptionHandler =
        std::map<std::uint16_t,
                 std::map<std::uint16_t,
                          std::map<std::uint16_t, std::pair<subscription_handler_t, async_subscription_handler_t>>>>;

    bool application_state_ = false;
    application_state_handler_t application_state_handler_ = nullptr;
    HandlerSetType3<message_handler_t> message_handler_set_;
    HandlerSetType2<subscription_status_handler_t> subscription_status_handler_set_;
    HandlerSetType<error_handler_t> error_handler_set_;
    serviceAvailabilityHandler service_availability_handler_map_;
    subscriptionHandler subscription_handler_map_;

    std::mutex application_handler_mutex_;
    std::mutex message_handler_mutex_;
    std::mutex subscription_status_handler_mutex_;
    std::mutex subscription_handler_mutex_;
    std::mutex error_handler_mutex_;

    // Message Sender
public:
    void send(Message& message, bool flush = true);
    void send(std::shared_ptr<Message> message, bool flush = true);
    void notify(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id, std::shared_ptr<Payload> payload,
                std::uint16_t client = 0, bool force = false, bool flush = false) const;

    // Message Receiver
public:
    // for Service Conrtol Message in SOME/IP-SD
    void on_message(std::shared_ptr<MessageSD> message);
    // for Service Data Message in SOME/IP
    void on_message(std::shared_ptr<MessageSOMEIP> message);

private:
#if !defined(ENABLE_SOMEIP_IPC)
    std::vector<message_handler_t> get_message_handler(std::uint16_t service_id, std::uint16_t instance_id,
                                                       std::uint16_t method_id, std::uint8_t major_version,
                                                       std::uint8_t type);
#else
    std::vector<message_handler_t> get_message_handler(std::uint16_t service_id, std::uint16_t instance_id,
                                                       std::uint16_t method_id, std::uint8_t major_version,
                                                       std::uint8_t type, std::uint16_t app_id,
                                                       std::uint16_t request_id);
#endif // ENABLE_SOMEIP_IPC
    std::uint16_t get_new_session_id();
    std::uint32_t get_new_request_id(std::uint32_t request_id = 0, std::uint8_t type = SOMEIP_MESSAGE_TYPE::REQUEST);
    std::uint32_t get_sd_request_id();

    std::atomic<std::uint16_t> session_count_{0};

    // SOME/IP-TP Implementation
private:
    std::uint32_t previous_session_id_ = 0;
    std::uint32_t previous_message_id_ = 0;

    bool check_enabled_tp_message(MessageSOMEIP& message);
    void send_tp_message(MessageSOMEIP& message);
    void on_message_tp(MessageSOMEIP& message);

    //    std::map<std::uint32_t, std::map<std::uint32_t, std::shared_ptr<MessageSOMEIP>>> tp_message_cache_;
    std::shared_ptr<MessageSOMEIP> tp_message_cache_ = nullptr;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_APPLICATION_MANAGER_H
