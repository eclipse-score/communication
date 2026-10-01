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

#include <sstream>

#include <message/Message.h>
#include <message/MessageComposer.h>

#include <packetrouter/PacketRouterProxy.h>
#include <runtime/ApplicationConstant.h>
#include <runtime/ApplicationManager.h>
#include <runtime/EventManager.h>

#include <exception/Exception.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
#include <utils/statistics/SomeipPacketStatistics.h>
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

#if defined(LINUX)
#include <sys/syscall.h>
#elif defined(QNX)
#include <process.h>
#endif
#include <thread>

namespace lgsomeip {

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method
// -----------------------------------------------------------------------------

ApplicationManager::ApplicationManager(std::string name, std::string config_path,
                                       std::shared_ptr<ApplicationRouter> packet_router)
    : application_name_(name), packet_router_(std::move(packet_router)) {
    if (packet_router_ == nullptr) {
        packet_router_ = std::make_shared<PacketRouterProxy>(this);
    }

    configuration_ = std::make_shared<Configuration>(config_path);
    application_id_ = configuration_->get_application_id(name);

    thread_pool_ = std::make_shared<ThreadPool>(LGSOMEIP_NUM_OF_THREADS);

    if (application_id_ == 0) {
#if defined(LINUX)
        application_id_ = static_cast<std::uint16_t>(syscall(SYS_gettid));
#elif defined(QNX)
        application_id_ = static_cast<std::uint16_t>(getpid());
#endif
        LGSOMEIP_LOG_INFO << "ApplicationManager::ApplicationManager / ApplicationID is 0. Assign a temporary value: "
                          << format_named_id("AppID", this->application_id_, 4);
    }

    if (application_name_.size() == 0) {
        application_name_ = "noname" + application_id_;
    }

    LGSOMEIP_LOG_INFO << "ApplicationManager::ApplicationManager / AppName: " << name << ", "
                      << format_named_id("AppID", this->application_id_, 4);
}

void ApplicationManager::init() {
    packet_router_->init();
    event_manager_ = std::make_shared<EventManager>(this);
}

void ApplicationManager::start() {
    LGSOMEIP_LOG_INFO << "LG SOME/IP Library v" << SOMEIP_VERSION << " Start!!";
    packet_router_->start();
    event_manager_->start();
}

void ApplicationManager::stop() {
    clear_all_handler();

    if (event_manager_ != nullptr) {
        event_manager_->stop();
    }
    if (packet_router_ != nullptr) {
        packet_router_->stop();
    }
}

void ApplicationManager::join() {
    event_manager_->join();
    packet_router_->join();
}

std::uint16_t ApplicationManager::get_application_id() {
    return application_id_;
}

const std::string& ApplicationManager::get_application_name() {
    return application_name_;
}

void ApplicationManager::clear_all_handler() {
    unregister_application_state_handler();
    {
        std::lock_guard<std::mutex> guard(service_management_mutex_);
        service_availability_handler_map_.clear();
    }
    {
        std::lock_guard<std::mutex> guard(message_handler_mutex_);
        message_handler_set_.clear();
    }
    {
        std::lock_guard<std::mutex> guard(subscription_status_handler_mutex_);
        subscription_status_handler_set_.clear();
    }
    {
        std::lock_guard<std::mutex> guard(subscription_handler_mutex_);
        subscription_handler_map_.clear();
    }
    {
        std::lock_guard<std::mutex> guard(error_handler_mutex_);
        error_handler_set_.clear();
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (Service Management)
// -----------------------------------------------------------------------------

// OFFER SERVICE
void ApplicationManager::offer_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                       std::uint32_t minor_version) {
    std::unique_lock<std::recursive_mutex> lck_offer(offer_service_list_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::offer_service "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version);

    offer_service_list_[service][instance][major_version][minor_version] = SOMEIP_SERVICE_OFFER;
    lck_offer.unlock();

    event_manager_->set_service_enabled(service, instance, major_version, true);

    std::uint32_t ttl = 0xFFFFFF;
    send_offer_service(service, instance, major_version, minor_version, ttl);
}

void ApplicationManager::stop_offer_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                            std::uint32_t minor_version) {
    std::lock_guard<std::recursive_mutex> guard(offer_service_list_mutex_);

    LGSOMEIP_LOG_INFO << "ApplicationManager::stop_offer_service "
                      << format_service_instance_interface_version(service, instance, major_version, minor_version);

    std::uint8_t major;
    std::uint32_t minor;

    if (major_version == SOMEIP_DEFAULT_ANY_MAJOR) {
        auto& majorlist = offer_service_list_[service][instance];
        for (auto& majors : majorlist) {
            major = majors.first;
            if (major == SOMEIP_DEFAULT_ANY_MAJOR)
                continue;
            if (minor_version == SOMEIP_DEFAULT_ANY_MINOR) {
                auto& minorlist = offer_service_list_[service][instance][major];
                for (auto& minors : minorlist) {
                    minor = minors.first;
                    if (minor == SOMEIP_DEFAULT_ANY_MINOR)
                        continue;
                    stop_offer_service(service, instance, major, minor);
                }
            } else {
                minor = minor_version;
                stop_offer_service(service, instance, major, minor);
            }
        }
    } else if (minor_version == SOMEIP_DEFAULT_ANY_MINOR) {
        major = major_version;
        auto& minorlist = offer_service_list_[service][instance][major];
        for (auto& minors : minorlist) {
            minor = minors.first;
            if (minor == SOMEIP_DEFAULT_ANY_MINOR)
                continue;
            stop_offer_service(service, instance, major, minor);
        }
    } else {
        if (offer_service_list_[service][instance][major_version][minor_version] == SOMEIP_SERVICE_OFFER) {
            offer_service_list_[service][instance][major_version][minor_version] = SOMEIP_SERVICE_STOP_OFFER;
            event_manager_->set_service_enabled(service, instance, major_version, false);

            std::uint32_t ttl = 0;
            send_offer_service(service, instance, major_version, minor_version, ttl);
        }
    }
}

// FIND SERVICE
void ApplicationManager::find_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                      std::uint32_t minor_version) {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    LGSOMEIP_LOG_INFO << "ApplicationManager::find_service "
                      << format_service_instance_interface_version(service, instance, major_version, minor_version);

    set_requested_service_state(service, instance, major_version, minor_version, SOMEIP_SERVICE_REQUEST);

    std::uint32_t ttl = SOMEIP_DEFAULT_TTL_ON;

    send_find_service(service, instance, major_version, minor_version, ttl);
}

void ApplicationManager::request_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                         std::uint32_t minor_version) {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_service "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " start";

    if (application_state_ == false) {
        LGSOMEIP_LOG_INFO << "ApplicationManager::request_service "
                          << format_service_instance_interface_version(service, instance, major_version, minor_version)
                          << " Application is disconnected!";
    }

    auto service_config = configuration_->get_service_info(service, instance);
    // If service_config == nullptr, It's IPC
    if (service_config != nullptr) {
        // Set InstanceID, MajorVersion, MinorVersion from lgsomeip_config.json when the factors are in the config file.
        // Otherwise, the values are set from the SOME/IP binding layer
        if (service_config->has_instance_id()) {
            instance = service_config->get_instance_id();
        }
        if (service_config->has_major_version()) {
            major_version = service_config->get_major_version();
        }
        if (service_config->has_minor_version()) {
            minor_version = service_config->get_minor_version();
        }
    }

    auto state = get_requested_service_state(service, instance, major_version, minor_version);
    if (state == SOMEIP_SERVICE_REQUEST || state == SOMEIP_SERVICE_AVAILABLE) {
        LGSOMEIP_LOG_INFO << "ApplicationManager::request_service "
                          << format_service_instance_interface_version(service, instance, major_version, minor_version)
                          << " Already requested.";
        return;
    }

    set_requested_service_state(service, instance, major_version, minor_version, SOMEIP_SERVICE_REQUEST);
    std::uint32_t ttl = SOMEIP_DEFAULT_TTL_ON;

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_service "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " send FindService.";

    send_find_service(service, instance, major_version, minor_version, ttl);
}

void ApplicationManager::release_service(std::uint16_t service, std::uint16_t instance) {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::release_service " << format_service_instance_id(service, instance)
                       << " start";

    if (request_service_list_[service][instance].size() > 0) {
        std::uint32_t ttl = SOMEIP_DEFAULT_TTL_OFF;
        for (auto& instance_info : request_service_list_[service][instance]) {
            std::uint8_t major = instance_info.first;
            for (auto& info : instance_info.second) {
                std::uint32_t minor = info.first;
                std::uint8_t state = info.second;
                if (state != SOMEIP_SERVICE_REQUEST && state == SOMEIP_SERVICE_AVAILABLE)
                    continue;

                LGSOMEIP_LOG_DEBUG << "ApplicationManager::release_service "
                                   << format_service_instance_interface_version(service, instance, major, minor);

                send_find_service(service, instance, major, minor, ttl);
            }
        }

        request_service_list_[service][instance].clear();

        LGSOMEIP_LOG_INFO << "ApplicationManager::release_service " << format_service_instance_id(service, instance)
                          << " is released.";
    }
}

std::string get_request_service_state_string(std::uint8_t state) {
    std::string state_string;

    switch (state) {
    case SOMEIP_SERVICE_AVAILABLE:
        state_string = "SOMEIP_SERVICE_AVAILABLE";
        break;
    case SOMEIP_SERVICE_REQUEST:
        state_string = "SOMEIP_SERVICE_REQUEST";
        break;
    default:
        state_string = "NONE";
        break;
    }

    return state_string;
}

void ApplicationManager::set_requested_service_state(std::uint16_t service, std::uint16_t instance,
                                                     std::uint8_t major_version, std::uint32_t minor_version,
                                                     std::uint8_t state) {
    for (auto& service_entry : request_service_list_) {
        if (service_entry.first != service)
            continue;
        for (auto& instance_entry : service_entry.second) {
            if (instance_entry.first != instance && instance_entry.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                continue;
            // major.first is the value stored in request_service_list_ by FindService
            // major_version is the value stored in incomming OfferService
            for (auto& major : instance_entry.second) {
                for (auto& minor : major.second) {
                    // Service has already been requested (FindService)
                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::set_requested_service_state "
                                       << format_service_instance_interface_version(
                                              service_entry.first, instance_entry.first, major.first, minor.first)
                                       << " " << "set state: " << get_request_service_state_string(minor.second)
                                       << " -> " << get_request_service_state_string(state);

                    minor.second = state;
                }
            }
        }
    }

    if (request_service_list_[service][instance][major_version][minor_version] == 0) {
        // Service has not been requested (FindService)
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::set_requested_service_state "
                           << format_service_instance_interface_version(service, instance, major_version, minor_version)
                           << " Has not requested. set state: " << get_request_service_state_string(state);

        request_service_list_[service][instance][major_version][minor_version] = state;
    }

    // TODO : implement the routine when Service ID is ANY_MAJOR(0xFFFF).
}

std::uint8_t ApplicationManager::get_requested_service_state(std::uint16_t service, std::uint16_t instance,
                                                             std::uint8_t major_version,
                                                             std::uint32_t minor_version) const {
    // get Request Service State : SOMEIP_SERVICE_AVAILABLE or SOMEIP_SERVICE_REQUEST
    // when to fail to find the state : return 0
    for (auto& service_entry : request_service_list_) {
        if (service_entry.first != service && service_entry.first != SOMEIP_DEFAULT_ANY_SERVICE)
            continue;
        for (auto& instance_entry : service_entry.second) {
            if (instance_entry.first != instance && instance_entry.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                continue;
            for (auto& major : instance_entry.second) {
                for (auto& minor : major.second) {
                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_requested_service_state "
                                       << format_service_instance_interface_version(
                                              service_entry.first, instance_entry.first, major.first, minor.first)
                                       << " List, " << "State: " << get_request_service_state_string(minor.second);

                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_requested_service_state "
                                       << format_service_instance_interface_version(
                                              service_entry.first, instance_entry.first, major.first, minor.first)
                                       << " Returned, " << "State: " << get_request_service_state_string(minor.second);

                    return minor.second;
                }
            }
        }
    }

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_requested_service_state "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " has not requested yet.";
    // TODO : implement the routine when Service ID is ANY_MAJOR(0xFFFF).

    return std::uint8_t{0};
}

bool ApplicationManager::is_service_available(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                              std::uint32_t minor_version) const {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    std::uint8_t state = get_requested_service_state(service, instance, major_version, minor_version);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::is_service_available "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " state: " << get_request_service_state_string(state);

    if (state == SOMEIP_SERVICE_AVAILABLE)
        return true;

    return false;
}

bool ApplicationManager::are_service_available(available_t& available, std::uint16_t service, std::uint16_t instance,
                                               std::uint8_t major_version, std::uint32_t minor_version) const {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    for (auto& service_entry : request_service_list_) {
        if (service_entry.first != service && service_entry.first != SOMEIP_DEFAULT_ANY_SERVICE)
            continue;
        for (auto& instance_entry : service_entry.second) {
            if (instance_entry.first != instance && instance_entry.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                continue;
            for (auto& major : instance_entry.second) {
                for (auto& minor : major.second) {
                    if (minor.second == SOMEIP_SERVICE_AVAILABLE) {
                        available[service_entry.first][instance_entry.first][major.first] = minor.first;
                    }
                }
            }
        }
    }

    if (available.empty()) {
        available[service][instance][major_version] = minor_version;

        LGSOMEIP_LOG_DEBUG << "ApplicationManager::are_service_available "
                           << format_service_instance_interface_version(service, instance, major_version, minor_version)
                           << " not available";

        return false;
    }

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::are_service_available "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " available";

    return true;
}

void ApplicationManager::on_offer_service(const std::shared_ptr<MessageSD> message) {
    auto& entry = message->entry(0);
    std::uint32_t req_id = message->get_request_id();
    std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
    std::uint16_t service_id = entry.get_service_id();
    std::uint16_t instance_id = entry.get_instance_id();
    std::uint8_t major_version = entry.get_major_version();
    std::uint32_t minor_version = entry.get_minor_version();
    std::uint32_t ttl = entry.get_ttl();

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_offer_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << ", " << format_named_id("AppID", app_id, 4) << ", TTL: " << ttl;

    std::unique_lock<std::mutex> lck_service(service_management_mutex_);

#if defined(ENABLE_SOMEIP_IPC)
    if (is_internal_message_for_ipc(service_id, instance_id)) {
        packet_router_->add_ipc_route(service_id, instance_id, 0xffff, app_id, true);
    }
#endif // ENABLE_SOMEIP_IPC

    service_availability_handler_t handler = nullptr;
    for (auto& service : service_availability_handler_map_) {
        if (service.first != service_id && service.first != SOMEIP_DEFAULT_ANY_SERVICE)
            continue;
        for (auto& instance : service.second) {
            if (instance.first != instance_id && instance.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                continue;
            for (auto& major : instance.second) {
                for (auto& minor : major.second) {
                    handler = minor.second;
                    if (handler != nullptr) {
                        // Check whether ServiceAvailablilityHandler has been already called or not to prevent calling
                        // ServiceAvailablityHandler many times
                        if (get_requested_service_state(service_id, instance_id, major_version, minor_version) !=
                                SOMEIP_SERVICE_AVAILABLE ||
                            ttl == 0) {
                            LGSOMEIP_LOG_INFO << "ApplicationManager::on_offer_service "
                                              << format_service_instance_interface_version(service_id, instance_id,
                                                                                           major_version, minor_version)
                                              << " call ServiceAvailablilityHandler, " << "TTL: " << ttl;

                            std::thread t([handler, service_id, instance_id, ttl]() {
                                handler(service_id, instance_id, (ttl != 0) ? true : false);
                            });
                            // For assigning thread name
                            pthread_setname_np(t.native_handle(), "SomeipOnOffer");
                            t.detach();
                        } else {
                            LGSOMEIP_LOG_DEBUG
                                << "ApplicationManager::on_offer_service "
                                << format_service_instance_interface_version(service.first, instance.first, major.first,
                                                                             minor.first)
                                << " ServiceAvailablilityHandler has been already called, " << "TTL: " << ttl;
                        }
                    } else {
                        LGSOMEIP_LOG_INFO << "ApplicationManager::on_offer_service "
                                          << format_service_instance_interface_version(service.first, instance.first,
                                                                                       major.first, minor.first)
                                          << " ServiceAvailablilityHandler is null, " << "TTL: " << ttl;
                    }
                }
            }
        }
    }
    if (ttl > 0) {
        // Set Request Service State to SOMEIP_SERVICE_AVAILABLE
        set_requested_service_state(service_id, instance_id, major_version, minor_version, SOMEIP_SERVICE_AVAILABLE);
    } else {
        set_requested_service_state(service_id, instance_id, major_version, minor_version, SOMEIP_SERVICE_REQUEST);
    }
    lck_service.unlock();

    // Send SubscribeEventgroup
    std::lock_guard<std::mutex> guard(subscribe_event_list_mutex_);
    auto& subscribelist = subscribe_event_list_[service_id][instance_id];
    if (subscribelist.size() == 0) {
        subscribelist = subscribe_event_list_[service_id][SOMEIP_DEFAULT_ANY_INSTANCE];
        if (subscribelist.size() == 0)
            return;
    }

    if (ttl == 0) {
        for (auto& eventgrouplist : subscribelist) {
            for (auto& subscribe : eventgrouplist.second) {
                if (subscribe.second != SOMEIP_EVENT_SUBSCRIBE) {
                    subscribe.second = SOMEIP_EVENT_SUBSCRIBE;
                }
            }
        }
        // When receiving StopOffer, the SOME/IP library should notify the binding layer of the state of subscription
        // for events related to this service.
        std::lock_guard<std::mutex> guard(subscription_status_handler_mutex_);

        auto send_subscription_state = [&](std::uint16_t service_id, std::uint16_t instance_id) -> void {
            auto found_service = subscription_status_handler_set_.find(service_id);
            if (found_service == subscription_status_handler_set_.end())
                return;

            auto found_instance = found_service->second.find(instance_id);
            if (found_instance != found_service->second.end()) {
                for (auto& eventgroup : found_instance->second) {
                    for (auto& event : eventgroup.second) {
                        auto handler = event.second.first;
                        if (handler != nullptr) {
                            LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_offer_service "
                                               << format_service_instance_id(service_id, instance_id) << " call "
                                               << "subscription_status_handler for "
                                               << format_named_id("EventGroupID", eventgroup.first, 4) << " "
                                               << format_named_id("EventID", event.first, 4) << ", TTL: " << ttl;

                            handler(service_id, instance_id, eventgroup.first, event.first, 0xff);
                        }
                    }
                }
            }
        };
        send_subscription_state(service_id, instance_id);
        send_subscription_state(service_id, SOMEIP_DEFAULT_ANY_INSTANCE);
    } else {
        auto eventgrouplist = subscribelist[major_version];
        if (eventgrouplist.size() == 0) {
            eventgrouplist = subscribelist[SOMEIP_DEFAULT_ANY_MAJOR];
            if (eventgrouplist.size() == 0)
                return;
        }

        for (auto& subscribe : eventgrouplist) {
            std::uint16_t eventgroup_id = subscribe.first;

            LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_offer_service / Subscribed "
                               << format_named_id("EventGroupID", eventgroup_id, 4) << " "
                               << format_named_id("State", subscribe.second, 2);

            if (subscribe.second == SOMEIP_EVENT_SUBSCRIBE) {
                int ttl = SOMEIP_DEFAULT_TTL_ON;

                LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_offer_service / sendSubscribedEventgroup "
                                   << format_named_id("EventGroupID", eventgroup_id, 4);

                send_subscribe_eventgroup(service_id, instance_id, major_version, eventgroup_id, ttl);
            }
        }
    }
}

void ApplicationManager::on_find_service(std::shared_ptr<MessageSD> message) {
    auto& entry = message->entry(0);
    std::uint32_t req_id = message->get_request_id();
    std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
    std::uint16_t service_id = entry.get_service_id();
    std::uint16_t instance_id = entry.get_instance_id();
    std::uint8_t major_version = entry.get_major_version();
    std::uint32_t minor_version = entry.get_minor_version();
    std::uint32_t ttl = entry.get_ttl();

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_offer_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << ", " << format_named_id("AppID", app_id, 4) << ", TTL: " << ttl;

    // TODO :: Implementation of processing find service message
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (EVENT MANAGEMENT)
// -----------------------------------------------------------------------------
void ApplicationManager::offer_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id,
                                     const std::set<std::uint16_t>& event_groups, bool is_field, std::uint32_t cycle,
                                     epsilon_change_func_t epsilon_change_function) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::offer_event "
                       << format_service_instance_event_id(service, instance, event_id)
                       << ", is_field: " << (is_field ? "true" : "false") << ", update_cycle: " << cycle
                       << ", epsilon_func: " << (epsilon_change_function ? "set" : "not set");

    std::uint8_t major = SOMEIP_DEFAULT_ANY_MAJOR;

    std::unique_lock<std::recursive_mutex> lck_service(offer_service_list_mutex_);
    auto& svc_state = offer_service_list_[service][instance];
    if (svc_state.size() != 0) {
        major = svc_state.begin()->first;
    }
    lck_service.unlock();

    if (event_id < 0x8000) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::offer_event / event < 0x8000), "
                           << format_service_instance_event_id(service, instance, event_id);
    }
    std::unique_lock<std::mutex> lck_offerevent(offer_event_list_mutex_);
    for (std::uint16_t eventgroup : event_groups) {
        offer_event_list_[service][instance][eventgroup][event_id] = SOMEIP_EVENT_OFFER;
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::offer_event "
                           << format_service_instance_event_id(service, instance, event_id) << " "
                           << format_named_id("EventGroupID", eventgroup, 4) << " is offered.";
    }
    lck_offerevent.unlock();

    // add event object to event manager
    event_manager_->add_event(service, instance, event_id, major, is_field, cycle, epsilon_change_function);

    // enable event when service offered previous
    lck_service.lock();
    auto& service_state = offer_service_list_[service][instance];
    for (auto& majors : service_state) {
        std::uint16_t major = majors.first;
        for (auto& minors : majors.second) {
            if (minors.second == SOMEIP_SERVICE_OFFER) {
                event_manager_->set_service_enabled(service, instance, major, true);
            }
        }
    }
}

void ApplicationManager::stop_offer_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id) {
    LGSOMEIP_LOG_INFO << "ApplicationManager::stop_offer_event "
                      << format_service_instance_event_id(service, instance, event_id);

    std::unique_lock<std::mutex> lck_offerevent(offer_event_list_mutex_);
    for (auto& eventgroupinfo : offer_event_list_[service][instance]) {
        eventgroupinfo.second.erase(event_id);
    }
    lck_offerevent.unlock();

    // remove event object
    event_manager_->remove_event(service, instance, event_id);
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (Event Message Management)
// -----------------------------------------------------------------------------
void ApplicationManager::request_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id,
                                       const std::set<std::uint16_t>& event_groups) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event "
                       << format_service_instance_event_id(service, instance, event_id) << " start";

    // Check Consistency between Configuration Data and requested Subscribe data(Event / Event Group)
    auto service_config = configuration_->get_service_info(service, instance);

    if (service_config == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event " << format_service_instance_id(service, instance)
                           << " is not in the someip configuration. This service is for IPC.";

        for (std::uint16_t eventgroupid : event_groups) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event "
                               << format_service_instance_event_id(service, instance, event_id) << " "
                               << format_named_id("EventGroupID", eventgroupid, 4) << " is requested!";

            std::lock_guard<std::mutex> guard(requested_event_list_mutex_);
            requested_event_list_[service][instance][event_id][eventgroupid] = SOMEIP_EVENT_REQUEST;
        }
        return;
    }

    if (service_config->get_event(event_id) == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event "
                           << format_service_instance_event_id(service, instance, event_id)
                           << " is not in the someip configuration, but it will be used.";
    }

    for (std::uint16_t eventgroupid : event_groups) {
        if (service_config->get_event_group(eventgroupid) == nullptr) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event " << format_service_instance_id(service, instance)
                               << " " << format_named_id("EventGroupID", eventgroupid, 4)
                               << " is not in the someip configuration, but it will be used.";
        }

        LGSOMEIP_LOG_DEBUG << "ApplicationManager::request_event "
                           << format_service_instance_event_id(service, instance, event_id) << " "
                           << format_named_id("EventGroupID", eventgroupid, 4) << " is requested!";

        std::lock_guard<std::mutex> guard(requested_event_list_mutex_);
        requested_event_list_[service][instance][event_id][eventgroupid] = SOMEIP_EVENT_REQUEST;
    }
}

void ApplicationManager::release_event(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id) {
    std::lock_guard<std::mutex> guard(requested_event_list_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::release_event / "
                       << format_service_instance_event_id(service, instance, event_id) << " start";

    if (requested_event_list_.find(service) == std::end(requested_event_list_))
        return;
    auto& servicelist = requested_event_list_[service];

    if (servicelist.find(instance) == std::end(servicelist))
        return;
    auto& instancelist = servicelist[instance];

    instancelist.erase(event_id);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::release_event / "
                       << format_service_instance_event_id(service, instance, event_id) << " is removed.";
}

void ApplicationManager::subscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group,
                                   std::uint8_t major_version, std::uint16_t event_id) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::subscribe "
                       << format_service_instance_interface_major_version(service, instance, major_version) << " "
                       << format_service_instance_event_id(service, instance, event_id) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " start";

    // Check Consistency between Configuration Data and requested Subscribe data(Event / Event Group)
    auto service_config = configuration_->get_service_info(service, instance);

    if (service_config == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::subscribe " << format_service_instance_id(service, instance)
                           << " is not in the someip configuration, but it will be used.";
    } else if (service_config->get_event_group(event_group) == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::subscribe " << format_service_instance_id(service, instance) << " "
                           << format_named_id("EventGroupID", event_group, 4)
                           << " is not in the someip configuration, but it will be used.";
    }

    bool find_req_event = false;

    std::unique_lock<std::mutex> lck_reqevent(requested_event_list_mutex_);
    auto& eventlist = requested_event_list_[service][instance];
    for (auto& event : eventlist) {
        auto item = event.second.find(event_group);
        if (item != event.second.end() && item->second == SOMEIP_EVENT_REQUEST) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::subscribe "
                               << format_service_instance_event_id(service, instance, event.first) << " "
                               << format_named_id("EventGroupID", event_group, 4) << " is found!";

            find_req_event = true;
        }
    }
    lck_reqevent.unlock();

    if (!find_req_event) {
        LGSOMEIP_LOG_WARN << "ApplicationManager::subscribe "
                          << format_service_instance_event_id(service, instance, event_id) << " "
                          << format_named_id("EventGroupID", event_group, 4) << " is NOT requested!";

        return;
    }

    // Set Service Info.
    if (application_state_ == false) {
        LGSOMEIP_LOG_WARN << "ApplicationManager::subscribe / Application is disconnected.";
    }

    std::lock_guard<std::mutex> lck_service(service_management_mutex_);

    auto servicestate = get_requested_service_state(service, instance, major_version, SOMEIP_DEFAULT_ANY_MINOR);

    std::lock_guard<std::mutex> guard(subscribe_event_list_mutex_);
    if (servicestate == SOMEIP_SERVICE_AVAILABLE) {
        subscribe_event_list_[service][instance][major_version][event_group] = SOMEIP_EVENT_SUBSCRIBE;
        send_subscribe_eventgroup(service, instance, major_version, event_group, SOMEIP_DEFAULT_TTL_ON);
    } else {
        subscribe_event_list_[service][instance][major_version][event_group] = SOMEIP_EVENT_SUBSCRIBE;

        LGSOMEIP_LOG_WARN << "ApplicationManager::subscribe "
                          << format_service_instance_interface_major_version(service, instance, major_version) << " "
                          << format_named_id("EventGroupID", event_group, 4)
                          << " is NOT sent! (service has not been available yet)";
    }
}

void ApplicationManager::unsubscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t event_group) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unsubscribe " << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " start";

    std::lock_guard<std::mutex> guard(subscribe_event_list_mutex_);
    for (auto& item : subscribe_event_list_[service][instance]) {
        std::uint8_t major = item.first;
        auto& eventgroup = item.second;
        auto it = eventgroup.find(event_group);
        if (it != eventgroup.end()) {
            // send unsubscribe even before receiving subscribe ack
            if (it->second == SOMEIP_EVENT_SUBSCRIBE_ACK || it->second == SOMEIP_EVENT_SUBSCRIBE) {
                it->second = SOMEIP_EVENT_UNSUBSCRIBE;

                send_subscribe_eventgroup(service, instance, major, event_group, SOMEIP_DEFAULT_TTL_OFF);

                LGSOMEIP_LOG_DEBUG << "ApplicationManager::unsubscribe "
                                   << format_service_instance_id(service, instance) << " "
                                   << format_named_id("EventGroupID", event_group, 4) << " is sent.";
            }
        }
    }
}

// Compose and Send SD-Message
void ApplicationManager::send_offer_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                            std::uint32_t minor_version, std::uint32_t ttl) {
    LGSOMEIP_LOG_INFO << "ApplicationManager::send_offer_service "
                      << format_service_instance_interface_version(service, instance, major_version, minor_version)
                      << ", TTL: " << ttl;

    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();
    std::uint32_t req_id = get_sd_request_id();
    message->set_request_id(req_id);

    SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major_version);
    entry.set_minor_version(minor_version);
    entry.set_ttl(ttl);

    MessageComposer::add_entry(message, &entry, nullptr);

    packet_router_->send_message(message);
}

void ApplicationManager::send_find_service(std::uint16_t service, std::uint16_t instance, std::uint8_t major_version,
                                           std::uint32_t minor_version, std::uint32_t ttl) {
    LGSOMEIP_LOG_INFO << "ApplicationManager::send_find_service "
                      << format_service_instance_interface_version(service, instance, major_version, minor_version)
                      << ", TTL: " << ttl;

    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();
    std::uint32_t req_id = get_sd_request_id();
    message->set_request_id(req_id);

    SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major_version);
    entry.set_minor_version(minor_version);
    entry.set_ttl(ttl);

    MessageComposer::add_entry(message, &entry, nullptr);

    packet_router_->send_message(message);
}

void ApplicationManager::send_subscribe_eventgroup(std::uint16_t service, std::uint16_t instance,
                                                   std::uint8_t major_version, std::uint16_t event_group,
                                                   std::uint32_t ttl) {
    LGSOMEIP_LOG_INFO << "ApplicationManager::send_subscribe_eventgroup "
                      << format_service_instance_interface_major_version(service, instance, major_version) << " "
                      << format_named_id("EventGroupID", event_group, 4) << ", TTL: " << ttl;

    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();
    std::uint32_t req_id = get_sd_request_id();
    message->set_request_id(req_id);
    message->set_flag(0xc0);

    SDEntry entry(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major_version);
    entry.set_event_group_id(event_group);
    entry.set_ttl(ttl);
    entry.set_flag(0x00);

    MessageComposer::add_entry(message, &entry, nullptr);

    packet_router_->send_message(message);
}

void ApplicationManager::send_initial_event(std::uint16_t service, std::uint16_t instance,
                                            const std::uint16_t event_group) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::send_initial_event " << format_service_instance_id(service, instance)
                       << " " << format_named_id("EventGroupID", event_group, 4);

    std::lock_guard<std::mutex> guard(offer_event_list_mutex_);
    auto& eglist = offer_event_list_[service][instance][event_group];

    for (auto& event : eglist) {
        if (event.second == SOMEIP_EVENT_OFFER) {
            event_manager_->notify_initial_event(service, instance, event.first);
        }
    }
}

#if defined(ENABLE_SOMEIP_IPC)
bool ApplicationManager::is_internal_message_for_ipc(std::uint16_t service_id, std::uint16_t instance_id) {
    if (configuration_->get_service_info(service_id, instance_id) == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::is_internal_message_for_ipc "
                           << format_service_instance_id(service_id, instance_id);
        return true;
    }
    return false;
}
#endif // ENABLE_SOMEIP_IPC

void ApplicationManager::on_subscribe_eventgroup(std::shared_ptr<MessageSD> message) {
    auto& entry = message->entry(0);
    std::uint32_t req_id = message->get_request_id();
    std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
    std::uint16_t service_id = entry.get_service_id();
    std::uint16_t instance_id = entry.get_instance_id();
    std::uint8_t major = entry.get_major_version();
    std::uint16_t eventgroup_id = entry.get_event_group_id();
    std::uint32_t ttl = entry.get_ttl();

    LGSOMEIP_LOG_INFO << "ApplicationManager::on_subscribe_eventgroup "
                      << format_service_instance_interface_major_version(service_id, instance_id, major) << " "
                      << format_named_id("EventGroupID", eventgroup_id, 4) << ", "
                      << format_named_id("AppID", app_id, 4) << ", TTL: " << ttl;

    // Send Subscribe Ack/Nack
    int result = 0;

    std::unique_lock<std::recursive_mutex> lck_offer(offer_service_list_mutex_);
    auto& svc_state = offer_service_list_[service_id][instance_id][major];
    for (auto& item : svc_state) {
        if (item.second == SOMEIP_SERVICE_OFFER) {
            result |= 0x10;
        }
    }

#if defined(ENABLE_SOMEIP_IPC)
    if (is_internal_message_for_ipc(service_id, instance_id)) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_subscribe_eventgroup for IPC";
        packet_router_->add_ipc_route(service_id, instance_id, eventgroup_id, app_id, false);
    }
#endif // ENABLE_SOMEIP_IPC

    lck_offer.unlock();

    std::unique_lock<std::mutex> lck_event(offer_event_list_mutex_);
    auto& event_state = offer_event_list_[service_id][instance_id][eventgroup_id];
    for (auto& item : event_state) {
        if (item.second == SOMEIP_EVENT_OFFER) {
            result |= 0x01;
        }
    }
    lck_event.unlock();

    entry.set_type(SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    if (result != 0x11 || ttl == 0) {
        entry.set_ttl(SOMEIP_DEFAULT_TTL_OFF);
    }

    std::uint32_t sendto_req_id = get_sd_request_id();
    std::uint16_t sendto_app_id = static_cast<std::uint16_t>(sendto_req_id >> 16);
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::sendSubscribeAckFunc "
                       << format_service_instance_id(service_id, instance_id) << " "
                       << format_named_id("RequestID", sendto_req_id, 8) << ", "
                       << format_named_id("AppID", sendto_app_id, 4);
    auto send_subscribe_ack_func = [this, &message, service_id, instance_id, eventgroup_id,
                                    &entry](const bool subscription_accepted) {
        if (subscription_accepted) {
            LGSOMEIP_LOG_INFO << "ApplicationManager::sendSubscribeAckFunc "
                              << format_service_instance_id(service_id, instance_id) << " "
                              << format_named_id("EventGroupID", eventgroup_id, 4) << ", SubscribeAck";
            packet_router_->send_message(message);
            //
            // The server shall send the first notifications/events(i.e. initial events)
            // immediately after sending the Subscribe Eventgroup Ack

            // The ApplicaionManager::sendSubscribeAckFunc is called only once
            // on the first subscribe.
            // Since the first subscribeAck, subscribeAck is sent by someip-daemon not lib.
            // Therefore, the send_initial_event method is also called once.
            send_initial_event(service_id, instance_id, eventgroup_id);
        } else {
            entry.set_ttl(SOMEIP_DEFAULT_TTL_OFF);
            LGSOMEIP_LOG_INFO << "ApplicationManager::sendSubscribeAckFunc "
                              << format_service_instance_id(service_id, instance_id) << " "
                              << format_named_id("EventGroupID", eventgroup_id, 4) << ", SubscribeNack";
            packet_router_->send_message(message);
        }
    };

    std::unique_lock<std::mutex> lck_subhdr(subscription_handler_mutex_);
    std::pair<subscription_handler_t, async_subscription_handler_t> handlers;
    auto found_service = subscription_handler_map_.find(service_id);
    if (found_service != subscription_handler_map_.end()) {
        auto found_instance = found_service->second.find(instance_id);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(eventgroup_id);
            if (found_eventgroup != found_instance->second.end()) {
                handlers = found_eventgroup->second;
            }
        }
    }
    lck_subhdr.unlock();

    if (auto handler = handlers.first) {
        bool ret = handler(app_id, ((ttl == 0) ? false : true));
        send_subscribe_ack_func(ret);
        return;
    }

    if (auto handler = handlers.second) {
        handler(app_id, ((ttl == 0) ? false : true), send_subscribe_ack_func);
        return;
    }

    send_subscribe_ack_func(true);
}

void ApplicationManager::on_subscribe_eventgroup_ack(std::shared_ptr<MessageSD> message) {
    auto& entry = message->entry(0);
    std::uint32_t req_id = message->get_request_id();
    std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
    std::uint16_t service_id = entry.get_service_id();
    std::uint16_t instance_id = entry.get_instance_id();
    std::uint32_t eventgroup_id = entry.get_event_group_id();
    std::uint32_t ttl = entry.get_ttl();

    std::uint8_t major = entry.get_major_version();

    LGSOMEIP_LOG_INFO << "ApplicationManager::on_subscribe_eventgroup_ack "
                      << format_service_instance_interface_major_version(service_id, instance_id, major) << " "
                      << format_named_id("EventGroupID", eventgroup_id, 4) << ", "
                      << format_named_id("AppID", app_id, 4) << ", TTL: " << ttl;

    std::unique_lock<std::mutex> lck_subevent(subscribe_event_list_mutex_);
    for (auto& item : subscribe_event_list_[service_id][instance_id]) {
        auto& eventgroup = item.second;
        auto it = eventgroup.find(eventgroup_id);
        if (it != eventgroup.end()) {
            if (it->second == SOMEIP_EVENT_SUBSCRIBE) {
                it->second = (ttl == SOMEIP_DEFAULT_TTL_OFF) ? SOMEIP_EVENT_SUBSCRIBE_NACK : SOMEIP_EVENT_SUBSCRIBE_ACK;
            }
        }
    }

#if defined(ENABLE_SOMEIP_IPC)
    if (is_internal_message_for_ipc(service_id, instance_id)) {
        packet_router_->add_ipc_route(service_id, instance_id, eventgroup_id, app_id, true);
    }
#endif // ENABLE_SOMEIP_IPC

    lck_subevent.unlock();

    // Callback SubscribeStatusHandler
    std::lock_guard<std::mutex> guard(requested_event_list_mutex_);
    auto& eventlist = requested_event_list_[service_id][instance_id];
    auto handle_subscribe_eventgroup_ack = [&](std::uint16_t service_id, std::uint16_t instance_id) -> void {
        auto found_service = subscription_status_handler_set_.find(service_id);
        if (found_service == subscription_status_handler_set_.end())
            return;

        auto found_instance = found_service->second.find(instance_id);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(eventgroup_id);
            if (found_eventgroup != found_instance->second.end()) {
                for (auto& event : eventlist) {
                    auto item = event.second.find(eventgroup_id);
                    if (item != event.second.end() && item->second == SOMEIP_EVENT_REQUEST) {
                        auto event_id = event.first;

                        auto found_event = found_eventgroup->second.find(event_id);
                        if (found_event != found_eventgroup->second.end()) {
                            auto handler = found_event->second.first;

                            if (handler != nullptr) {
                                LGSOMEIP_LOG_DEBUG
                                    << "ApplicationManager::on_subscribe_eventgroup_ack "
                                    << format_service_instance_event_id(service_id, instance_id, event_id) << " call "
                                    << "subscription_status_handler for "
                                    << format_named_id("EventGroupID", eventgroup_id, 4) << " TTL= " << ttl
                                    << " isSelective: " << (found_event->second.second ? "true" : "false");

                                if (ttl != SOMEIP_DEFAULT_TTL_OFF) {
                                    handler(service_id, instance_id, eventgroup_id, event_id, 0);
                                } else if (found_event->second.second == true) {
                                    handler(service_id, instance_id, eventgroup_id, event_id, 0xff);
                                } else {
                                    LGSOMEIP_LOG_WARN
                                        << "ApplicationManager::on_subscribe_eventgroup_ack "
                                        << format_service_instance_event_id(service_id, instance_id, event_id) << " "
                                        << format_named_id("EventGroupID", eventgroup_id, 4) << " TTL= " << ttl
                                        << " isSelective: " << (found_event->second.second ? "true" : "false")
                                        << " / does not notify the binding layer of the subscription state.";
                                }
                            }
                        }

                        auto its_any_event = found_eventgroup->second.find(SOMEIP_DEFAULT_ANY_EVENT);
                        if (its_any_event != found_eventgroup->second.end()) {
                            auto handler = its_any_event->second.first;

                            if (handler != nullptr) {
                                LGSOMEIP_LOG_DEBUG
                                    << "ApplicationManager::on_subscribe_eventgroup_ack "
                                    << format_service_instance_event_id(service_id, instance_id, event_id) << " call "
                                    << "subscription_status_handler for "
                                    << format_named_id("EventGroupID", eventgroup_id, 4) << " TTL= " << ttl;

                                if (ttl != SOMEIP_DEFAULT_TTL_OFF) {
                                    handler(service_id, instance_id, eventgroup_id, event_id, 0);
                                } else if (its_any_event->second.second == true) {
                                    handler(service_id, instance_id, eventgroup_id, event_id, 0xff);
                                } else {
                                    LGSOMEIP_LOG_WARN
                                        << "ApplicationManager::on_subscribe_eventgroup_ack "
                                        << format_service_instance_event_id(service_id, instance_id, event_id) << " "
                                        << format_named_id("EventGroupID", eventgroup_id, 4) << " TTL= " << ttl
                                        << " isSelective: " << (found_event->second.second ? "true" : "false")
                                        << " / does not notify the binding layer of the subscription state.";
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    handle_subscribe_eventgroup_ack(service_id, instance_id);
    handle_subscribe_eventgroup_ack(service_id, SOMEIP_DEFAULT_ANY_INSTANCE);
    handle_subscribe_eventgroup_ack(SOMEIP_DEFAULT_ANY_SERVICE, instance_id);
    handle_subscribe_eventgroup_ack(SOMEIP_DEFAULT_ANY_SERVICE, SOMEIP_DEFAULT_ANY_INSTANCE);
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (HANDLER MANAGEMENT)
// -----------------------------------------------------------------------------
void ApplicationManager::register_application_state_handler(application_state_handler_t handler) {
    std::lock_guard<std::mutex> guard(application_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_application_state_handler";

    application_state_handler_ = handler;
}

void ApplicationManager::unregister_application_state_handler() {
    std::lock_guard<std::mutex> guard(application_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_application_state_handler";

    application_state_handler_ = nullptr;
}

void ApplicationManager::register_message_handler(std::uint16_t service, std::uint16_t instance,
                                                  std::uint16_t method_id, message_handler_t handler,
                                                  bool is_provider) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_message_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("MethodID", method_id, 4) << " for "
                       << (is_provider ? "provider" : "consumer") << " is registered.";

    std::lock_guard<std::mutex> guard(message_handler_mutex_);

    auto& msg_handler_vector = message_handler_set_[service][instance][method_id][is_provider];

    msg_handler_vector.push_back(handler);
}

void ApplicationManager::unregister_message_handler(std::uint16_t service, std::uint16_t instance,
                                                    std::uint16_t method_id, bool is_provider) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_message_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("MethodID", method_id, 4) << " is unregistered.";

    std::lock_guard<std::mutex> guard(message_handler_mutex_);

    auto& msg_handler_vector = message_handler_set_[service][instance][method_id][is_provider];

    msg_handler_vector.clear();
}

void ApplicationManager::register_subscription_handler(std::uint16_t service, std::uint16_t instance,
                                                       std::uint16_t event_group, subscription_handler_t handler) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4)
                       << ", handler: " << (handler ? "set" : "not set") << " start";

    if (!handler) {
        unregister_subscription_handler(service, instance, event_group);
        return;
    }
    std::lock_guard<std::mutex> guard(subscription_handler_mutex_);
    auto handlers = subscription_handler_map_[service][instance].find(event_group);

    if (handlers == subscription_handler_map_[service][instance].end()) {
        subscription_handler_map_[service][instance][event_group] = std::make_pair(handler, nullptr);
    } else {
        subscription_handler_map_[service][instance][event_group] = std::make_pair(handler, handlers->second.second);
    }

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " is registered.";
}

void ApplicationManager::unregister_subscription_handler(std::uint16_t service, std::uint16_t instance,
                                                         std::uint16_t event_group) {
    std::lock_guard<std::mutex> guard(subscription_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " start";

    auto found_service = subscription_handler_map_.find(service);
    if (found_service != subscription_handler_map_.end()) {
        auto found_instance = found_service->second.find(instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(event_group);
            if (found_eventgroup != found_instance->second.end()) {
                found_eventgroup->second.first = nullptr;
                if (found_eventgroup->second.second == nullptr) {
                    found_instance->second.erase(event_group);

                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_subscription_handler "
                                       << format_service_instance_id(service, instance) << " "
                                       << format_named_id("EventGroupID", event_group, 4) << " is unregistered.";

                    if (found_instance->second.size() == 0) {
                        found_service->second.erase(instance);
                        if (found_service->second.size() == 0) {
                            subscription_handler_map_.erase(found_service);
                        }
                    }
                }
            }
        }
    }
}

void ApplicationManager::register_async_subscription_handler(std::uint16_t service, std::uint16_t instance,
                                                             std::uint16_t event_group,
                                                             async_subscription_handler_t handler) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_async_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4)
                       << ", handler: " << (handler ? "set" : "not set") << " start";

    if (!handler) {
        unregister_async_subscription_handler(service, instance, event_group);
        return;
    }

    std::lock_guard<std::mutex> guard(subscription_handler_mutex_);
    auto handlers = subscription_handler_map_[service][instance].find(event_group);

    if (handlers == subscription_handler_map_[service][instance].end()) {
        subscription_handler_map_[service][instance][event_group] = std::make_pair(nullptr, handler);
    } else {
        subscription_handler_map_[service][instance][event_group] = std::make_pair(handlers->second.first, handler);
    }

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_async_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " is registered.";
}

void ApplicationManager::unregister_async_subscription_handler(std::uint16_t service, std::uint16_t instance,
                                                               std::uint16_t event_group) {
    std::lock_guard<std::mutex> guard(subscription_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_async_subscription_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4) << " start";

    auto found_service = subscription_handler_map_.find(service);
    if (found_service != subscription_handler_map_.end()) {
        auto found_instance = found_service->second.find(instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(event_group);
            if (found_eventgroup != found_instance->second.end()) {
                found_eventgroup->second.second = nullptr;
                if (found_eventgroup->second.first == nullptr) {
                    found_instance->second.erase(event_group);

                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_async_subscription_handler "
                                       << format_service_instance_id(service, instance) << " "
                                       << format_named_id("EventGroupID", event_group, 4) << " is unregistered.";

                    if (found_instance->second.size() == 0) {
                        found_service->second.erase(instance);
                        if (found_service->second.size() == 0) {
                            subscription_handler_map_.erase(found_service);
                        }
                    }
                }
            }
        }
    }
}

void ApplicationManager::register_subscription_status_handler(std::uint16_t service, std::uint16_t instance,
                                                              std::uint16_t event_group, std::uint16_t event_id,
                                                              subscription_status_handler_t handler, bool selective) {
    std::lock_guard<std::mutex> guard(subscription_status_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_status_handler "
                       << format_service_instance_event_id(service, instance, event_id) << " "
                       << format_named_id("EventGroupID", event_group, 4)
                       << ", handler: " << (handler ? "set" : "not set")
                       << ", isSelective: " << (selective ? "true" : "false");

    if (handler) {
        subscription_status_handler_set_[service][instance][event_group][event_id] = std::make_pair(handler, selective);

        LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_status_handler / "
                           << format_service_instance_event_id(service, instance, event_id) << " "
                           << format_named_id("EventGroupID", event_group, 4) << " is registered.";
    } else {
        auto found_service = subscription_status_handler_set_.find(service);
        if (found_service != subscription_status_handler_set_.end()) {
            auto found_instance = found_service->second.find(instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(event_group);

                if (found_eventgroup != found_instance->second.end()) {
                    found_eventgroup->second.erase(event_id);

                    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_status_handler "
                                       << format_service_instance_id(service, instance) << " "
                                       << format_named_id("EventGroupID", event_group, 4) << " is unregistered.";

                    if (found_eventgroup->second.size() == 0) {
                        found_instance->second.erase(event_group);
                        if (found_instance->second.size() == 0) {
                            found_service->second.erase(instance);
                            if (found_service->second.size() == 0) {
                                subscription_status_handler_set_.erase(found_service);
                            }
                        }
                    }
                }
            }
        }
    }
}

void ApplicationManager::unregister_subscription_status_handler(std::uint16_t service, std::uint16_t instance,
                                                                std::uint16_t event_group, std::uint16_t event_id) {
    register_subscription_status_handler(service, instance, event_group, event_id, nullptr, false);
}

void ApplicationManager::register_subscription_error_handler(std::uint16_t service, std::uint16_t instance,
                                                             std::uint16_t event_group, error_handler_t handler) {
    std::lock_guard<std::mutex> guard(error_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_subscription_error_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4)
                       << ", handler: " << (handler ? "set" : "not set");

    error_handler_set_[service][instance][event_group] = handler;
}

void ApplicationManager::unregister_subscription_error_handler(std::uint16_t service, std::uint16_t instance,
                                                               std::uint16_t event_group) {
    std::lock_guard<std::mutex> guard(error_handler_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_subscription_error_handler "
                       << format_service_instance_id(service, instance) << " "
                       << format_named_id("EventGroupID", event_group, 4);

    error_handler_set_[service][instance][event_group] = nullptr;
}

void ApplicationManager::register_availability_handler(std::uint16_t service, std::uint16_t instance,
                                                       service_availability_handler_t handler,
                                                       std::uint8_t major_version, std::uint32_t minor_version) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_availability_handler "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << ", handler: " << (handler ? "set" : "not set");

    if (handler == nullptr) {
        LGSOMEIP_LOG_ERROR << "ApplicationManager::register_availability_handler "
                           << format_service_instance_interface_version(service, instance, major_version, minor_version)
                           << " ServiceAvailabilityHandler is nullptr.";
        return;
    }

    std::lock_guard<std::mutex> lck_service(service_management_mutex_);
    service_availability_handler_map_[service][instance][major_version][minor_version] = handler;

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::register_availability_handler / "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " is registered.";

    if (instance == SOMEIP_DEFAULT_ANY_INSTANCE) {
        for (auto& service_entry : request_service_list_) {
            if (service_entry.first != service)
                continue;
            for (auto& instance_entry : service_entry.second) {
                if (instance_entry.first == SOMEIP_DEFAULT_ANY_INSTANCE)
                    continue;

                bool state = false;
                for (auto& major : instance_entry.second) {
                    for (auto& minor : major.second) {
                        if (minor.second == SOMEIP_SERVICE_AVAILABLE)
                            state = true;
                    }
                }

                if (state == true) {
                    LGSOMEIP_LOG_INFO
                        << "ApplicationManager::register_availability_handler "
                        << format_service_instance_id(service_entry.first, instance_entry.first)
                        << " has already received OfferService, so call ServiceAvailabilityHandler right away.";

                    std::uint16_t service_id = service_entry.first;
                    std::uint16_t instance_id = instance_entry.first;
                    std::thread callback_thread(
                        [handler, service_id, instance_id]() { handler(service_id, instance_id, true); });
                    // For assigning thread name
                    pthread_setname_np(callback_thread.native_handle(), "SomeipAvailCB");
                    callback_thread.detach();
                }
            }
        }
    } else {
        // when to fail to find the state : return 0
        for (auto& service_entry : request_service_list_) {
            if (service_entry.first != service)
                continue;
            for (auto& instance_entry : service_entry.second) {
                if (instance_entry.first != instance && instance_entry.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                    continue;

                bool state = false;
                for (auto& major : instance_entry.second) {
                    for (auto& minor : major.second) {
                        if (minor.second == SOMEIP_SERVICE_AVAILABLE)
                            state = true;
                    }
                }

                if (state == true) {
                    LGSOMEIP_LOG_INFO
                        << "ApplicationManager::register_availability_handler "
                        << format_service_instance_id(service_entry.first, instance_entry.first)
                        << " has already received OfferService, so call ServiceAvailabilityHandler right away.";

                    std::uint16_t service_id = service_entry.first;
                    std::uint16_t instance_id = instance_entry.first;
                    std::thread callback_thread(
                        [handler, service_id, instance_id]() { handler(service_id, instance_id, true); });
                    // For assigning thread name
                    pthread_setname_np(callback_thread.native_handle(), "SomeipAvailCB");
                    callback_thread.detach();
                }
            }
        }
    }
}

void ApplicationManager::unregister_availability_handler(std::uint16_t service, std::uint16_t instance,
                                                         std::uint8_t major_version, std::uint32_t minor_version) {
    std::lock_guard<std::mutex> guard(service_management_mutex_);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::unregister_availability_handler "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version);

    service_availability_handler_map_[service][instance][major_version][minor_version] = nullptr;
}

void ApplicationManager::on_application_state(bool connected) {
    LGSOMEIP_LOG_INFO << "ApplicationManager::on_application_state / " << (connected ? "Connected" : "Disconnected");

    if (application_state_ != connected) {
        application_state_ = connected;
        std::unique_lock<std::mutex> lck(application_handler_mutex_);
        if (application_state_handler_) {
            application_state_handler_(connected ? SOMEIP_APPLICATION_REGISTERED : SOMEIP_APPLICATION_DEREGISTERED);
        }
        lck.unlock();

        if (connected == true) { // OnConnected
            {
                // Offer service again
                std::unique_lock<std::recursive_mutex> lck_offer(offer_service_list_mutex_);
                for (auto& service : offer_service_list_) {
                    for (auto& instance : service.second) {
                        for (auto& major : instance.second) {
                            for (auto& minor : major.second) {
                                if (minor.second == SOMEIP_SERVICE_OFFER) {
                                    LGSOMEIP_LOG_INFO << "ApplicationManager::on_application_state offer service "
                                                      << format_service_instance_interface_version(
                                                             service.first, instance.first, major.first, minor.first);
                                    send_offer_service(service.first, instance.first, major.first, minor.first,
                                                       0xFFFFFF);
                                }
                            }
                        }
                    }
                }
            }
            {
                // Find service again
                std::lock_guard<std::mutex> guard(service_management_mutex_);
                for (auto& service : request_service_list_) {
                    for (auto& instance : service.second) {
                        for (auto& majorversion : instance.second) {
                            for (auto& minorversion : majorversion.second) {
                                if (minorversion.second == SOMEIP_SERVICE_REQUEST) {
                                    LGSOMEIP_LOG_INFO
                                        << "ApplicationManager::on_application_state find service "
                                        << format_service_instance_interface_version(
                                               service.first, instance.first, majorversion.first, minorversion.first);
                                    send_find_service(service.first, instance.first, majorversion.first,
                                                      minorversion.first, SOMEIP_DEFAULT_TTL_ON);
                                }
                            }
                        }
                    }
                }
            }
        } else { // OnDisconnected
            {
                // Clear requested service
                std::lock_guard<std::mutex> guard(service_management_mutex_);
                for (auto& service : request_service_list_) {
                    for (auto& instance : service.second) {
                        for (auto& majorversion : instance.second) {
                            for (auto& minorversion : majorversion.second) {
                                minorversion.second = SOMEIP_SERVICE_REQUEST;
                            }
                        }
                    }
                }
            }
            {
                // Clear SubscribeEvent state
                std::unique_lock<std::mutex> lck_subevent(subscribe_event_list_mutex_);
                for (auto& service : subscribe_event_list_) {
                    for (auto& instance : service.second) {
                        for (auto& major : instance.second) {
                            for (auto& eventgroup : major.second) {
                                if (eventgroup.second != SOMEIP_EVENT_UNSUBSCRIBE)
                                    eventgroup.second = SOMEIP_EVENT_SUBSCRIBE;
                            }
                        }
                    }
                }
            }
            {
                // Notify service unavailability
                service_availability_handler_t handler = nullptr;
                for (auto& service : service_availability_handler_map_) {
                    for (auto& instance : service.second) {
                        for (auto& major : instance.second) {
                            for (auto& minor : major.second) {
                                handler = minor.second;
                                if (handler != nullptr) {
                                    handler(service.first, instance.first, false);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Private Method (SOME/IP-TP)
// -----------------------------------------------------------------------------
#if defined(ENABLE_SOMEIP_TP)
bool ApplicationManager::check_enabled_tp_message(MessageSOMEIP& message) {
    std::uint16_t svcid = static_cast<std::uint16_t>(message.get_message_id() >> 16);
    std::uint16_t methodid = static_cast<std::uint16_t>(message.get_message_id() & 0xffff);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::check_enabled_tp_message / " << format_named_id("ServiceID", svcid, 4)
                       << " " << format_named_id("MethodID", methodid, 4);

    auto service_config = configuration_->get_service_info(svcid, SOMEIP_DEFAULT_ANY_INSTANCE);
    if (service_config == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::check_enabled_tp_message / IPC communication "
                           << format_named_id("ServiceID", svcid, 4);
        return false;
    }

    // TP list: std::vector<std::uint16_t> tp_list_
    auto tp_list = service_config->get_tp_list();

    auto it = find(tp_list.begin(), tp_list.end(), methodid);
    if (it == tp_list.end()) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::check_enabled_tp_message / No method/event id in tp_list";
        return false;
    } else {
        for (auto i : tp_list) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::check_enabled_tp_message / "
                               << format_named_id("MethodID", i, 4);
        }
    }

    if (service_config->get_reliable_port() != 0 || service_config->get_unreliable_port() == 0)
        return false;

    if (message.get_length() <= SOMEIP_UDP_MAX_PAYLOAD_SIZE)
        return false;
    return true;
}

void ApplicationManager::on_message_tp(MessageSOMEIP& message) {
    static int offset_value = 1;
    static int bytes_of_offset = 0;
    static int order = 0;

    std::uint32_t msg_id = message.get_message_id();
    std::uint32_t req_id = message.get_request_id();
    std::uint32_t session_id = static_cast<std::uint32_t>(req_id & 0x0000ffff);
    std::uint8_t type = message.get_message_type();

    // Get TPHeader
    int max_payload_size = configuration_->get_max_payload_size();
    auto recv_payload = message.get_payload_type();

    std::uint32_t tp_header{0};
    get_byte_stream(&tp_header, (recv_payload->get_payload()));

    tp_header = static_cast<std::uint32_t>(tp_header >> 4);

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message_tp / " << format_named_id("MessageID", msg_id, 8) << " "
                       << format_named_id("RequestID", req_id, 8) << " " << format_named_id("SessionID", session_id, 8)
                       << " / " << format_named_id("MessageType", type, 2) << " / Len=" << message.get_length() << " / "
                       << format_named_id("TPOffset", tp_header, 8);

    switch (type) {
    case SOMEIP_MESSAGE_TYPE::TP_REQUEST:
        type = SOMEIP_MESSAGE_TYPE::REQUEST;
        break;
    case SOMEIP_MESSAGE_TYPE::TP_REQUEST_NO_RETURN:
        type = SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN;
        break;
    case SOMEIP_MESSAGE_TYPE::TP_RESPONSE:
        type = SOMEIP_MESSAGE_TYPE::RESPONSE;
        break;
    case SOMEIP_MESSAGE_TYPE::TP_ERROR:
        type = SOMEIP_MESSAGE_TYPE::ERROR;
        break;
    case SOMEIP_MESSAGE_TYPE::TP_NOTIFICATION:
        type = SOMEIP_MESSAGE_TYPE::NOTIFICATION;
        break;
    default:
        return;
    }

    std::shared_ptr<MessageSOMEIP> msg = nullptr;

    // New SOME/IP-TP segments first in
    if (msg_id != previous_message_id_) {
        tp_message_cache_ = std::make_shared<MessageSOMEIP>();
        msg = std::make_shared<MessageSOMEIP>();
        msg->set_message_id(msg_id);
        msg->set_interface_version(message.get_interface_version());
        msg->set_protocol_version(message.get_protocol_version());
        msg->set_request_id(req_id);
        msg->set_return_code(message.get_return_code());
        msg->set_message_type(type);

        tp_message_cache_ = msg;
        previous_message_id_ = msg_id;
        previous_session_id_ = session_id;
        order = 0;
    } else {
        msg = tp_message_cache_;
        if (order == 1) {
            offset_value = tp_header;
            bytes_of_offset = offset_value * 16;
        }
    }

    // The session ID must remain stable while the configured reassembly buffer is filled.
    if (max_payload_size > msg->get_length() && session_id == previous_session_id_ &&
        (tp_header / offset_value) == order) {
        std::uint8_t more = (*(recv_payload->get_payload() + 3)) & 0x01;
        msg->get_payload_type()->append(recv_payload->get_payload() + 4, recv_payload->get_length() - 4);
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message_tp / " << format_named_id("More", more, 2)
                           << " / Len=" << recv_payload->get_length();
        order++;

        if (more == 0x00) {
            msg->set_payload(msg->get_payload_type());

            // Compare the size of ressembled message and received message.
            // The size of received message can be calculated by Length.
            if ((tp_header / offset_value * bytes_of_offset) + (message.get_length() - 4 - 8) ==
                (msg->get_length() - 8)) {
                on_message(msg);

                LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message_tp / Success to reassembly";
            }
            tp_message_cache_ == nullptr;
            order = 0;
        }
    } else {
        // Clear the reassembly buffers
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message_tp / Clear the reassembly buffers";

        tp_message_cache_ == nullptr;
        order = 0;
    }
}

void ApplicationManager::send_tp_message(MessageSOMEIP& message) {
    std::uint32_t offset = 0;
    std::uint32_t remains = message.get_payload_type()->get_length();
    std::uint32_t reqid = message.get_request_id();

    std::uint8_t type = message.get_message_type();
    switch (type) {
    case SOMEIP_MESSAGE_TYPE::REQUEST:
        type = SOMEIP_MESSAGE_TYPE::TP_REQUEST;
        break;
    case SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN:
        type = SOMEIP_MESSAGE_TYPE::TP_REQUEST_NO_RETURN;
        break;
    case SOMEIP_MESSAGE_TYPE::RESPONSE:
        type = SOMEIP_MESSAGE_TYPE::TP_RESPONSE;
        break;
    case SOMEIP_MESSAGE_TYPE::ERROR:
        type = SOMEIP_MESSAGE_TYPE::TP_ERROR;
        break;
    case SOMEIP_MESSAGE_TYPE::NOTIFICATION:
        type = SOMEIP_MESSAGE_TYPE::TP_NOTIFICATION;
        break;
    default:
        return;
    }

    MessageSOMEIP msg;
    msg.set_message_id(message.get_message_id());
    msg.set_interface_version(message.get_interface_version());
    msg.set_protocol_version(message.get_protocol_version());
    msg.set_request_id(reqid);
    msg.set_return_code(message.get_return_code());
    msg.set_message_type(type);

    auto original_payload = message.get_payload();

    const std::uint32_t max_payload = SOMEIP_UDP_MAX_PAYLOAD_SIZE - 8;

    while (remains > 0) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::send_tp_message / remains : " << remains;

        bool more = remains > max_payload;
        std::shared_ptr<MessagePayload> payload = std::make_shared<MessagePayload>();
        std::uint32_t tp_header = (offset << 4) | more;

        payload->append(tp_header);
        payload->append(original_payload + offset * 16, (more ? 1392 : remains));

        offset += (max_payload) / 16; // 87 : (1400 - 8) / 16
        remains = (remains > max_payload) ? remains - max_payload : 0;

        msg.set_payload(payload);
        packet_router_->send_message(msg);
    }
}
#endif // ENABLE_SOMEIP_TP

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (Message Sender)
// -----------------------------------------------------------------------------
void ApplicationManager::send(std::shared_ptr<MessageSOMEIP> message, bool flush) {
    send(*message, flush);
}

void ApplicationManager::send(MessageSOMEIP& message, bool flush) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::send / " << format_named_id("MessageID", message.get_message_id(), 8)
                       << ", " << format_named_id("RequestID", message.get_request_id(), 8) << ", "
                       << format_named_id("MessageType", message.get_message_type(), 2) << ", "
                       << format_named_id("InstanceID", message.get_instance_id(), 4);

    std::uint32_t req_id = get_new_request_id(message.get_request_id(), message.get_message_type());
    if (req_id == 0) {
        LGSOMEIP_LOG_WARN << "ApplicationManager::send / wrong request id!";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kOutgoing, message.get_message_id(),
            SomeipPacketStatistics::kApplicationManagerWrongRequestId);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }
    message.set_request_id(req_id);

    // check isEnabledTP;
#if !defined(ENABLE_SOMEIP_IPC)
#if defined(ENABLE_SOMEIP_TP)
    if (check_enabled_tp_message(message)) {
        send_tp_message(message);
    } else {
        packet_router_->send_message(message);
    }
#else
    packet_router_->send_message(message);
#endif // ENABLE_SOMEIP_TP
#else
    std::uint16_t service_id = static_cast<std::uint16_t>(message.get_message_id() >> 16);
    std::uint16_t instance_id = message.get_instance_id();
    std::uint16_t method_id = static_cast<std::uint16_t>(message.get_message_id() & 0xffff);
    bool is_provider = message.get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST ? true : false;

    if (is_internal_message_for_ipc(service_id, instance_id)) {
        LGSOMEIP_LOG_DEBUG << "ApplicationManager::send / "
                           << format_service_instance_event_id(service_id, instance_id, method_id)
                           << (is_provider ? " Provider" : " Consumer");
        if (message.get_message_type() == SOMEIP_MESSAGE_TYPE::RESPONSE) {
            std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::send / ( IPC RESPONSE) / " << format_named_id("AppID", app_id, 4)
                               << " " << format_named_id("RequestID", req_id, 8);
            packet_router_->send_ipc_response_message(message, req_id, app_id);
        } else {
            packet_router_->send_ipc_message(message, is_provider);
        }
    } else {
#if defined(ENABLE_SOMEIP_TP)
        if (check_enabled_tp_message(message)) {
            send_tp_message(message);
        } else {
            packet_router_->send_message(message);
        }
#else
        packet_router_->send_message(message);
#endif // ENABLE_SOMEIP_TP
    }
#endif // ENABLE_SOMEIP_IPC

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
    SomeipPacketStatistics::get_instance().increase_received_packet(SomeipPacketStatistics::kOutgoing,
                                                                    message.get_message_id());
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
}

void ApplicationManager::notify(std::uint16_t service, std::uint16_t instance, std::uint16_t event_id,
                                std::shared_ptr<Payload> payload, std::uint16_t client, bool force, bool flush) const {
    event_manager_->notify(service, instance, event_id, payload, client, force, flush);
}

// -----------------------------------------------------------------------------
//  PacketRouter Proxy : Public Method (Message Receiver)
// -----------------------------------------------------------------------------
// Callback for Control Message
void ApplicationManager::on_message(std::shared_ptr<MessageSD> message) {
    for (auto& entry : message->entries()) {
        std::uint8_t type = entry.get_type();

        switch (type) {
        case SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, entry.get_service_id(),
                SomeipPacketStatistics::kSomeipSdFindServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            on_find_service(message);

            break;
        case SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, entry.get_service_id(),
                SomeipPacketStatistics::kSomeipSdOfferServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            on_offer_service(message);

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, entry.get_service_id(),
                SomeipPacketStatistics::kSomeipSdSubscribeReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            on_subscribe_eventgroup(message);

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, entry.get_service_id(),
                SomeipPacketStatistics::kSomeipSdSubscribeAckReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            on_subscribe_eventgroup_ack(message);

            break;
        default:
            LGSOMEIP_LOG_WARN << "ApplicationManager::on_message / NoType";
            break;
        }
    }
}

/*
   Return Codes in the Response Messages of methods shall
   be used to transport application errors and the response data of a method from the
   provider to the caller of a method.

   Explicit Error Messages shall be used to transport application
   errors and the response data or generic SOME/IP errors from the provider to the
   caller of a method.

   If more detailed error information need to be transmitted, the
   payload of the Error Message (Message Type 0x81) shall be filled with error specific
   data, e.g. an exception string. Error Messages shall be sent instead of Response
   Messages.

   Only responses (Response Messages (message type 0x80)
   and Error Messages (message type 0x81) shall use the return code field to carry a
   return code to the request (Message Type 0x00) they answer.

   All other messages than 0x80 and 0x81
   (see Chapter 4.1.1.6) shall set this field to 0x00.

   A SOME/IP error message (i.e. return code 0x01 - 0x1f)
   shall not be answered with an error message.

   The receiver of a SOME/IP message shall not return an error
   message for events/notifications.

   For Request/Response methods the error message shall
   copy over the fields of the SOME/IP header (i.e. Message ID, Request ID, and Interface
   Version) but not the payload. In addition Message Type and Return Code have
   to be set to the appropriate values.

   Error handling shall be based on the message type received
   (e.g. only methods can be answered with a return code) and shall be checked in a defined
   order of [].

*/

void ApplicationManager::send_error(std::shared_ptr<MessageSOMEIP> message, std::uint8_t return_code,
                                    std::uint8_t* error_message) {
    LGSOMEIP_LOG_DEBUG << "ApplicationManager::send_error / "
                       << format_named_id("MessageID", message->get_message_id(), 8) << ", "
                       << format_named_id("ReturnCode", return_code, 2) << ", errorMessage: " << error_message;

    if (error_message == nullptr) {
        message->set_payload(nullptr, 0);
        message->set_message_type(SOMEIP_MESSAGE_TYPE::ERROR);
    } else {
        message->set_payload(error_message, strlen((char*)error_message));
        message->set_message_type(SOMEIP_MESSAGE_TYPE::ERROR);
    }

    message->set_return_code(return_code);

    send(message);
}

#if !defined(ENABLE_SOMEIP_IPC)
std::vector<message_handler_t>
ApplicationManager::get_message_handler(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t method_id,
                                        std::uint8_t major_version, std::uint8_t type)
#else
std::vector<message_handler_t>
ApplicationManager::get_message_handler(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t method_id,
                                        std::uint8_t major_version, std::uint8_t type, std::uint16_t app_id,
                                        std::uint16_t request_id)
#endif // ENABLE_SOMEIP_IPC
{
    std::uint16_t serviceid = service_id;
    std::uint16_t methodid = method_id;
    std::uint16_t instanceid = instance_id;
    std::uint8_t major = major_version;
    bool is_provider = false;

    std::vector<message_handler_t> handler_vector;
    std::uint8_t error = 0;

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_message_handler / ServiceInfo "
                       << format_service_instance_id(serviceid, instanceid) << " "
                       << format_named_id("MethodID", methodid, 4) << " start";

    // check major version and IPC info
    if (type == SOMEIP_MESSAGE_TYPE::REQUEST || type == SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN) {
        is_provider = true;
        std::lock_guard<std::recursive_mutex> guard(offer_service_list_mutex_);

        auto service = offer_service_list_[service_id][instanceid][major];
        if (service.size() == 0) {
            service = offer_service_list_[service_id][instanceid][SOMEIP_DEFAULT_ANY_MAJOR];
            if (service.size() == 0) {
                error = SOMEIP_RETURN_CODE::E_WRONG_INTERFACE_VERSION;
                throw LSAR_APPLICATION_ERROR(error);
            }
            major = SOMEIP_DEFAULT_ANY_MAJOR;
        }

        if (service.begin()->second != SOMEIP_SERVICE_OFFER) {
            error = SOMEIP_RETURN_CODE::E_WRONG_INTERFACE_VERSION;
            throw LSAR_APPLICATION_ERROR(error);
        }

#if defined(ENABLE_SOMEIP_IPC)
        if (is_internal_message_for_ipc(serviceid, instanceid)) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_message_handler is_internal_message_for_ipc "
                               << format_service_instance_id(serviceid, instanceid) << " "
                               << format_named_id("MethodID", methodid, 4) << (is_provider ? " Provider" : " Consumer");
            packet_router_->add_ipc_route(serviceid, instanceid, methodid, app_id, false);
            packet_router_->on_internal_request(serviceid, instanceid, request_id, app_id);
        }
#endif // ENABLE_SOMEIP_IPC
    }

    // find message handler
    std::lock_guard<std::mutex> guard(message_handler_mutex_);
    for (auto& svc : message_handler_set_) {
        if (svc.first != serviceid && svc.first != SOMEIP_DEFAULT_ANY_SERVICE)
            continue;
        for (auto& ins : svc.second) {
            if (ins.first != instanceid && ins.first != SOMEIP_DEFAULT_ANY_INSTANCE)
                continue;
            for (auto& mth : ins.second) {
                if (mth.first == methodid || mth.first == SOMEIP_DEFAULT_ANY_METHOD) {
                    handler_vector = message_handler_set_[svc.first][ins.first][mth.first][is_provider];
                    if (!handler_vector.empty()) {
                        serviceid = svc.first;
                        instanceid = ins.first;
                        methodid = mth.first;
                        break;
                    }
                }
            }
        }
    }

    LGSOMEIP_LOG_DEBUG << "ApplicationManager::get_message_handler / ServiceInfo "
                       << format_service_instance_id(serviceid, instanceid) << " "
                       << format_named_id("MethodID", methodid, 4) << " " << (is_provider ? "Provider" : "Consumer");

    if (!handler_vector.empty()) {
        LGSOMEIP_LOG_DEBUG << " ==> Handler " << format_service_instance_id(serviceid, instanceid) << " "
                           << format_named_id("MethodID", methodid, 4) << ", "
                           << (is_provider ? "Provider, " : "Consumer, ") << handler_vector.size() << " Handlers";
    } else {
        LGSOMEIP_LOG_DEBUG << " : message handler is not registered!";
    }

    return handler_vector;
}

std::uint16_t ApplicationManager::get_new_session_id() {
    session_count_ = (session_count_ % 0xffff) + 1;
    return session_count_;
}

std::uint32_t ApplicationManager::get_new_request_id(std::uint32_t request_id, std::uint8_t type) {
    std::uint32_t clientid = static_cast<std::uint32_t>(get_application_id()) << 16;
    std::uint32_t sessionid = static_cast<std::uint32_t>(request_id & 0x0000ffff);

    if (type == SOMEIP_MESSAGE_TYPE::REQUEST) {
        if (sessionid == 0)
            sessionid = get_new_session_id();
        std::uint32_t req_id = clientid | sessionid;
        return req_id;
    } else if (type == SOMEIP_MESSAGE_TYPE::RESPONSE) {
        if (sessionid == 0)
            return 0;
    } else if (type == SOMEIP_MESSAGE_TYPE::NOTIFICATION) {
        if (sessionid == 0)
            return 0;
    }

    return request_id;
}

std::uint32_t ApplicationManager::get_sd_request_id() {
    std::uint32_t req_id = static_cast<std::uint32_t>(get_application_id()) << 16;
    return req_id;
}

// Callback for Data Message
void ApplicationManager::on_message(std::shared_ptr<MessageSOMEIP> message) {
    std::uint16_t serviceid = static_cast<std::uint16_t>(message->get_message_id() >> 16);
    std::uint16_t methodid = static_cast<std::uint16_t>(message->get_message_id() & 0xffff);
    std::uint8_t major = message->get_interface_version();
    std::uint8_t type = message->get_message_type();
    std::uint16_t instanceid = message->get_instance_id();
#if defined(ENABLE_SOMEIP_IPC)
    std::uint32_t req_id = message->get_request_id();
    std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
#endif // ENABLE_SOMEIP_IPC
    bool can_handle = false;

    // check the InstanceID
    if (instanceid == 0) {
        std::lock_guard<std::mutex> guard(message_handler_mutex_);
        auto instance_list = message_handler_set_.find(serviceid);
        if (instance_list == message_handler_set_.end()) {
            serviceid = SOMEIP_DEFAULT_ANY_SERVICE;
            instance_list = message_handler_set_.find(serviceid);
            if (instance_list == message_handler_set_.end()) {
                LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message / Invalid "
                                   << format_named_id("ServiceID", serviceid, 4);

                if (message->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
                    throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_METHOD);
                } else {
                    throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_NOT_OK);
                }
            }
        }

        for (auto it = instance_list->second.begin(); it != instance_list->second.end(); it++) {
            instanceid = it->first;
            if (!message_handler_set_[serviceid][instanceid][methodid][true].empty() ||
                !message_handler_set_[serviceid][instanceid][SOMEIP_DEFAULT_ANY_METHOD][true].empty() ||
                !message_handler_set_[serviceid][instanceid][methodid][false].empty() ||
                !message_handler_set_[serviceid][instanceid][SOMEIP_DEFAULT_ANY_METHOD][false].empty()) {
                message->set_instance_id(instanceid);
                can_handle = true;
                break;
            }
        }
        if (can_handle == false) {
            LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message / Handler "
                               << format_service_instance_id(serviceid, instanceid) << " "
                               << format_named_id("MethodID", methodid, 4) << " : message handler is not registered!";

            if (message->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
                throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_METHOD);
            } else {
                throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_NOT_OK);
            }
        }
    }

    if (type == SOMEIP_MESSAGE_TYPE::TP_REQUEST || type == SOMEIP_MESSAGE_TYPE::TP_REQUEST_NO_RETURN ||
        type == SOMEIP_MESSAGE_TYPE::TP_NOTIFICATION || type == SOMEIP_MESSAGE_TYPE::TP_RESPONSE ||
        type == SOMEIP_MESSAGE_TYPE::TP_ERROR) {
#if defined(ENABLE_SOMEIP_TP)
        on_message_tp(*message);
#endif // ENABLE_SOMEIP_TP
        return;
    }

    try {
#if !defined(ENABLE_SOMEIP_IPC)
        std::vector<message_handler_t> handler = get_message_handler(serviceid, instanceid, methodid, major, type);
#else
        std::vector<message_handler_t> handler =
            get_message_handler(serviceid, instanceid, methodid, major, type, app_id, req_id);
#endif // ENABLE_SOMEIP_IPC
        if (!handler.empty()) {
            for (auto& it : handler) {
                LGSOMEIP_LOG_DEBUG << "ApplicationManager::on_message "
                                   << format_service_instance_id(serviceid, instanceid) << " "
                                   << format_named_id("MethodID", methodid, 4) << " call the callback function.";

                // Push the callback method(it) to queue
                // and thread runs the method with message.
                thread_pool_->enqueue_job(it, message);
            }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kIncoming,
                                                                             message->get_message_id());
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        } else {
            LGSOMEIP_LOG_WARN << "ApplicationManager::on_message No message handler";
            if (message->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
                throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_METHOD);
            } else {
                throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_NOT_OK);
            }
        }
    } catch (const ApplicationErrorException& e) {
        if (message->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
            LGSOMEIP_LOG_WARN << "ApplicationManager::on_message / Send error message, error: " << e.what();
            send_error(message, e.get_error_code());
        } else {
            LGSOMEIP_LOG_WARN << "ApplicationManager::on_message / Ignore messge, error: " << e.what();
        }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message->get_message_id(),
            SomeipPacketStatistics::kApplicationManagerError);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

} // namespace lgsomeip
