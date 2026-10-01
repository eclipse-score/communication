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

#include <packetrouter/PacketRouterHost.h>
#include <runtime/ApplicationConstant.h>
#include <runtime/ServiceManager.h>

#include <message/MessageComposer.h>

#include <exception/Exception.h>
#include <utils/time/TimeCheck.h>
#include <utils/time/TimerMux.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <cmath>
#include <sstream>

// To write the current state of SOME/IP services to json files
#include "rapidjson/filewritestream.h"
#include <rapidjson/writer.h>
#include "rapidjson/prettywriter.h"
#include <sys/stat.h>
#include <sys/types.h>

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
#include <utils/statistics/SomeipPacketStatistics.h>
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

namespace lgsomeip {

// To handle ODR-use
const std::uint8_t ServiceManager::RETRY_CONNECTION_PERIOD;

ServiceManager::ServiceManager(std::string name, std::string config_path, std::shared_ptr<ServiceRouter> packet_router)
    : application_name_(name), application_id_(0), packet_router_(std::move(packet_router)) {
    if (packet_router_ == nullptr) {
        packet_router_ = std::make_shared<PacketRouterHost>(this);
    }
    configuration_ = std::make_shared<Configuration>(config_path);

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
    // loading packet fileters for protecting SOME/IP overload
    // Only someip-daemon loads and uses this configuration.
    configuration_->configure_someip_packet_filter();
#endif // ENABLE_SOMEIP_PACKET_FILTERING

    // Initialize gathered_subscribe_list_ and gathered_subscribe_ack_list_ which are going to be used in the
    // on_external_message() funciton
    gathered_subscribe_list_ = MessageBuilder::create<SOMEIPSD>();
    gathered_subscribe_ack_list_ = MessageBuilder::create<SOMEIPSD>();
}

ServiceManager::~ServiceManager() {
    join_add_connection_threads();
}

void ServiceManager::init() {
    packet_router_->init();

#if !defined(ENABLE_QNX_MESSAGE_PASSING)
    timer_ = std::make_shared<TimerMux>("Timer-Host");
    timer_->register_handler(std::bind(&ServiceManager::on_timer, this));
#endif // ENABLE_QNX_MESSAGE_PASSING
}

void ServiceManager::start() {
    packet_router_->start();

#if defined(ENABLE_QNX_MESSAGE_PASSING)
    packet_router_->message_passing_set_timer(1, SOMEIP_DEFAULT_TIMER_CYCLE, true,
                                              std::bind(&ServiceManager::on_timer, this));
#else
    timer_->start_listen(packet_router_->get_multiplexer());
    timer_->set_timer(SOMEIP_DEFAULT_TIMER_CYCLE, true);
#endif // ENABLE_QNX_MESSAGE_PASSING
}

void ServiceManager::stop() {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    packet_router_->message_passing_kill_timer(1);
#else  // ENABLE_QNX_MESSAGE_PASSING
    timer_->cancel_timer();
    timer_->stop_listen();
#endif // ENABLE_QNX_MESSAGE_PASSING

    join_add_connection_threads();

    if (packet_router_ != nullptr) {
        packet_router_->stop();
    }
}

void ServiceManager::on_timer() {
    // Send 1) CyclicMainOfferService 2)RepetitionOfferService 3)RepetitionFindService
    on_send_cyclic_offer_find_service();

    on_check_ttl_service();
    on_check_ttl_subscribe();
    on_send_magic_cookies();
}

void ServiceManager::on_send_cyclic_repetition_offer_service(std::shared_ptr<MessageSD>& message) {
    auto sd_config = get_configuration()->get_service_discovery_info();

    std::lock_guard<std::mutex> lock(repetition_offer_list_mutex_);

    for (auto& service : repetition_offer_list_) {
        std::uint16_t serviceid = service.first;
        for (auto& instance : service.second) {
            std::uint16_t instanceid = instance.first;

            // Send Offer message only on providing services in the point of server
            if (instance.second.app_id > 0 && instance.second.is_in_config_file == true) {
                auto service_config = get_configuration()->get_service_info(serviceid, instanceid);
                if (service_config == nullptr) {
                    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service Info "
                                       << format_service_instance_id(serviceid, instanceid)
                                       << " not found in configuration. Skip sending OfferService.";

                    continue;
                }

                // Send Offer service
                auto& item = instance.second;
                std::uint32_t state = item.state & 0xffff0000;
                std::uint32_t count = item.state & 0x0000ffff;
                std::uint32_t timer = item.timer;

                switch (state) {
                case SOMEIP_SERVICE_STATE_INITIAL: {
                    if (timer == 0) {
                        item.timer = sd_config->get_initial_delay_min();

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service/ Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / INITIAL WAIT PHASE";
                        break;
                    }

                    item.timer = (timer > SOMEIP_DEFAULT_TIMER_CYCLE) ? timer - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
                    if (item.timer == 0) {
                        // Add entry to message
                        SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
                        entry.set_service_id(serviceid);
                        entry.set_instance_id(instanceid);
                        entry.set_major_version(item.major);
                        entry.set_minor_version(item.minor);
                        entry.set_ttl(3);

                        std::uint16_t option_type = (get_configuration()->get_ip_type() == 6)
                                                        ? SOMEIP_SD_OPTION::IP6::TYPEID
                                                        : SOMEIP_SD_OPTION::IP4::TYPEID;
                        auto op_type = static_cast<std::uint8_t>(option_type);
                        SDOption option1[2]{op_type, op_type};
                        if (item.tcp_address != nullptr && item.udp_address != nullptr) {
                            option1[0].set_address_option(item.tcp_address);
                            option1[1].set_address_option(item.udp_address);
                            entry.set_option1st_count(2);
                        } else if (item.tcp_address != nullptr) {
                            option1[0].set_address_option(item.tcp_address);
                            entry.set_option1st_count(1);
                        } else if (item.udp_address != nullptr) {
                            option1[0].set_address_option(item.udp_address);
                            entry.set_option1st_count(1);
                        }

                        MessageComposer::add_entry(message, &entry, option1, nullptr);
                        // Added

                        item.state = SOMEIP_SERVICE_STATE_REPETITION;
                        item.timer = sd_config->get_repetitions_base_delay();

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / send initial offer service";
                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / change state from INITIAL WAIT PHASE to REPETITION PHASE";
                    }
                    break;
                }
                case SOMEIP_SERVICE_STATE_REPETITION: {
                    if (count >= sd_config->get_repetitions_max()) {
                        item.timer = sd_config->get_cyclic_offer_delay() / 2; // 1000 / 2
                        item.state = SOMEIP_SERVICE_STATE_MAIN;

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / change state REPETITION PHASE to MAIN PHASE";

                        break;
                    }

                    item.timer = (timer > SOMEIP_DEFAULT_TIMER_CYCLE) ? timer - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
                    if (item.timer == 0) {
                        // Add entry to message
                        SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
                        entry.set_service_id(serviceid);
                        entry.set_instance_id(instanceid);
                        entry.set_major_version(item.major);
                        entry.set_minor_version(item.minor);
                        entry.set_ttl(3);

                        std::uint16_t option_type = (get_configuration()->get_ip_type() == 6)
                                                        ? SOMEIP_SD_OPTION::IP6::TYPEID
                                                        : SOMEIP_SD_OPTION::IP4::TYPEID;
                        auto op_type = static_cast<std::uint8_t>(option_type);
                        SDOption option1[2]{op_type, op_type};
                        if (item.tcp_address != nullptr && item.udp_address != nullptr) {
                            option1[0].set_address_option(item.tcp_address);
                            option1[1].set_address_option(item.udp_address);
                            entry.set_option1st_count(2);
                        } else if (item.tcp_address != nullptr) {
                            option1[0].set_address_option(item.tcp_address);
                            entry.set_option1st_count(1);
                        } else if (item.udp_address != nullptr) {
                            option1[0].set_address_option(item.udp_address);
                            entry.set_option1st_count(1);
                        }

                        MessageComposer::add_entry(message, &entry, option1, nullptr);
                        // Added

                        item.state = item.state + 1;
                        const std::uint64_t base_delay = sd_config->get_repetitions_base_delay();
                        const std::uint32_t shift = count + 1;
                        const std::uint64_t multiplier =
                            shift >= 64 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << shift);
                        const std::uint64_t delay =
                            base_delay > 0 && multiplier > std::numeric_limits<std::uint64_t>::max() / base_delay
                                ? std::numeric_limits<std::uint64_t>::max()
                                : base_delay * multiplier;
                        item.timer = static_cast<std::uint32_t>(
                            std::min(delay, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / send repetition offer service";
                    }
                    break;
                }
                case SOMEIP_SERVICE_STATE_MAIN: {
                    item.timer = (timer > SOMEIP_DEFAULT_TIMER_CYCLE) ? timer - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
                    if (item.timer == 0) {
                        available_service_list_[serviceid][instanceid].state = SOMEIP_SERVICE_STATE_MAIN;
                        std::pair<std::uint16_t, std::uint16_t> del_offer_pair = std::make_pair(serviceid, instanceid);
                        deleted_repetition_offer_list_.push_back(del_offer_pair);

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / waited for CYCLIC_OFFER_DELAY / 2 before main offer service";
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    if (deleted_repetition_offer_list_.begin() != deleted_repetition_offer_list_.end()) {
        for (auto del_iter = deleted_repetition_offer_list_.begin(); del_iter != deleted_repetition_offer_list_.end();
             del_iter++) {
            std::uint16_t del_serviceid = (*del_iter).first;
            std::uint16_t del_instanceid = (*del_iter).second;

            repetition_offer_list_[del_serviceid].erase(del_instanceid);

            if (repetition_offer_list_[del_serviceid].empty()) {
                repetition_offer_list_.erase(del_serviceid);

                LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_offer_service / Service "
                                   << format_service_instance_id(del_serviceid, del_instanceid)
                                   << " / is erased in repetition_offer_list_";
            }
        }
        deleted_repetition_offer_list_.clear();
    }
}

void ServiceManager::on_send_cyclic_repetition_find_service(std::shared_ptr<MessageSD>& message) {
    auto sd_config = get_configuration()->get_service_discovery_info();

    std::lock_guard<std::mutex> lock(request_list_mutex_);

    for (auto& service : repetition_find_list_) {
        std::uint16_t serviceid = service.first;
        for (auto& instance : service.second) {
            std::uint16_t instanceid = instance.first;

            auto service_config = get_configuration()->get_service_info(serviceid, instanceid);
            if (service_config == nullptr) {
                LGSOMEIP_LOG_VERBOSE << "ServiceManager::on_send_cyclic_repetition_find_service / Service Info "
                                     << format_service_instance_id(serviceid, instanceid)
                                     << " not found in configuration. Skip sending FindService.";

                continue;
            }

            if (instanceid != SOMEIP_DEFAULT_ANY_INSTANCE && service_config->is_provider() == true)
                continue;

            // Send FindService only on first service
            // Not send FindService of all services duplicated
            auto& item_vector = instance.second;
            if (item_vector.begin() != item_vector.end()) {
                auto& item = item_vector.front();
                std::uint32_t state = item.state & 0xffff0000;
                std::uint32_t count = item.state & 0x0000ffff;
                std::uint32_t timer = item.timer;

                // construct find service list;
                switch (state) {
                case SOMEIP_SERVICE_STATE_INITIAL: {
                    if (timer == 0) {
                        item.timer = sd_config->get_initial_delay_min();

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / INITIAL WAIT PHASE";
                        break;
                    }

                    item.timer = (timer > SOMEIP_DEFAULT_TIMER_CYCLE) ? timer - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
                    if (item.timer == 0) {
                        // Add entry to message
                        SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
                        entry.set_service_id(serviceid);
                        entry.set_instance_id(instanceid);
                        entry.set_major_version(item.major);
                        entry.set_minor_version(item.minor);
                        entry.set_ttl(3);

                        MessageComposer::add_entry(message, &entry, nullptr, nullptr);
                        // Added

                        item.state = SOMEIP_SERVICE_STATE_REPETITION;
                        item.timer = sd_config->get_repetitions_base_delay();

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / send initial find service";
                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / change state from INITIAL WAIT PHASE to REPETITION PHASE";
                    }
                    break;
                }
                case SOMEIP_SERVICE_STATE_REPETITION: {
                    if (count >= sd_config->get_repetitions_max()) {
                        item.timer = 0;
                        item.state = SOMEIP_SERVICE_STATE_MAIN;
                        std::pair<std::uint16_t, uint16_t> del_find_pair = std::make_pair(serviceid, instanceid);
                        deleted_repetition_find_list_.push_back(del_find_pair);

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / change state REPETITION PHASE to MAIN PHASE";
                        break;
                    }

                    item.timer = (timer > SOMEIP_DEFAULT_TIMER_CYCLE) ? timer - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
                    if (item.timer == 0) {
                        // Add entry to message
                        SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
                        entry.set_service_id(serviceid);
                        entry.set_instance_id(instanceid);
                        entry.set_major_version(item.major);
                        entry.set_minor_version(item.minor);
                        entry.set_ttl(3);

                        MessageComposer::add_entry(message, &entry, nullptr, nullptr);
                        // Added

                        item.state = item.state + 1;
                        item.timer = sd_config->get_repetitions_base_delay() * std::pow(2, count + 1);

                        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                           << format_service_instance_id(serviceid, instanceid)
                                           << " / send repetition find service";
                    }
                    break;
                }
                case SOMEIP_SERVICE_STATE_MAIN:
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (deleted_repetition_find_list_.begin() != deleted_repetition_find_list_.end()) {
        for (auto del_iter = deleted_repetition_find_list_.begin(); del_iter != deleted_repetition_find_list_.end();
             ++del_iter) {
            std::uint16_t del_serviceid = (*del_iter).first;
            std::uint16_t del_instanceid = (*del_iter).second;

            repetition_find_list_[del_serviceid].erase(del_instanceid);

            if (repetition_find_list_[del_serviceid].empty()) {
                repetition_find_list_.erase(del_serviceid);
                LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_repetition_find_service / Service "
                                   << format_service_instance_id(del_serviceid, del_instanceid)
                                   << " / is erased in repetition_find_list_";
            }
        }
        deleted_repetition_find_list_.clear();
    }
}

void ServiceManager::on_send_cyclic_main_offer_service(std::shared_ptr<MessageSD>& message) {
    static std::int32_t offer_cycle =
        get_configuration()->get_service_discovery_info()->get_cyclic_offer_delay() - SOMEIP_DEFAULT_TIMER_CYCLE;
    if (offer_cycle > 0) {
        offer_cycle = (offer_cycle > SOMEIP_DEFAULT_TIMER_CYCLE) ? offer_cycle - SOMEIP_DEFAULT_TIMER_CYCLE : 0;
        return;
    } else
        offer_cycle =
            get_configuration()->get_service_discovery_info()->get_cyclic_offer_delay() - SOMEIP_DEFAULT_TIMER_CYCLE;

    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);

    for (auto& service : available_service_list_) {
        std::uint16_t serviceid = service.first;
        for (auto& instance : service.second) {
            std::uint16_t instanceid = instance.first;

            std::uint32_t state = instance.second.state & 0xffff0000;

            // Send Offer message only on providing services in the point of server and state SOMEIP_SERVICE_STATE_MAIN
            if (instance.second.app_id > 0 && instance.second.is_in_config_file == true &&
                state == SOMEIP_SERVICE_STATE_MAIN) {
                LGSOMEIP_LOG_DEBUG << "ServiceManager::on_send_cyclic_main_offer_service / Service "
                                   << format_service_instance_id(serviceid, instanceid) << " / send main offer service";

                auto& item = instance.second;

                // Add entry to message
                SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
                entry.set_service_id(serviceid);
                entry.set_instance_id(instanceid);
                entry.set_major_version(item.major);
                entry.set_minor_version(item.minor);
                entry.set_ttl(3);

                std::uint16_t option_type = (get_configuration()->get_ip_type() == 6) ? SOMEIP_SD_OPTION::IP6::TYPEID
                                                                                      : SOMEIP_SD_OPTION::IP4::TYPEID;
                auto op_type = static_cast<std::uint8_t>(option_type);
                SDOption option1[2]{op_type, op_type};
                if (item.tcp_address != nullptr && item.udp_address != nullptr) {
                    option1[0].set_address_option(item.tcp_address);
                    option1[1].set_address_option(item.udp_address);
                    entry.set_option1st_count(2);
                } else if (item.tcp_address != nullptr) {
                    option1[0].set_address_option(item.tcp_address);
                    entry.set_option1st_count(1);
                } else if (item.udp_address != nullptr) {
                    option1[0].set_address_option(item.udp_address);
                    entry.set_option1st_count(1);
                }

                MessageComposer::add_entry(message, &entry, option1, nullptr);
                // Added
            }
        }
    }
}

void ServiceManager::on_send_cyclic_offer_find_service() {
    std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    std::uint32_t len = 0;
    std::shared_ptr<lgsomeip::osabstraction::Address> sdaddr = nullptr;

    auto message = MessageBuilder::create<SOMEIPSD>();

    // 0. on_send_cyclic_main_offer_service
    on_send_cyclic_main_offer_service(message);

    // 1. on_send_cyclic_repetition_offer_service
    on_send_cyclic_repetition_offer_service(message);

    // 2. onSencCyclicRepetitionFindService
    on_send_cyclic_repetition_find_service(message);

    // If message is not empty, send SD message
    if (message->entries().size() > 0) {
        auto session_info =
            get_session_id_and_reboot_flag(get_configuration()->get_service_discovery_info()->get_multicast());

        message->set_request_id(static_cast<std::uint32_t>(session_info.first));
        // Add unicast flag
        message->set_flag(session_info.second | 0x40);

        MessageBuilder::build_byte_stream(send_buffer, &len, *message);
        packet_router_->send_external_sd_message(send_buffer, len, nullptr, LGSOMEIP_MULTI_ENDPOINT);
    }
}

void ServiceManager::on_check_ttl_service() {
    static std::int16_t ttl_timer = 1000;
    if (ttl_timer > 0) {
        ttl_timer -= SOMEIP_DEFAULT_TIMER_CYCLE;
        return;
    } else
        ttl_timer = 1000;

    // reduce TTL and when TTL reach at 0, remove service info.
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    for (auto& service : available_service_list_) {
        auto instance = std::begin(service.second);
        while (instance != std::end(service.second)) {
            std::uint16_t sid = service.first;
            std::uint16_t iid = instance->first;
            if (instance->second.ttl == 0) {
                LGSOMEIP_LOG_INFO << "ServiceManager::on_check_ttl_service " << format_service_instance_id(sid, iid)
                                  << " is removed due to TTL time out.";

                send_internal_offer_service_all(sid, iid, SOMEIP_DEFAULT_TTL_OFF);

                instance = service.second.erase(instance);
                packet_router_->remove_route(sid, iid);
            } else {
                if (instance->second.ttl != 0xffffff) {
                    instance->second.ttl--;

                    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_check_ttl_service "
                                       << format_service_instance_id(sid, iid)
                                       << " / ttl reduced = " << instance->second.ttl;
                }
                ++instance;
            }
        }
    }
}

void ServiceManager::on_check_ttl_subscribe() {
    static std::int16_t ttl_timer = 1000;
    if (ttl_timer > 0) {
        ttl_timer -= SOMEIP_DEFAULT_TIMER_CYCLE;
        return;
    } else
        ttl_timer = 1000;

    // reduce TTL and when TTL reach at 0, remove subscribe info.
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    for (auto& service : available_service_list_) {
        std::uint16_t sid = service.first;
        for (auto& instance : service.second) {
            std::uint16_t iid = instance.first;
            auto& eventgrouplist = instance.second.subscribe;
            for (auto& eventgroup : eventgrouplist) {
                std::uint16_t egid = eventgroup.first;

                auto& subscribelist = eventgroup.second;
                auto subscribe = std::begin(subscribelist);
                while (subscribe != std::end(subscribelist)) {
                    if (subscribe->ttl == 0) {
                        LGSOMEIP_LOG_INFO
                            << "ServiceManager::on_check_ttl_subscribe " << format_service_instance_id(sid, iid) << " "
                            << format_named_id("EventGroupID", egid, 4) << " is removed due to TTL time out.";

                        // Remove Routing Info
                        auto config_service_info = get_configuration()->get_service_info(sid, iid);
                        if (config_service_info != nullptr) {
                            // In case of external services
                            auto eventgroup = config_service_info->get_event_group(egid);
                            for (std::uint16_t event_id : *eventgroup) {
                                if (subscribe->app_id > 0) {
                                    packet_router_->remove_subscribe_route(sid, iid, event_id, subscribe->app_id);
                                } else {
                                    if (subscribe->tcp_address != nullptr) {
                                        packet_router_->remove_subscribe_route(sid, iid, event_id, 0,
                                                                               subscribe->tcp_address);
                                    }
                                    if (subscribe->udp_address != nullptr) {
                                        packet_router_->remove_subscribe_route(sid, iid, event_id, 0,
                                                                               subscribe->udp_address);
                                    }
                                }
                            }

                            subscribe = subscribelist.erase(subscribe);
                        } else {
                            // In case of internal services (IPC), do nothing since TTL of internal services has been
                            // set to 0xFFFFFF and then it never decreases.
                        }
                    } else {
                        LGSOMEIP_LOG_VERBOSE
                            << "ServiceManager::on_check_ttl_subscribe " << format_service_instance_id(sid, iid) << " "
                            << format_named_id("EventGroupID", egid, 4) << " / ttl reduced = " << subscribe->ttl;

                        if (subscribe->ttl != 0xffffff) {
                            subscribe->ttl--;
                        }
                        ++subscribe;
                    }
                }
            }
        }
    } // end - for (auto& service)
}

void ServiceManager::on_send_magic_cookies() {
    packet_router_->check_and_send_magic_cookies();
}

std::uint16_t ServiceManager::get_application_id() {
    return application_id_;
}

std::string ServiceManager::get_application_name() {
    return application_name_;
}

void ServiceManager::on_disconnected_service(std::shared_ptr<lgsomeip::osabstraction::Address> addr) {
    if (addr == nullptr) {
        LGSOMEIP_LOG_WARN << "ServiceManager::on_disconnected_service / addr nullptr";
    }

    LGSOMEIP_LOG_INFO << "ServiceManager::on_disconnected_service / [Addr:" << addr->to_string() << "]";

    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    for (auto service_it = available_service_list_.begin(); service_it != available_service_list_.end();) {
        std::uint16_t service_id = 0;
        auto& instance_map = service_it->second;
        for (auto& instance : instance_map) {
            std::uint16_t instance_id = instance.first;
            auto& service_info = instance.second;

            if (service_info.tcp_address != nullptr && addr != nullptr && *service_info.tcp_address == *addr) {
                service_id = service_it->first;

                // TCP server case
                if (service_info.app_id > 0) {
                    // remove route and send stop offer service
                    packet_router_->remove_route(service_id, instance_id);
                    send_internal_offer_service_all(service_id, instance_id, SOMEIP_DEFAULT_TTL_OFF);

                    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>> stop_offer_list;
                    stop_offer_list[service_id][instance_id] = &service_info;
                    send_external_offer_service(stop_offer_list, nullptr, SOMEIP_DEFAULT_TTL_OFF,
                                                LGSOMEIP_MULTI_ENDPOINT);
                }
                // TCP client case
                else {
                    // Remove Subscribe Information
                    auto& eventgroup_list = service_info.subscribe;
                    auto eventgroup_list_it = eventgroup_list.begin();
                    while (eventgroup_list_it != eventgroup_list.end()) {
                        auto& eventgroup_id = eventgroup_list_it->first;
                        auto& subscribe_list = eventgroup_list_it->second;

                        auto subscribe_it = subscribe_list.begin();
                        while (subscribe_it != subscribe_list.end()) {
                            auto& subscribe = *subscribe_it;

                            if (subscribe.tcp_address == addr) {
                                if (service_info.is_in_config_file == true) {
                                    send_subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                                              SOMEIP_DEFAULT_TTL_OFF, service_info.major, 0, true);
                                }
                                subscribe_it = subscribe_list.erase(subscribe_it);
                            } else {
                                ++subscribe_it;
                            }
                        }

                        if (subscribe_list.empty()) {
                            eventgroup_list_it = eventgroup_list.erase(eventgroup_list_it);
                        } else {
                            ++eventgroup_list_it;
                        }
                    }
                }
            }
        }

        if (service_id != 0) {
            service_it = available_service_list_.erase(service_it);
            repetition_offer_list_.erase(service_id);
        } else {
            ++service_it;
        }
    }
}

void ServiceManager::on_disconnected_application(std::uint16_t app_id) {
    LGSOMEIP_LOG_INFO << "ServiceManager::on_disconnected_application / " << format_named_id("AppID", app_id, 4);

    // Remove Available Service Information
    std::set<std::uint16_t> remove_set;
    std::unique_lock<std::recursive_mutex> available_lck(available_list_mutex_);
    for (auto& service : available_service_list_) {
        std::uint16_t service_id = service.first;
        for (auto& instance : service.second) {
            std::uint16_t instance_id = instance.first;
            auto& service_info = instance.second;
            if (service_info.app_id == app_id) {
                // If this app provides service, send stop offer and remove it from available list
                send_internal_offer_service_all(service_id, instance_id, SOMEIP_DEFAULT_TTL_OFF);

                if (service_info.is_in_config_file == true) {
                    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>> stop_offer_list;
                    stop_offer_list[service_id][instance_id] = &service_info;
                    send_external_offer_service(stop_offer_list, nullptr, SOMEIP_DEFAULT_TTL_OFF,
                                                LGSOMEIP_MULTI_ENDPOINT);
                }

                remove_set.insert(service_id);
            } else {
                // Remove Subscribe Information
                auto& eventgroup_list = service_info.subscribe;
                auto eventgroup_list_it = eventgroup_list.begin();
                while (eventgroup_list_it != eventgroup_list.end()) {
                    auto& eventgroup_id = eventgroup_list_it->first;
                    auto& subscribe_list = eventgroup_list_it->second;

                    auto subscribe_it = subscribe_list.begin();
                    while (subscribe_it != subscribe_list.end()) {
                        auto& subscribe = *subscribe_it;
                        if (subscribe.app_id == app_id) {
                            if (service_info.is_in_config_file == true) {
                                send_subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                                          SOMEIP_DEFAULT_TTL_OFF, service_info.major, 0, true);
                            }
                            subscribe_it = subscribe_list.erase(subscribe_it);
                        } else {
                            ++subscribe_it;
                        }
                    }

                    if (subscribe_list.empty()) {
                        eventgroup_list_it = eventgroup_list.erase(eventgroup_list_it);
                    } else {
                        ++eventgroup_list_it;
                    }
                }
            }
        }
    }

    for (auto id : remove_set) {
        available_service_list_.erase(id);

        // Erase service in repetition_offer_list_
        repetition_offer_list_.erase(id);
    }
    available_lck.unlock();

    std::lock_guard<std::mutex> request_lck(request_list_mutex_);
    // Remove Request Service Information
    for (auto& service : request_service_list_) {
        for (auto& instance : service.second) {
            auto& request_list = instance.second;
            auto iter = std::begin(request_list);
            while (iter != std::end(request_list)) {
                if (iter->app_id == app_id) {
                    request_list.erase(iter);
                    break;
                }
                ++iter;
            }
        }
    }

    // Remove Repeated Find Service Information
    for (auto& service : repetition_find_list_) {
        for (auto& instance : service.second) {
            auto& repetition_list = instance.second;
            auto iter = std::begin(repetition_list);
            while (iter != std::end(repetition_list)) {
                if (iter->app_id == app_id) {
                    repetition_list.erase(iter);
                    break;
                }
                ++iter;
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  ServiceManager : Callback for Control Message
// -----------------------------------------------------------------------------
void ServiceManager::on_internal_message(std::shared_ptr<MessageSD> message) {
    SDEntry* entry = nullptr;
    std::uint16_t service_id = 0;
    std::uint16_t instance_id = 0;
    std::uint16_t eventgroup_id = 0;
    std::uint32_t minor = 0;
    std::uint8_t major = 0;
    std::uint32_t ttl = 0;

    std::uint32_t req_id = message->get_request_id();
    std::uint32_t len = message->entries().size();
    for (std::uint32_t i = 0; i < len; i++) {
        entry = &message->entry(i);
        service_id = entry->get_service_id();
        instance_id = entry->get_instance_id();
        major = entry->get_major_version();
        ttl = entry->get_ttl();

        switch (entry->get_type()) {
        case SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdFindServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            minor = entry->get_minor_version();
            on_internal_find_service(service_id, instance_id, major, minor, ttl, req_id);

            break;
        case SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdOfferServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            minor = entry->get_minor_version();
            on_internal_offer_service(service_id, instance_id, major, minor, ttl, req_id);

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdSubscribeReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            eventgroup_id = entry->get_event_group_id();
            try {
                check_subscribe_error(entry, nullptr, nullptr, "");
                on_internal_subscribe_eventgroup(service_id, instance_id, eventgroup_id, major, ttl, req_id);
            } catch (const SubscribeErrorException& e) {
                // std::cerr << e.what();
                std::uint16_t app_id = static_cast<std::uint16_t>(req_id >> 16);
                send_subscribe_eventgroup_ack(service_id, instance_id, eventgroup_id, SOMEIP_DEFAULT_TTL_OFF, major,
                                              app_id);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kOutgoing, service_id,
                    SomeipPacketStatistics::kSomeipSdSubscribeCheckError);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
            }

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdSubscribeAckReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            eventgroup_id = entry->get_event_group_id();
            on_subscribe_eventgroup_ack(service_id, instance_id, eventgroup_id, major, ttl, true);

            break;
        default:
            break;
        }
    }
}

void ServiceManager::on_external_message(std::shared_ptr<Endpoint> endpoint, std::shared_ptr<MessageSD> message,
                                         bool reboot) {
    SDEntry* entry = nullptr;
    SDOption* option1 = nullptr;
    SDOption* option2 = nullptr;

    std::uint16_t service_id = 0;
    std::uint16_t instance_id = 0;
    std::uint16_t eventgroup_id = 0;
    std::uint8_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t ttl = 0;

    std::uint32_t reqid = message->get_request_id();
    std::uint8_t flag = message->get_flag();
    bool is_flag_unicast_set = (flag & 0x40) != 0;

    std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address = nullptr;
    std::shared_ptr<lgsomeip::osabstraction::Address> udp_address = nullptr;
    std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr;

    std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>> mcast_vector;

    std::shared_ptr<lgsomeip::osabstraction::Address> sender_address = nullptr;
    sender_address = endpoint->get_sender_address();
    bool multi = false;
    std::string sender_ip_address;

    if (sender_address != nullptr) {
        sender_ip_address = sender_address->get_ip_address();
        multi = endpoint->get_socket()->get_src_address()->is_multicast();

        if (!check_sd_message(message, sender_ip_address))
            return;

        // In the case of Reboot
        // Remove the service info and renew TCP connection
        if (reboot) {
            remove_service_info(sender_address);
            packet_router_->reboot_route(sender_address);
        }
    }

    // Clear gathered_subscribe_list_, gathered_subscribe_ack_list_ and gathered_offer_list_
    gathered_subscribe_list_->entries().clear();
    gathered_subscribe_list_->options().clear();
    gathered_subscribe_ack_list_->entries().clear();
    gathered_subscribe_ack_list_->options().clear();
    gathered_offer_list_.clear();
    SDEntry ack_entry(SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);

    std::uint32_t len = message->entries().size();
    std::uint8_t option1_index = 0;

    for (std::uint32_t i = 0; i < len; i++) {
        entry = &message->entry(i);
        service_id = entry->get_service_id();
        instance_id = entry->get_instance_id();
        major = entry->get_major_version();
        ttl = entry->get_ttl();

        std::uint8_t option1st_count = entry->get_option1st_count();
        std::uint8_t option2nd_count = entry->get_option2nd_count();
        option1 = (option1st_count > 0) ? &message->option(entry->get_option1st_index()) : nullptr;
        option2 = (option2nd_count > 0) ? &message->option(entry->get_option2nd_index()) : nullptr;
        if (option1 == nullptr && option2 == nullptr) {
            if (message->options().size() > 0) {
                option1 = &message->option(0);
                option1st_count = 1;
            }
        }

        // Get endpoints (tcp_address or udp_address) from the address options
        bool find_option_addr_ok =
            find_option_address(tcp_address, udp_address, option1, option1st_count, option2, option2nd_count);

        switch (entry->get_type()) {
        case SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdFindServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            minor = entry->get_minor_version();
            on_external_find_service(service_id, instance_id, major, minor, ttl, reqid, is_flag_unicast_set);

            break;
        case SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdOfferServiceReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            // Offer Service entries shall always reference at least an IPv4 or IPv6 Endpoint Option
            //  to signal how the service is reachable.
            if (tcp_address == nullptr && udp_address == nullptr) {
                LGSOMEIP_LOG_WARN
                    << "ServiceManager::on_external_message / There is no endpoint in OfferService Message. "
                    << format_service_instance_interface_major_version(service_id, instance_id, major) << " "
                    << format_named_id("EventGroupID", eventgroup_id, 4) << " from " << sender_ip_address;

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdOfferServiceNoEndpoint);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

                break;
            }
            minor = entry->get_minor_version();
            try {
                // A SubscribeEventgroup message to the OfferService will be gathered during processing the
                // on_external_offer_service() function, and then gathered_subscribe_list_ will be sent at the end of
                // this function.
                on_external_offer_service(service_id, instance_id, major, minor, ttl, tcp_address, udp_address,
                                          sender_address);
            } catch (const ConfigurationErrorException& e) {
                LGSOMEIP_LOG_INFO << "ServiceManager::on_external_message / Error message: " << e.what();

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdOfferServiceInvalidConfig);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
            }

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdSubscribeReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            eventgroup_id = entry->get_event_group_id();

            ack_entry.set_service_id(service_id);
            ack_entry.set_instance_id(instance_id);
            ack_entry.set_major_version(major);
            ack_entry.set_event_group_id(eventgroup_id);

            // If there is error on options, just send NACK (ETS_117)
            if (!find_option_addr_ok) {
                LGSOMEIP_LOG_WARN
                    << "ServiceManager::on_external_message / Subscribe Error cannot find any AddrOptions "
                    << format_service_instance_interface_major_version(service_id, instance_id, major) << " "
                    << format_named_id("EventGroupID", eventgroup_id, 4) << " from " << sender_ip_address;
                ack_entry.set_ttl(SOMEIP_DEFAULT_TTL_OFF);
                // Gather a SubscribeEventgroupAck message to send it together in a packet
                gathered_subscribe_ack_list_->entries().push_back(ack_entry);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdSubscribeNoAddrOption);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

                break;
            }

            try {
                check_subscribe_error(entry, tcp_address, udp_address, sender_ip_address);

                // Only if this eventgroup has been already subscribed from application, the SubscribeEventGroupAck
                // message can be gathered in a packet.
                struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);
                bool should_sent_application = true;
                if (service_info != nullptr && service_info->app_id != 0 && ttl != SOMEIP_DEFAULT_TTL_OFF) {
                    auto subscribe_map_it = service_info->subscribe.find(eventgroup_id);
                    if (subscribe_map_it != service_info->subscribe.end()) {
                        auto& subscribe_list = subscribe_map_it->second;
                        auto subscribe = std::begin(subscribe_list);
                        while (subscribe != std::end(subscribe_list)) {
                            if (subscribe->state == SubscribeState::SUBSCRIBED &&
                                subscribe->ip_address == sender_ip_address) {
                                LGSOMEIP_LOG_DEBUG
                                    << "ServiceManager::on_external_message / Subscribe "
                                    << format_service_instance_interface_major_version(service_id, instance_id, major)
                                    << " " << format_named_id("EventGroupID", eventgroup_id, 4) << " TTL=" << ttl
                                    << " from " << sender_ip_address;
                                // Update subscribe info
                                subscribe->ttl = ttl;
                                subscribe->request_id_received = reqid;

                                // Get Multicast Option corresponding to the serviceID, instnaceID, and eventgroupID
                                auto service_config = get_configuration()->get_service_info(service_id, instance_id);
                                auto eventgroup_object = service_config->get_event_group_object(eventgroup_id);
                                std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr;

                                if ((service_config->get_multicast_address())->size() > 0) {
                                    if (eventgroup_object->is_multicast()) {
                                        multicast = eventgroup_object->get_multicast_address();
                                        if (multicast == nullptr) {
                                            multicast = (service_config->get_multicast_address()->operator[](0));
                                        }
                                    }
                                }

                                // Set Multicast Option to SubscribeAck Message.
                                if (multicast != nullptr) {
                                    SDOption option(SOMEIP_SD_OPTION::IP4MULTI::TYPEID);
                                    if (get_configuration()->get_ip_type() == 6) {
                                        option.set_type(SOMEIP_SD_OPTION::IP6MULTI::TYPEID);
                                    }

                                    // Check if same Multicast Option has been entered before.
                                    // Multicast Options are stored in mcastVector.
                                    auto mcast_it = std::find_if(
                                        std::begin(mcast_vector), std::end(mcast_vector),
                                        [&multicast](std::shared_ptr<lgsomeip::osabstraction::Address>& i) {
                                            return *multicast == *i;
                                        });

                                    // 1) New Multicast Option
                                    if (mcast_it == std::end(mcast_vector)) {
                                        LGSOMEIP_LOG_DEBUG
                                            << "ServiceManager::on_external_message / Set Multicast Option to "
                                            << "SubscribeAck "
                                            << format_service_instance_interface_major_version(service_id, instance_id,
                                                                                               major)
                                            << " " << format_named_id("EventGroupID", eventgroup_id, 4) << " , "
                                            << multicast->to_string() << " , set new multicast option";

                                        mcast_vector.push_back(multicast);
                                        option.set_address_option(multicast);
                                        gathered_subscribe_ack_list_->options().push_back(option);
                                        ack_entry.set_option1st_index(option1_index++);
                                        ack_entry.set_option1st_count(gathered_subscribe_ack_list_->options().size());
                                    } else { // 2) Duplicated Multicast Option
                                        // Calculate index and set the index to 1stIndex
                                        std::uint8_t multicast_index = 0;
                                        multicast_index = std::distance(std::begin(mcast_vector), mcast_it);
                                        ack_entry.set_option1st_index(multicast_index);
                                        ack_entry.set_option1st_count(gathered_subscribe_ack_list_->options().size());
                                        LGSOMEIP_LOG_DEBUG
                                            << "ServiceManager::on_external_message / Set Multicast Option to "
                                            << "SubscribeAck "
                                            << format_service_instance_interface_major_version(service_id, instance_id,
                                                                                               major)
                                            << " " << format_named_id("EventGroupID", eventgroup_id, 4) << " , "
                                            << multicast->to_string() << " , set index as stored multicast option";
                                    }
                                } else {
                                    ack_entry.set_option1st_count(0);
                                    ack_entry.set_option1st_index(0);
                                }

                                ack_entry.set_ttl(ttl);
                                // Gather a SubscribeEventgoupAck message to send it together in a packet
                                gathered_subscribe_ack_list_->entries().push_back(ack_entry);
                                should_sent_application = false;

                                break;
                            }
                            ++subscribe;
                        }
                    }
                }

                // In the case of first Subscribe message from external,
                // the value of shouldSentApplication is set to true.
                // (The daemon needs to notify the app that Subscribe message has arrived.)
                // After second Subscribe message, SubscribeAck is sent from someip-daemon directly.
                if (should_sent_application == true) {
                    on_external_subscribe_eventgroup(service_id, instance_id, eventgroup_id, major, ttl, reqid,
                                                     tcp_address, udp_address);
                }
            } catch (const SubscribeErrorException& e) {
                // std::cerr << e.what();
                LGSOMEIP_LOG_WARN << "ServiceManager::on_external_message / Subscribe Error "
                                  << format_service_instance_interface_major_version(service_id, instance_id, major)
                                  << " " << format_named_id("EventGroupID", eventgroup_id, 4) << " from "
                                  << sender_ip_address << " Error: " << e.what();
                ack_entry.set_ttl(SOMEIP_DEFAULT_TTL_OFF);
                // Gather a SubscribeEventgroupAck message to send it together in a packet
                gathered_subscribe_ack_list_->entries().push_back(ack_entry);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdSubscribeCheckError);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
            }

            break;
        case SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID:
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdSubscribeAckReceived);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            eventgroup_id = entry->get_event_group_id();
            if (entry->get_option1st_count() == 1 && message->options().size() == 1) {
                multicast = message->option(0).get_address_option();
                if (multicast->is_multicast() == false)
                    multicast == nullptr;
            }

            on_subscribe_eventgroup_ack(service_id, instance_id, eventgroup_id, major, ttl, false, multicast);

            break;
        default:
            // TODO : create an error message.
            break;
        }
    }

    // Send the gathered OfferService messages in a packet
    if (!gathered_offer_list_.empty()) {
        auto rev_addr = (is_flag_unicast_set == true) ? sender_address : nullptr;
        send_external_offer_service(gathered_offer_list_, rev_addr, 3, multi);
    }
    // Send the gathered SubscribeEventgroupAck messages in a packet
    if (!gathered_subscribe_ack_list_->entries().empty()) {
        mcast_vector.clear();

        // Set the number of 1st options as final size of options.
        for (auto& itr : gathered_subscribe_ack_list_->entries()) {
            if (itr.get_option1st_count() != 0) {
                itr.set_option1st_count(gathered_subscribe_ack_list_->options().size());
            }
        }

        std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
        std::uint32_t len;

        auto session_info = get_session_id_and_reboot_flag(sender_ip_address);
        gathered_subscribe_ack_list_->set_request_id(static_cast<std::uint32_t>(session_info.first));
        // Unicast Flag
        gathered_subscribe_ack_list_->set_flag(session_info.second | 0x40);

        MessageBuilder::build_byte_stream(send_buffer, &len, *gathered_subscribe_ack_list_);
        packet_router_->send_external_sd_message(send_buffer, len, sender_address, multi);
    }
    // Send the gathered SubscribeEventgroup messages in a packet
    if (!gathered_subscribe_list_->entries().empty()) {
        std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
        std::uint32_t len;

        auto session_info = get_session_id_and_reboot_flag(sender_ip_address);
        gathered_subscribe_list_->set_request_id(static_cast<std::uint32_t>(session_info.first));
        // Unicast Flag
        gathered_subscribe_list_->set_flag(session_info.second | 0x40);

        MessageBuilder::build_byte_stream(send_buffer, &len, *gathered_subscribe_list_);
        packet_router_->send_external_sd_message(send_buffer, len, sender_address, multi);
    }
}

bool ServiceManager::check_sd_message(std::shared_ptr<MessageSD> message, std::string sender_ip_address) {
    std::uint32_t len = message->entries().size();

    SDEntry* entry = nullptr;

    std::uint16_t service_id = 0;
    std::uint16_t instance_id = 0;
    std::uint8_t major = 0;
    std::uint16_t eventgroup_id = 0;
    std::uint32_t reqid = 0;

    int index1;
    int index2;
    int options;

    // Case1. lengh of entry is 0 or exceeds the SD message size
    if (len == 0) {
        LGSOMEIP_LOG_WARN << "ServiceManager::onCheckSDMessage / no entry";
        return false;
    }

    // Case2. length of entry is wrong(not multiple of 16)
    entry = &message->entry(0);
    if (entry->get_wrong_length() && entry->get_type() == SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID) {
        // We received a subscription for a wrong length of entry SD message.
        // --> send Nack with serviceID, instanceID, major, eventgroupID, reqid 0.
        LGSOMEIP_LOG_INFO << "ServiceManager::onCheckSDMessage / wrong length of entry";
        send_subscribe_eventgroup_ack(service_id, instance_id, eventgroup_id, SOMEIP_DEFAULT_TTL_OFF, major,
                                      (reqid >> 16), nullptr, sender_ip_address);
        return false;
    }

    // check existing of options for each entities
    for (std::uint32_t i = 0; i < len; i++) {
        entry = &message->entry(i);
        index1 = entry->get_option1st_index() + entry->get_option1st_count();
        index2 = entry->get_option2nd_index() + entry->get_option2nd_count();
        options = message->options().size();

        if (options < index1 || options < index2) {
            // the received SD message is wrong format!
            if (entry->get_type() == SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID) {
                service_id = entry->get_service_id();
                instance_id = entry->get_instance_id();
                major = entry->get_major_version();
                eventgroup_id = entry->get_event_group_id();
                reqid = message->get_request_id();
                send_subscribe_eventgroup_ack(service_id, instance_id, eventgroup_id, SOMEIP_DEFAULT_TTL_OFF, major,
                                              (reqid >> 16), nullptr, sender_ip_address);
            }

            LGSOMEIP_LOG_INFO << "ServiceManager::onCheckSDMessage / SD Message(from " << sender_ip_address
                              << ") is discarded!";

            return false;
        }
    }

    return true;
}

void ServiceManager::on_internal_find_service(std::uint16_t service_id, std::uint16_t instance_id,
                                              std::uint8_t major_version, std::uint32_t minor_version,
                                              std::uint32_t ttl, std::uint32_t request_id) {
    std::uint16_t app_id = static_cast<std::uint16_t>(request_id >> 16);

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_internal_find_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << " TTL=" << ttl << " from " << format_named_id("AppID", app_id, 4);

    // Register Find Service on Request Service List
    std::lock_guard<std::mutex> lock(request_list_mutex_);
    auto& req_app_list = request_service_list_[service_id][instance_id];
    auto reqitem = std::find_if(std::begin(req_app_list), std::end(req_app_list),
                                [&](struct RequestedService& i) { return i.app_id == app_id; });

    // Register Cyclic Repetition Find Service on Request Service List
    ServiceInfo* config = configuration_->get_service_info(service_id, instance_id);
    bool is_in_config = (config != nullptr);

    if (ttl > 0) {
        // check pre-requested service & register request info
        if (reqitem == std::end(req_app_list)) {
            req_app_list.emplace_back(app_id, major_version, minor_version);
        }

        std::uint32_t minimum_minor_version = 0;
        bool has_minimum_minor = false;

        // Find Service on Available Service
        if (instance_id != SOMEIP_DEFAULT_ANY_INSTANCE) {
            auto service_info = find_available_service_instance(service_id, instance_id);
            auto service_config = configuration_->get_service_info(service_id, instance_id);
            if (service_config != nullptr) {
                minimum_minor_version = service_config->get_minimum_minor_version();
                has_minimum_minor = service_config->has_minimum_minor_version();
            }

            if (service_info != nullptr) {
                if (check_service_version(major_version, service_info->major, minor_version, service_info->minor,
                                          minimum_minor_version, has_minimum_minor)) {
                    req_app_list.back().state = SOMEIP_SERVICE_STATE_MAIN;
                    packet_router_->set_instance_id(service_id, instance_id, app_id);
                    // send "offer service message" to requesting app.
                    send_internal_offer_service(service_id, instance_id, SOMEIP_DEFAULT_TTL_ON, service_info->major,
                                                service_info->minor, app_id);
                }
            } else { // Only register one external FindService in repetition_find_list_
                if (is_in_config) {
                    auto& repetition_app_list = repetition_find_list_[service_id][instance_id];
                    auto repetition_item = std::find_if(std::begin(repetition_app_list), std::end(repetition_app_list),
                                                        [&](struct RequestedService& i) { return i.app_id == app_id; });

                    if (repetition_item == std::end(repetition_app_list)) {
                        repetition_app_list.emplace_back(app_id, major_version, minor_version);
                    }
                }
            }
        } else {
            auto instance_list = find_available_service_instance_all(service_id);
            if (!instance_list.empty()) {
                for (auto& instance : instance_list) {
                    auto service_config = configuration_->get_service_info(service_id, instance.first);

                    if (service_config != nullptr) {
                        minimum_minor_version = service_config->get_minimum_minor_version();
                        has_minimum_minor = service_config->has_minimum_minor_version();
                    }

                    if (check_service_version(major_version, instance.second->major, minor_version,
                                              instance.second->minor, minimum_minor_version, has_minimum_minor)) {
                        req_app_list.back().state = SOMEIP_SERVICE_STATE_MAIN;
                        packet_router_->set_instance_id(service_id, instance.first, app_id);
                        send_internal_offer_service(service_id, instance.first, SOMEIP_DEFAULT_TTL_ON,
                                                    instance.second->major, instance.second->minor, app_id);
                    }
                }
            } else { // Only register one external FindService in repetition_find_list_
                if (is_in_config) {
                    auto& repetition_app_list = repetition_find_list_[service_id][instance_id];
                    auto repetition_item = std::find_if(std::begin(repetition_app_list), std::end(repetition_app_list),
                                                        [&](struct RequestedService& i) { return i.app_id == app_id; });

                    if (repetition_item == std::end(repetition_app_list)) {
                        repetition_app_list.emplace_back(app_id, major_version, minor_version);
                    }
                }
            }
        }
    } else {
        // Stop Request Service
        if (reqitem != std::end(req_app_list)) {
            req_app_list.erase(reqitem);
        }

        // Erase the findServices from repetition_find_list_
        auto it = repetition_find_list_.find(service_id);
        if (it != repetition_find_list_.end()) {
            auto& instance_repetition_list = it->second;
            auto it2 = instance_repetition_list.find(instance_id);
            if (it2 != instance_repetition_list.end()) {
                auto& repetition_list = it2->second;
                auto repetition_item = std::find_if(std::begin(repetition_list), std::end(repetition_list),
                                                    [&](struct RequestedService& i) { return i.app_id == app_id; });

                if (repetition_item != repetition_list.end()) {
                    repetition_list.erase(repetition_item);
                }
            }
        }
    }
}

void ServiceManager::on_external_find_service(std::uint16_t service_id, std::uint16_t instance_id,
                                              std::uint8_t major_version, std::uint32_t minor_version,
                                              std::uint32_t ttl, std::uint32_t request_id, bool unicast_flag_set) {
    std::shared_ptr<lgsomeip::osabstraction::Address> recv_addr =
        packet_router_->get_service_discovery_multicast_endpoint()->get_sender_address();

    if (ttl == 0) { // early exit
        LGSOMEIP_LOG_INFO << "ServiceManager::on_external_find_service / ttl is 0. ignore";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdFindServiceTtlZero);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_find_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << " from " << recv_addr->get_ip_address();

    std::uint32_t minimum_minor_version = 0;
    bool has_minimum_minor = false;

    // Find Service on Available Service
    struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    if (service_info == nullptr) {
        if (instance_id == SOMEIP_DEFAULT_ANY_INSTANCE) {
            std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>> offerlist;
            auto instance_list = find_available_service_instance_all(service_id);
            if (!instance_list.empty()) {
                for (auto& instance : instance_list) {
                    auto service_config = configuration_->get_service_info(service_id, instance.first);

                    if (service_config != nullptr) {
                        minimum_minor_version = service_config->get_minimum_minor_version();
                        has_minimum_minor = service_config->has_minimum_minor_version();
                    }

                    if (check_service_version(major_version, instance.second->major, minor_version,
                                              instance.second->minor, minimum_minor_version, has_minimum_minor)) {
                        // send "offer service message" to requesting app.
                        // Gather an OfferService message to send it together in a packet
                        gathered_offer_list_[service_id][instance.first] = instance.second;
                    }

                    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_find_service "
                                       << format_service_instance_interface_version(service_id, instance.first,
                                                                                    major_version, minor_version);
                }

                LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_find_service " << " / send OfferService to "
                                   << recv_addr->to_string();
            }
        } else {
            LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_find_service / No service info. Ignore message";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id,
                SomeipPacketStatistics::kSomeipSdFindServiceNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }

        return;
    }

    // register service request to available service list.
    auto service_config = configuration_->get_service_info(service_id, instance_id);
    if (service_config != nullptr) {
        minimum_minor_version = service_config->get_minimum_minor_version();
        has_minimum_minor = service_config->has_minimum_minor_version();
    }

    if (check_service_version(major_version, service_info->major, minor_version, service_info->minor,
                              minimum_minor_version, has_minimum_minor)) {
        // send "offer service message" to requesting app.
        // Gather an OfferService message to send it together in a packet
        if (service_info->app_id > 0 && service_info->is_in_config_file == true) {
            gathered_offer_list_[service_id][instance_id] = service_info;

            LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_find_service "
                               << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                            minor_version)
                               << " / send OfferService to " << recv_addr->to_string();
        } else {
            LGSOMEIP_LOG_INFO << "ServiceManager::on_external_find_service / There is no provider inside "
                              << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                           minor_version)
                              << " requested from " << recv_addr->to_string();

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id,
                SomeipPacketStatistics::kSomeipSdFindServiceNoProviderInside);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }
    } else {
        LGSOMEIP_LOG_INFO << "ServiceManager::on_external_find_service / There is no matched provider "
                          << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                       minor_version)
                          << " requested from " << recv_addr->to_string();

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kIncoming, service_id,
            SomeipPacketStatistics::kSomeipSdFindServiceNoMatchedProvider);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void ServiceManager::on_internal_offer_service(std::uint16_t service_id, std::uint16_t instance_id,
                                               std::uint8_t major_version, std::uint32_t minor_version,
                                               std::uint32_t ttl, std::uint32_t request_id) {
    std::uint16_t app_id = static_cast<std::uint16_t>(request_id >> 16);

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_internal_offer_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << " TTL=" << ttl << " from " << format_named_id("AppID", app_id, 4);

    auto config = configuration_->get_service_info(service_id, instance_id);

    struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);

    //  Sending Find entries shall be stopped
    // after receiving the coresponding Offer entries by jumping to the Main Phase
    // in which no Find entries are sent.
    // Erase the service in repetition_find_list_ when Offer service comes in.
    // This erase part is implemented in on_external_offer_service and on_internal_offer_service.
    // Thus, please modify together.
    std::unique_lock<std::mutex> lock(request_list_mutex_);
    if (ttl > 0 && repetition_find_list_.find(service_id) != repetition_find_list_.end()) {
        repetition_find_list_[service_id].erase(instance_id);

        if (repetition_find_list_[service_id].empty()) {
            repetition_find_list_.erase(service_id);

            LGSOMEIP_LOG_DEBUG << "ServiceManager::on_internal_offer_service / Service "
                               << format_service_instance_id(service_id, instance_id)
                               << " / is erased in repetition_find_list_";
        }
    }
    lock.unlock();

    if (service_info != nullptr && ttl > 0) {
        // Update Offer Service Information : update TTL
        service_info->ttl = ttl;
    } else if (service_info == nullptr && ttl > 0) {
        handle_new_offer_service(service_id, instance_id, major_version, minor_version, ttl, config, app_id);
    } else if (ttl == 0) {
        handle_stop_offer_service(service_id, instance_id, major_version, minor_version, service_info, config);
    }
}

void ServiceManager::on_external_offer_service(std::uint16_t service_id, std::uint16_t instance_id,
                                               std::uint8_t major_version, std::uint32_t minor_version,
                                               std::uint32_t ttl,
                                               std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                               std::shared_ptr<lgsomeip::osabstraction::Address> udp_address,
                                               std::shared_ptr<lgsomeip::osabstraction::Address> received_address) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_offer_service "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version)
                       << " TTL=" << ttl << " from Addr[" << received_address->get_ip_address() << "]";

    if (tcp_address != nullptr)
        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_offer_service / Received Service Address(TCP) : "
                           << tcp_address->to_string();
    if (udp_address != nullptr)
        LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_offer_service / Received Service Address(UDP) : "
                           << udp_address->to_string();

    auto config = configuration_->get_service_info(service_id, instance_id);
    if (config == nullptr) {
        LGSOMEIP_LOG_DEBUG
            << "ServiceManager::on_external_offer_service / Config for the service is nullptr. Unexpected service "
            << "from external.";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdOfferServiceNoConfig);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    if (config->is_provider() == true) {
        std::ostringstream sout;
        sout << "Configuration Error! / Service Info " << format_service_instance_id(service_id, instance_id)
             << " is external service / set \'is-provider\' to false";

        throw LSAR_CONFIGURATION_ERROR(sout.str());
    }

    AvailableService* service_info = find_available_service_instance(service_id, instance_id);

    //  Sending Find entries shall be stopped
    // after receiving the coresponding Offer entries by jumping to the Main Phase
    // in which no Find entries are sent.
    // Erase the service in repetition_find_list_ when Offer service comes in.
    // This erase part is implemented in on_external_offer_service and on_internal_offer_service.
    // Thus, please modify together.
    std::unique_lock<std::mutex> lock(request_list_mutex_);
    if (ttl > 0 && repetition_find_list_.find(service_id) != repetition_find_list_.end()) {
        repetition_find_list_[service_id].erase(instance_id);

        if (repetition_find_list_[service_id].empty()) {
            repetition_find_list_.erase(service_id);

            LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_offer_service / Service "
                               << format_service_instance_id(service_id, instance_id)
                               << " / is erased in repetition_find_list_";
        }
    }
    lock.unlock();

    if (service_info != nullptr && ttl > 0) {
        // Update Offer Service Information : update TTL
        service_info->ttl = ttl;

        // Update SubscribeEventgroup Info
        auto& subscribelist = service_info->subscribe;
        for (auto& subscribe : subscribelist) {
            std::uint16_t eventgroup_id = subscribe.first;
            for (auto& request : subscribe.second) {
                // TODO : TTL Time
                request.ttl = 3;
            }
            // In the send_subscribe_eventgroup() function, a SubscribeEventgroup message will be gathered in
            // gathered_subscribe_list_.
            send_subscribe_eventgroup(service_id, instance_id, eventgroup_id, ttl, major_version, 0, false);
        }
    } else if (service_info == nullptr && ttl > 0) {
        handle_new_offer_service(service_id, instance_id, major_version, minor_version, ttl, config, 0, tcp_address,
                                 udp_address);
    } else if (ttl == 0) {
        handle_stop_offer_service(service_id, instance_id, major_version, minor_version, service_info);
    }
}

void ServiceManager::on_internal_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                                      std::uint16_t event_group_id, std::uint8_t major_version,
                                                      std::uint32_t ttl, std::uint32_t request_id) {
    std::uint16_t app_id = static_cast<std::uint16_t>(request_id >> 16);
    std::function<bool(RequestedSubscribe&)> predicate = [&](RequestedSubscribe& i) { return i.app_id == app_id; };

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_internal_subscribe_eventgroup "
                       << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
                       << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl << " from "
                       << format_named_id("AppID", app_id, 4);

    AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    if (service_info == nullptr) {
        LGSOMEIP_LOG_INFO << "ServiceManager::on_internal_subscribe_eventgroup / serviceInfo is nullptr. "
                          << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                          << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl << " from "
                          << format_named_id("AppID", app_id, 4);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdSubscribeNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        return;
    }

    auto& subscribe_list = service_info->subscribe[event_group_id];
    auto subscribe = std::find_if(std::begin(subscribe_list), std::end(subscribe_list), predicate);

    if (ttl > 0 && subscribe != std::end(subscribe_list)) {
        update_subscribe_info(service_id, instance_id, event_group_id, major_version, ttl, request_id, subscribe);
    } else if (ttl == 0 && subscribe != std::end(subscribe_list)) {
        handle_stop_subscribe_eventgroup(service_id, instance_id, event_group_id, major_version, ttl, app_id,
                                         service_info, subscribe);
    } else if (ttl > 0 && subscribe == std::end(subscribe_list)) {
        handle_new_subscribe_eventgroup(service_id, instance_id, event_group_id, major_version, ttl, request_id,
                                        service_info);
    }
}

void ServiceManager::on_external_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                                      std::uint16_t event_group_id, std::uint8_t major_version,
                                                      std::uint32_t ttl, std::uint32_t request_id,
                                                      std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                                      std::shared_ptr<lgsomeip::osabstraction::Address> udp_address) {
    std::function<bool(RequestedSubscribe&)> predicate = [&](RequestedSubscribe& i) {
        bool check_tcp = (i.tcp_address == nullptr && tcp_address == nullptr) ||
                         (i.tcp_address != nullptr && tcp_address != nullptr && *i.tcp_address == *tcp_address);
        bool check_udp = (i.udp_address == nullptr && udp_address == nullptr) ||
                         (i.udp_address != nullptr && udp_address != nullptr && *i.udp_address == *udp_address);
        return check_tcp && check_udp;
    };

    std::shared_ptr<lgsomeip::osabstraction::Address> discovery_address;
    if (packet_router_->get_service_discovery_unicast_endpoint() != nullptr) {
        discovery_address = packet_router_->get_service_discovery_unicast_endpoint()->get_sender_address();
    } else if (packet_router_->get_service_discovery_multicast_endpoint() != nullptr) {
        discovery_address = packet_router_->get_service_discovery_multicast_endpoint()->get_sender_address();
    }

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_external_subscribe_eventgroup "
                       << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
                       << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl << " from "
                       << discovery_address->get_ip_address();

    AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    if (service_info == nullptr) {
        LGSOMEIP_LOG_INFO << "ServiceManager::on_external_subscribe_eventgroup "
                          << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                          << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl << " from "
                          << discovery_address->get_ip_address() << " / serviceInfo is nullptr.";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdSubscribeNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    } else if (service_info->app_id == 0) {
        LGSOMEIP_LOG_INFO << "ServiceManager::on_external_subscribe_eventgroup / is not providing service. "
                          << format_service_instance_id(service_id, instance_id);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdSubscribeNoProviderInside);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    auto& subscribe_list = service_info->subscribe[event_group_id];
    auto subscribe = std::find_if(std::begin(subscribe_list), std::end(subscribe_list), predicate);

    if (ttl > 0 && subscribe != std::end(subscribe_list)) {
        update_subscribe_info(service_id, instance_id, event_group_id, major_version, ttl, request_id, subscribe);
    } else if (ttl == 0 && subscribe != std::end(subscribe_list)) {
        handle_stop_subscribe_eventgroup(service_id, instance_id, event_group_id, major_version, ttl, 0, service_info,
                                         subscribe, tcp_address, udp_address);
    } else if (ttl > 0 && subscribe == std::end(subscribe_list)) {
        handle_new_subscribe_eventgroup(service_id, instance_id, event_group_id, major_version, ttl, request_id,
                                        service_info, tcp_address, udp_address);
    }
}

void ServiceManager::on_subscribe_eventgroup_ack(std::uint16_t service_id, std::uint16_t instance_id,
                                                 std::uint16_t event_group_id, std::uint8_t major_version,
                                                 std::uint32_t ttl, bool from_internal,
                                                 std::shared_ptr<lgsomeip::osabstraction::Address> multicast) {
    std::uint16_t offer_svc_app_id = 0;

    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_subscribe_eventgroup_ack "
                       << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
                       << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                       << " from: " << ((from_internal) ? "Internal" : "External")
                       << (multicast != nullptr ? "/" + multicast->to_string() : std::string());

    struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    if (service_info == nullptr) {
        LGSOMEIP_LOG_INFO << "ServiceManager::on_subscribe_eventgroup_ack / ServiceInfo does not exist "
                          << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                          << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                          << " from: " << ((from_internal) ? "Internal" : "External");

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        if (from_internal) {
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id,
                SomeipPacketStatistics::kSomeipSdSubscribeAckNoServiceInfo);
        } else {
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id,
                SomeipPacketStatistics::kSomeipSdSubscribeAckNoServiceInfo);
        }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    offer_svc_app_id = service_info->app_id;
    LGSOMEIP_LOG_DEBUG << "ServiceManager::on_subscribe_eventgroup_ack / Service offered"
                       << (offer_svc_app_id > 0 ? std::string(" by ") + format_named_id("AppID", offer_svc_app_id, 4)
                                                : std::string())
                       << (offer_svc_app_id == 0 && service_info->tcp_address != nullptr
                               ? std::string(" from TCP Addr:") + service_info->tcp_address->to_string()
                               : std::string())
                       << (offer_svc_app_id == 0 && service_info->udp_address != nullptr
                               ? std::string(" from UDP Addr:") + service_info->udp_address->to_string()
                               : std::string());

    auto service_config = get_configuration()->get_service_info(service_id, instance_id);
    bool is_offered_from_external = (offer_svc_app_id == 0);
    bool is_in_config = (service_config != nullptr);
    std::vector<std::uint16_t>* eventgroup = nullptr;

    if ((is_offered_from_external == true) && (is_in_config == false)) {
        LGSOMEIP_LOG_INFO
            << "ServiceManager::on_subscribe_eventgroup_ack  / Configuration Error! Service Config does not exist "
            << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
            << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
            << " from: " << ((from_internal) ? "Internal" : "External");

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        if (from_internal) {
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kOutgoing, service_id, SomeipPacketStatistics::kSomeipSdSubscribeAckNoConfig);
        } else {
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, service_id, SomeipPacketStatistics::kSomeipSdSubscribeAckNoConfig);
        }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    } else if (is_in_config == true) {
        LGSOMEIP_LOG_DEBUG
            << "ServiceManager::on_subscribe_eventgroup_ack / Service is configured in the configuration file.";

        eventgroup = service_config->get_event_group(event_group_id);
        if (eventgroup == nullptr) {
            LGSOMEIP_LOG_INFO
                << "ServiceManager::on_subscribe_eventgroup_ack / Configuration Error! Eventgroup Info does not "
                << "exist " << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                << " from: " << ((from_internal) ? "Internal" : "External");

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            if (from_internal) {
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kOutgoing, service_id,
                    SomeipPacketStatistics::kSomeipSdSubscribeAckNoEventConfig);
            } else {
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdSubscribeAckNoEventConfig);
            }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            return;
        }
    } else {
        LGSOMEIP_LOG_DEBUG
            << "ServiceManager::on_subscribe_eventgroup_ack / This service is for internal communication (IPC)";
    }

    auto& subscribe_list = service_info->subscribe[event_group_id];
    auto subscribe = std::begin(subscribe_list);

    // Update Routing Info
    while (subscribe != std::end(subscribe_list)) {
        std::uint16_t subscribe_app_id = subscribe->app_id;
        bool is_subscribed_from_internal = (subscribe_app_id != 0);

        if (subscribe->state == SubscribeState::SUBSCRIBED) {
            subscribe++;
            continue;
        }

        if (ttl > 0) {
            if (is_subscribed_from_internal && subscribe->state == SubscribeState::UPDATE) {
                // Don't send ack message to app, when internal subscribe state.
                subscribe->state = SubscribeState::SUBSCRIBED;
                subscribe++;
                continue;
            }

            if (eventgroup != nullptr) {
                for (std::uint16_t event_id : *eventgroup) {
                    if (multicast != nullptr) {
                        subscribe->tcp_address = nullptr;
                        subscribe->udp_address = multicast;
                        packet_router_->add_subscribe_route(service_id, instance_id, event_id, subscribe_app_id,
                                                            multicast, event_group_id);
                    } else {
                        if (is_subscribed_from_internal == true) {
                            packet_router_->add_subscribe_route(service_id, instance_id, event_id, subscribe_app_id,
                                                                nullptr, event_group_id);
                        } else {
                            if (subscribe->tcp_address != nullptr) {
                                packet_router_->add_subscribe_route(service_id, instance_id, event_id, 0,
                                                                    subscribe->tcp_address, event_group_id);
                            }
                            if (subscribe->udp_address != nullptr) {
                                packet_router_->add_subscribe_route(service_id, instance_id, event_id, 0,
                                                                    subscribe->udp_address, event_group_id);
                            }
                        }
                    }
                }
            } else {
                if (multicast != nullptr) {
                    subscribe->tcp_address = nullptr;
                    subscribe->udp_address = multicast;
                    packet_router_->add_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT,
                                                        subscribe_app_id, multicast);
                } else {
                    if (is_subscribed_from_internal == true) {
                        packet_router_->add_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT,
                                                            subscribe_app_id, nullptr, event_group_id);
                    } else {
                        if (subscribe->tcp_address != nullptr) {
                            packet_router_->add_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT, 0,
                                                                subscribe->tcp_address);
                        }
                        if (subscribe->udp_address != nullptr) {
                            packet_router_->add_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT, 0,
                                                                subscribe->udp_address);
                        }
                    }
                }
            }

            // ttl on subscribe ack if it is different with ttl on subscribe
            if (is_offered_from_external) {
                subscribe->ttl = ttl;
            }
            // update info according to subscribe ack
            subscribe->state = SubscribeState::SUBSCRIBED;
        } else {
            // Remove Routing Info
            if (eventgroup != nullptr) {
                for (std::uint16_t event_id : *eventgroup) {
                    packet_router_->remove_subscribe_route(service_id, instance_id, event_id, subscribe_app_id);
                }
            } else {
                packet_router_->remove_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT,
                                                       subscribe_app_id);
            }
        }

        // Send SubscribeAck Message for internal
        if (is_subscribed_from_internal) {
            send_subscribe_eventgroup_ack(service_id, instance_id, event_group_id, ttl, major_version,
                                          subscribe_app_id);
        }
        // Send SubscribeAck Message for external
        else {
            std::string target_ip;
            if (subscribe->tcp_address != nullptr)
                target_ip = subscribe->tcp_address->get_ip_address();
            if (subscribe->udp_address != nullptr)
                target_ip = subscribe->udp_address->get_ip_address();

            subscribe->ip_address = target_ip;

            auto eventgroup_object = service_config->get_event_group_object(event_group_id);
            std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr;

            if ((service_config->get_multicast_address())->size() > 0) {
                if (eventgroup_object->is_multicast()) {
                    multicast = eventgroup_object->get_multicast_address();
                    if (multicast == nullptr) {
                        multicast = (service_config->get_multicast_address()->operator[](0));
                    }
                }
            }
            send_subscribe_eventgroup_ack(service_id, instance_id, event_group_id, ttl, major_version, subscribe_app_id,
                                          multicast, target_ip);
        }

        if (ttl == 0) {
            // update info according to subscribe nack
            subscribe = subscribe_list.erase(subscribe);
        }
    }
}

void ServiceManager::handle_stop_offer_service(std::uint16_t service_id, std::uint16_t instance_id,
                                               std::uint8_t major_version, std::uint32_t minor_version,
                                               AvailableService* service_info, ServiceInfo* config) {
    if (service_info != nullptr) {
        if (major_version != service_info->major || minor_version != service_info->minor)
            return;

        // Send "stop offer service" message to request app
        send_internal_offer_service_all(service_id, instance_id, SOMEIP_DEFAULT_TTL_OFF, major_version, minor_version);

        bool is_need_to_send_external = (config != nullptr);
        if (is_need_to_send_external) {
            std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>> stop_offer_list;
            stop_offer_list[service_id][instance_id] = service_info;
            send_external_offer_service(stop_offer_list, nullptr, SOMEIP_DEFAULT_TTL_OFF, LGSOMEIP_MULTI_ENDPOINT);
        }

        // Remove the service from available_service_list_ after sending StopOfferService out
        std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);

        if (available_service_list_.find(service_id) != available_service_list_.end()) {
            available_service_list_[service_id].erase(instance_id);

            if (available_service_list_[service_id].empty()) {
                available_service_list_.erase(service_id);
                LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_stop_offer_service / No more instances in "
                                   << format_named_id("ServiceID", service_id, 4) << ". Erase Service Info in list.";
            }
        }

        packet_router_->remove_route(service_id, instance_id);

        // Erase service in repetition_offer_list_
        if (repetition_offer_list_.find(service_id) != repetition_offer_list_.end()) {
            repetition_offer_list_[service_id].erase(instance_id);

            if (repetition_offer_list_[service_id].empty()) {
                repetition_offer_list_.erase(service_id);
                LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_stop_offer_service / Service "
                                   << format_service_instance_id(service_id, instance_id)
                                   << " / is erased in repetition_offer_list_";
            }
        }
    } else {
        LGSOMEIP_LOG_INFO << "ServiceManager::handle_stop_offer_service / Cannot find service information "
                          << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                       minor_version);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_sd_type_packet(
            SomeipPacketStatistics::kOutgoing, service_id,
            SomeipPacketStatistics::kSomeipSdOfferServiceHandleStopOfferNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }

    // Erase service in repetition_find_list_
    // After receiving StopOfferService, do not send FindService anymore
    std::lock_guard<std::mutex> lock(request_list_mutex_);
    if (repetition_find_list_.find(service_id) != repetition_find_list_.end()) {
        repetition_find_list_[service_id].erase(instance_id);

        if (repetition_find_list_[service_id].empty()) {
            repetition_find_list_.erase(service_id);
            LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_stop_offer_service / Service "
                               << format_service_instance_id(service_id, instance_id)
                               << " / is erased in repetition_find_list_";
        }
    }
}

void ServiceManager::handle_new_offer_service(std::uint16_t service_id, std::uint16_t instance_id,
                                              std::uint8_t major_version, std::uint32_t minor_version,
                                              std::uint32_t ttl, ServiceInfo* config, std::uint16_t app_id,
                                              std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address,
                                              std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address) {
    bool is_in_config = (config != nullptr);
    bool is_offer_from_internal = (app_id > 0);
    std::shared_ptr<lgsomeip::osabstraction::Address> tcp_addr{nullptr}, udp_addr{nullptr};
    std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> multicast_list{nullptr};
    std::uint16_t local_tcp_port{0}, local_udp_port{0};
    std::uint8_t our_vlan_priority{0xff};

    if (is_in_config) {
        local_tcp_port = config->get_reliable_port();
        local_udp_port = config->get_unreliable_port();
        our_vlan_priority = config->get_vlan_priority();

        LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_new_offer_service / Configured Local Port: " << "TCP="
                           << local_tcp_port << ", UDP=" << local_udp_port;

        if (is_offer_from_internal) {
            tcp_addr = config->get_reliable_address();
            udp_addr = config->get_unreliable_address();
            multicast_list = config->get_multicast_address();
        } else {
            tcp_addr = remote_tcp_address;
            udp_addr = remote_udp_address;
        }
    }

    {
        // Offer Service : Add new service
        std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
        available_service_list_[service_id].emplace(
            instance_id, AvailableService(app_id, is_in_config, major_version, minor_version, ttl, tcp_addr, udp_addr));
    }

    // It's for CyclicOfferService
    if (is_offer_from_internal && is_in_config) {
        std::lock_guard<std::mutex> repetition_lock(repetition_offer_list_mutex_);
        repetition_offer_list_[service_id].emplace(
            instance_id, AvailableService(app_id, is_in_config, major_version, minor_version, ttl, tcp_addr, udp_addr));
    }

    // Add route information in a new thread and retry in case of failure.
    // If retry also fails remove service information from DB.
    std::thread add_connection_th([=]() {
        // For assigning thread name
        pthread_setname_np(pthread_self(), "SomeipSvcConn");

        LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_new_offer_service / "
                           << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                        minor_version)
                           << " New thread for add_route";

        bool result = packet_router_->add_route(service_id, instance_id, app_id, tcp_addr, local_tcp_port, udp_addr,
                                                local_udp_port, our_vlan_priority, multicast_list);
        if (result == false && is_in_config && tcp_addr != nullptr) {
            for (auto i = 0; i < RETRY_CONNECTION_MAX_NUM; ++i) {
                LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_new_offer_service / "
                                   << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                                minor_version)
                                   << " Retry add_route after " << RETRY_CONNECTION_PERIOD << "ms";

                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_CONNECTION_PERIOD));
                result = packet_router_->add_route(service_id, instance_id, app_id, tcp_addr, local_tcp_port, udp_addr,
                                                   local_udp_port, our_vlan_priority, multicast_list);
                if (result == true) {
                    break;
                }
            }
        }

        if (result == true) {
            LGSOMEIP_LOG_INFO << "ServiceManager::handle_new_offer_service / "
                              << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                           minor_version)
                              << " add_route success";

            std::unique_lock<std::recursive_mutex> lock(available_list_mutex_);
            if (available_service_list_.find(service_id) != available_service_list_.end()) {
                lock.unlock();
                send_internal_offer_service_all(service_id, instance_id, SOMEIP_DEFAULT_TTL_ON, major_version,
                                                minor_version);
            } else {
                LGSOMEIP_LOG_WARN
                    << "ServiceManager::handle_new_offer_service / Fail to add_route (Cannot find service "
                    << "information) "
                    << format_service_instance_interface_version(service_id, instance_id, major_version, minor_version);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                if (is_offer_from_internal) {
                    SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                        SomeipPacketStatistics::kOutgoing, service_id,
                        SomeipPacketStatistics::kSomeipSdOfferServiceHandleNewOfferNoServiceInfo);
                } else {
                    SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                        SomeipPacketStatistics::kIncoming, service_id,
                        SomeipPacketStatistics::kSomeipSdOfferServiceHandleNewOfferNoServiceInfo);
                }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
            }
        } else {
            LGSOMEIP_LOG_WARN << "ServiceManager::handle_new_offer_service / Fail to add_route!!! "
                              << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                           minor_version);

            {
                std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
                available_service_list_[service_id].erase(instance_id);
                if (available_service_list_[service_id].empty()) {
                    available_service_list_.erase(service_id);
                }
            }

            if (is_offer_from_internal && is_in_config) {
                std::lock_guard<std::mutex> repetition_lock(repetition_offer_list_mutex_);
                repetition_offer_list_[service_id].erase(instance_id);
                if (repetition_offer_list_[service_id].empty()) {
                    repetition_offer_list_.erase(service_id);
                }
            }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            if (is_offer_from_internal) {
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kOutgoing, service_id,
                    SomeipPacketStatistics::kSomeipSdOfferServiceHandleNewOfferFailed);
            } else {
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, service_id,
                    SomeipPacketStatistics::kSomeipSdOfferServiceHandleNewOfferFailed);
            }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }
    });

    std::lock_guard<std::mutex> lock(add_connection_threads_mutex_);
    add_connection_threads_.emplace_back(std::move(add_connection_th));
}

void ServiceManager::join_add_connection_threads() {
    std::lock_guard<std::mutex> lock(add_connection_threads_mutex_);
    for (auto& thread : add_connection_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    add_connection_threads_.clear();
}

void ServiceManager::update_subscribe_info(std::uint16_t service_id, std::uint16_t instance_id,
                                           std::uint16_t event_group_id, std::uint8_t major_version, std::uint32_t ttl,
                                           std::uint32_t request_id,
                                           std::vector<RequestedSubscribe>::iterator subscription) {
    // update subscribe info
    LGSOMEIP_LOG_DEBUG << "ServiceManager::update_subscribe_info / Update subscribe";
    std::uint16_t app_id = static_cast<std::uint16_t>(request_id >> 16);

    subscription->ttl = ttl;
    subscription->request_id_received = request_id;
    if (subscription->state == SubscribeState::SUBSCRIBED)
        subscription->state = SubscribeState::UPDATE;

    send_subscribe_eventgroup(service_id, instance_id, event_group_id, ttl, major_version, app_id);
}

void ServiceManager::handle_stop_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                                      std::uint16_t event_group_id, std::uint8_t major_version,
                                                      std::uint32_t ttl, std::uint16_t app_id,
                                                      AvailableService* service_info,
                                                      std::vector<RequestedSubscribe>::iterator subscription,
                                                      std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                                      std::shared_ptr<lgsomeip::osabstraction::Address> udp_address) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_stop_subscribe_eventgroup / Stop subscribe";

    // If the service is subscribed from external, send stop subscribe eventgroup
    if (service_info->app_id == 0) {
        send_subscribe_eventgroup(service_id, instance_id, event_group_id, ttl, major_version, app_id);
    } else {
        send_subscribe_eventgroup_ack(service_id, instance_id, event_group_id, ttl, major_version, app_id);
    }

    // remove subscribe info
    auto& subscribe_list = service_info->subscribe[event_group_id];
    subscribe_list.erase(subscription);

    // If there are no subscribes in this eventgroup,
    // remove it from subscribe list.
    if (subscribe_list.empty()) {
        service_info->subscribe.erase(event_group_id);
    }

    auto config_service_info = get_configuration()->get_service_info(service_id, instance_id);
    if (config_service_info != nullptr) {
        // In case of external services
        auto eventgroup = config_service_info->get_event_group(event_group_id);
        if (tcp_address == nullptr && udp_address == nullptr) {
            for (std::uint16_t event_id : *eventgroup) {
                packet_router_->remove_subscribe_route(service_id, instance_id, event_id, app_id);
            }
        } else {
            for (std::uint16_t event_id : *eventgroup) {
                if (tcp_address != nullptr) {
                    packet_router_->remove_subscribe_route(service_id, instance_id, event_id, app_id, tcp_address);
                }
                if (udp_address != nullptr) {
                    packet_router_->remove_subscribe_route(service_id, instance_id, event_id, app_id, udp_address);
                }
            }
        }
    } else {
        // In case of internal services (IPC), since the event of this eventgroup has been registered
        // as SOMEIP_DEFAULT_ANY_EVENT,
        // SOMEIP_DEFAULT_ANY_EVENT with appID is used to remove the subscribe route.
        packet_router_->remove_subscribe_route(service_id, instance_id, SOMEIP_DEFAULT_ANY_EVENT, app_id);
    }
}

void ServiceManager::handle_new_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                                     std::uint16_t event_group_id, std::uint8_t major_version,
                                                     std::uint32_t ttl, std::uint32_t request_id,
                                                     AvailableService* service_info,
                                                     std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                                     std::shared_ptr<lgsomeip::osabstraction::Address> udp_address) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::handle_new_subscribe_eventgroup / Add subscribe";

    // add subscribe info
    auto& subscribe_list = service_info->subscribe[event_group_id];
    std::uint16_t app_id = static_cast<std::uint16_t>(request_id >> 16);

    RequestedSubscribe info;
    info.ttl = ttl;
    info.app_id = app_id;
    info.request_id_received = request_id;
    info.tcp_address = tcp_address;
    info.udp_address = udp_address;
    subscribe_list.push_back(info);
    send_subscribe_eventgroup(service_id, instance_id, event_group_id, ttl, major_version, app_id);
}

// -----------------------------------------------------------------------------
//  ServiceManager : Message Utils
// -----------------------------------------------------------------------------
struct AvailableService* ServiceManager::find_available_service_instance(std::uint16_t service_id,
                                                                         std::uint16_t instance_id) {
    struct AvailableService* service_info = nullptr;
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    auto it = available_service_list_.find(service_id);
    if (it != available_service_list_.end()) {
        auto& offerlist = it->second;
        auto it2 = offerlist.find(instance_id);
        auto it3 = offerlist.find(SOMEIP_DEFAULT_ANY_INSTANCE);
        if (it2 != offerlist.end()) {
            service_info = &(it2->second);
        } else if (it3 != offerlist.end()) {
            service_info = &(it3->second);
        }
    }
    return service_info;
}

std::map<std::uint16_t, struct AvailableService*>
ServiceManager::find_available_service_instance_all(std::uint16_t service_id) {
    std::map<std::uint16_t, struct AvailableService*> instance_list;
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    auto it = available_service_list_.find(service_id);
    if (it != std::end(available_service_list_)) {
        for (auto& instance : it->second) {
            instance_list[instance.first] = &instance.second;
        }
    }

    return instance_list;
}

bool ServiceManager::find_option_address(std::shared_ptr<lgsomeip::osabstraction::Address>& tcp_address,
                                         std::shared_ptr<lgsomeip::osabstraction::Address>& udp_address,
                                         SDOption* first_options, int first_option_count, SDOption* second_options,
                                         int second_option_count) {
    tcp_address = nullptr;
    udp_address = nullptr;

    for (int i = 0; i < first_option_count; i++) {
        if (first_options[i].get_type() == SOMEIP_SD_OPTION::IP4::TYPEID ||
            first_options[i].get_type() == SOMEIP_SD_OPTION::IP6::TYPEID) {
            auto addr = first_options[i].get_address_option();
            if (addr->is_multicast())
                continue;

            LGSOMEIP_LOG_DEBUG << "ServiceManager::find_option_address / Option Address = " << addr->to_string()
                               << (addr->get_reliable() ? " (TCP)" : " (UDP)");

            // if there are two TCP or Two UDP, it handles error
            if (addr->get_reliable() && tcp_address == nullptr) {
                tcp_address = addr;
            } else if (!addr->get_reliable() && udp_address == nullptr) {
                udp_address = addr;
            } else {
                LGSOMEIP_LOG_DEBUG << "ServiceManager::find_option_address / duplicated options";

                return false;
            }
        }
    }

    for (int i = 0; i < second_option_count; i++) {
        if (second_options[i].get_type() == SOMEIP_SD_OPTION::IP4::TYPEID ||
            second_options[i].get_type() == SOMEIP_SD_OPTION::IP6::TYPEID) {
            auto addr = second_options[i].get_address_option();
            if (addr->is_multicast())
                continue;

            LGSOMEIP_LOG_DEBUG << "ServiceManager::find_option_address / Option Address = " << addr->to_string()
                               << (addr->get_reliable() ? " (TCP)" : " (UDP)");

            // if there are two TCP or Two UDP, it handles error
            if (addr->get_reliable() && tcp_address == nullptr) {
                tcp_address = addr;
            } else if (!addr->get_reliable() && udp_address == nullptr) {
                udp_address = addr;
            } else {
                LGSOMEIP_LOG_DEBUG << "ServiceManager::find_option_address / duplicated options";

                return false;
            }
        }
    }
    return true;
}

void ServiceManager::check_subscribe_error(SDEntry* entry,
                                           std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                           std::shared_ptr<lgsomeip::osabstraction::Address> udp_address,
                                           std::string sender_ip_address) {
    std::uint16_t serviceid = entry->get_service_id();
    std::uint16_t instanceid = entry->get_instance_id();
    struct AvailableService* service_info = find_available_service_instance(serviceid, instanceid);
    auto serviceconfig = get_configuration()->get_service_info(serviceid, instanceid);

    // Check if the Service ID is known.
    if (service_info == nullptr) {
        throw LSAR_SUBSCRIBE_ERROR("serviceInfo is null");
    }

    // Check if the Major Version of this Service Instance is known
    if (service_info->major != entry->get_major_version() && entry->get_major_version() != SOMEIP_DEFAULT_ANY_MAJOR) {
        throw LSAR_SUBSCRIBE_ERROR("Major number Error");
    }

    // case : internal subscribe has no option information
    if (tcp_address == nullptr && udp_address == nullptr)
        return;

    if (serviceconfig == nullptr) {
        LGSOMEIP_LOG_WARN << "ServiceManager::check_subscribe_error / Serviceconfig is null "
                          << format_service_instance_id(serviceid, instanceid);
    }
    // Check if the Eventgroup ID of the Service Instance with Major Version is known
    else if (serviceconfig->get_event_group(entry->get_event_group_id()) == nullptr) {
        LGSOMEIP_LOG_WARN << "ServiceManager::check_subscribe_error / unknown "
                          << format_named_id("EventGroupID", entry->get_event_group_id(), 4) << " for "
                          << format_service_instance_id(serviceid, instanceid);

        throw LSAR_SUBSCRIBE_ERROR("Unknown EventgroupID");
    }

    // Check that at least enough bytes for an empty SOME/IP-SD message are present
    // Check if the referenced Options exist in the options array and are syntactically ok
    //          TODO : Endpoint Options with valid L4-Protocol field
    //                 Length of Options Array is consistent
    if (entry->get_option1st_count() == 0 && entry->get_option1st_index() != 0) {
        throw LSAR_SUBSCRIBE_ERROR("getOption1st Error");
    }
    if (entry->get_option2nd_count() == 0 && entry->get_option2nd_index() != 0) {
        throw LSAR_SUBSCRIBE_ERROR("getOption2nd Error");
    }
    if (tcp_address == nullptr && udp_address == nullptr) {
        throw LSAR_SUBSCRIBE_ERROR("Address is null");
    }

    if (udp_address != nullptr) {
        if (udp_address->get_ip_address() != sender_ip_address) {
            throw LSAR_SUBSCRIBE_ERROR("UDP Address Error");
        }
        if (udp_address->get_port_address() == 0) {
            throw LSAR_SUBSCRIBE_ERROR("UDP Port Error");
        }
    }

    // Check if the TCP connection is already present
    if (tcp_address != nullptr) {
        if (tcp_address->get_ip_address() != sender_ip_address ||
            packet_router_->find_connection(serviceid, instanceid, tcp_address) <= 0) {
            throw LSAR_SUBSCRIBE_ERROR("TCP connection Error");
        }
        if (tcp_address->get_port_address() == 0) {
            throw LSAR_SUBSCRIBE_ERROR("UDP Port Error");
        }
    }
}

void ServiceManager::remove_service_info(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    for (auto service_it = available_service_list_.begin(); service_it != available_service_list_.end();) {
        std::uint16_t service_id = 0;
        auto& instance_map = service_it->second;
        for (auto& instance : instance_map) {
            std::uint16_t instance_id = instance.first;
            auto& service_info = instance.second;

            if ((service_info.tcp_address != nullptr &&
                 service_info.tcp_address->get_ip_address().compare(address->get_ip_address()) == 0) ||
                (service_info.udp_address != nullptr &&
                 service_info.udp_address->get_ip_address().compare(address->get_ip_address()) == 0)) {
                service_id = service_it->first;
                instance_id = instance.first;

                packet_router_->remove_route(service_id, instance_id);
                // Send "stop offer service" message to request app
                send_internal_offer_service_all(service_id, instance_id, SOMEIP_DEFAULT_TTL_OFF);
            }
        }

        if (service_id != 0) {
            service_it = available_service_list_.erase(service_it);

            // Erase service in repetition_offer_list_
            repetition_offer_list_.erase(service_id);
        } else {
            ++service_it;
        }
    }
}

// -----------------------------------------------------------------------------
//  ServiceManager : Send Message
// -----------------------------------------------------------------------------

void ServiceManager::send_internal_offer_service_all(std::uint16_t service_id, std::uint16_t instance_id,
                                                     std::uint32_t ttl, std::uint8_t major_version,
                                                     std::uint32_t minor_version) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::send_internal_offer_service_all "
                       << format_service_instance_interface_version(service_id, instance_id, major_version,
                                                                    minor_version);

    std::uint32_t minimum_minor_version = 0;
    bool has_minimum_minor = false;

    std::lock_guard<std::mutex> lock(request_list_mutex_);
    for (auto& instance : request_service_list_[service_id]) {
        std::uint16_t iid = instance.first;
        if ((instance_id != SOMEIP_DEFAULT_ANY_INSTANCE) && (iid != instance_id && iid != SOMEIP_DEFAULT_ANY_INSTANCE))
            continue;

        auto& req_app_list = instance.second;
        auto service_config = configuration_->get_service_info(service_id, instance_id);

        if (service_config != nullptr) {
            minimum_minor_version = service_config->get_minimum_minor_version();
            has_minimum_minor = service_config->has_minimum_minor_version();
        }

        for (auto& request : req_app_list) {
            if (check_service_version(request.major, major_version, request.minor, minor_version, minimum_minor_version,
                                      has_minimum_minor)) {
                // send "offer service message" to requesting app.
                request.state = SOMEIP_SERVICE_STATE_MAIN;
                packet_router_->set_instance_id(service_id, instance_id, request.app_id);
                send_internal_offer_service(service_id, instance_id, ttl, request.major, request.minor, request.app_id);
            }
        }
    }
}

void ServiceManager::send_internal_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint32_t ttl,
                                                 std::uint8_t major_version, std::uint32_t minor_version,
                                                 std::uint32_t app_id) {
    std::uint8_t send_buffer[100];
    std::uint32_t len = 0;

    // Composing SOME/IP-SD Message
    auto message = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    entry.set_service_id(service_id);
    entry.set_instance_id(instance_id);
    entry.set_major_version(major_version);
    entry.set_minor_version(minor_version);
    entry.set_ttl(ttl);

    struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    if (service_info == nullptr) {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::senInternalOfferService / serviceInfo is Null";
        return;
    }

    std::uint16_t source_appid = service_info->app_id;
    std::uint32_t req_id = (source_appid & 0xffff) << 16;
    message->set_request_id(req_id);

    LGSOMEIP_LOG_DEBUG << "ServiceManager::send_internal_offer_service "
                       << format_service_instance_id(service_id, instance_id) << " Target to "
                       << format_named_id("AppID", app_id, 4)
                       << (source_appid > 0 ? ", Source " + format_named_id("AppID", source_appid, 4) : std::string());

    MessageComposer::add_entry(message, &entry, nullptr);
    MessageBuilder::build_byte_stream(send_buffer, &len, *message);

    packet_router_->send_internal_message(send_buffer, len, app_id);
}

void ServiceManager::send_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                               std::uint32_t event_group_id, std::uint32_t ttl,
                                               std::uint8_t major_version, std::uint16_t app_id, bool send_alone) {
    std::lock_guard<std::recursive_mutex> lock(available_list_mutex_);
    auto service_info = &(available_service_list_[service_id][instance_id]);
    if (service_info == nullptr)
        return;
    std::uint16_t target_appid = service_info->app_id;

    if (target_appid > 0) {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup "
                           << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                           << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                           << " send_alone: " << send_alone << " to " << format_named_id("AppID", target_appid, 4);
    } else {
        auto addr = service_info->tcp_address;
        if (addr == nullptr)
            addr = service_info->udp_address;
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup "
                           << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                           << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                           << " send_alone: " << send_alone << " to Addr[" << addr->get_ip_address() << "]";
    }

    std::uint8_t send_buffer[100];
    std::uint32_t len = 0;
    std::shared_ptr<SOMEIPSD::type> message;

    // if this message is supposed to be gathered and this message is for External,
    // gathered_subscribe_list_ will be used.
    if (send_alone == false && target_appid == 0) {
        message = gathered_subscribe_list_;
    } else {
        message = MessageBuilder::create<SOMEIPSD>();
    }

    SDEntry entry(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    entry.set_service_id(service_id);
    entry.set_instance_id(instance_id);
    entry.set_major_version(major_version);
    entry.set_event_group_id(event_group_id);
    entry.set_flag(0x00);

    std::uint32_t effective_ttl = (target_appid == 0 && ttl >= 0xFFFF) ? 3 : ttl;
    entry.set_ttl(effective_ttl);

    if (target_appid > 0) {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup / Send Internal Service";

        // Send Internal Service!
        message->set_request_id(app_id << 16);
        message->set_flag(0xc0);
        message->entries().push_back(entry);
        MessageBuilder::build_byte_stream(send_buffer, &len, *message);
        packet_router_->send_internal_message(send_buffer, len, target_appid);
    } else {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup / Send External Service";

        static std::shared_ptr<lgsomeip::osabstraction::Address> dest_sd_addr = packet_router_->make_address();
        dest_sd_addr->set_reliable(false);
        dest_sd_addr->set_port_address(get_configuration()->get_service_discovery_info()->get_port());

        auto service_config = get_configuration()->get_service_info(service_id, instance_id);
        if (service_config == nullptr) {
            LGSOMEIP_LOG_INFO
                << "ServiceManager::send_subscribe_eventgroup / Configuration Error! Service Info does not exist "
                << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
                << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                << " send_alone: " << send_alone;

            return;
        }

        auto svc_tcp_addr = service_config->get_reliable_address();
        auto svc_udp_addr = service_config->get_unreliable_address();
        bool is_available{false};
        std::uint8_t num_of_options = 0;

        std::uint16_t option_type =
            (get_configuration()->get_ip_type() == 6) ? SOMEIP_SD_OPTION::IP6::TYPEID : SOMEIP_SD_OPTION::IP4::TYPEID;
        auto op_type = static_cast<std::uint8_t>(option_type);
        SDOption option1[2]{op_type, op_type};

        if (service_info->tcp_address != nullptr && svc_tcp_addr != nullptr) {
            dest_sd_addr->set_ip_address(service_info->tcp_address->get_ip_address());
            is_available = true;
            option1[num_of_options++].set_address_option(svc_tcp_addr);
        }
        if (service_info->udp_address != nullptr && svc_udp_addr != nullptr) {
            dest_sd_addr->set_ip_address(service_info->udp_address->get_ip_address());
            is_available = true;
            option1[num_of_options++].set_address_option(svc_udp_addr);
        }
        if (is_available == false) {
            LGSOMEIP_LOG_INFO
                << "ServiceManager::send_subscribe_eventgroup / Configuration Error! Dst Address does not exist. "
                << format_service_instance_interface_major_version(service_id, instance_id, major_version) << " "
                << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl;
        }

        entry.set_option1st_count(num_of_options);
        MessageComposer::add_entry(message, &entry, option1, nullptr);

        if (send_alone == true) {
            auto session_info = get_session_id_and_reboot_flag(dest_sd_addr->get_ip_address());
            message->set_request_id(static_cast<std::uint32_t>(session_info.first));
            // Add unicast flag
            message->set_flag(session_info.second | 0x40);

            MessageBuilder::build_byte_stream(send_buffer, &len, *message);
            packet_router_->send_external_sd_message(send_buffer, len, dest_sd_addr, LGSOMEIP_UNI_ENDPOINT);
        }
        // if send_alone is false, which means this SD message is gathered in gathered_subscribe_list_,
        // this message is not sent alone because it will be sent together with other messages
        // in the on_external_offer_service() function.
    }
}

void ServiceManager::send_subscribe_eventgroup_ack(std::uint16_t service_id, std::uint16_t instance_id,
                                                   std::uint32_t event_group_id, std::uint32_t ttl,
                                                   std::uint8_t major_version, std::uint16_t app_id,
                                                   std::shared_ptr<lgsomeip::osabstraction::Address> multicast,
                                                   std::string target_ip) {
    std::uint8_t send_buffer[100];
    std::uint32_t len = 0;

    auto message = MessageBuilder::create<SOMEIPSD>();

    SDEntry entry(SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    entry.set_service_id(service_id);
    entry.set_instance_id(instance_id);
    entry.set_major_version(major_version);
    entry.set_event_group_id(event_group_id);
    entry.set_ttl(ttl);
    entry.set_flag(0x00);

    // Set Multicast Option to SubscribeAck Message
    if (multicast != nullptr) {
        SDOption option(SOMEIP_SD_OPTION::IP4MULTI::TYPEID);
        if (get_configuration()->get_ip_type() == 6) {
            option.set_type(SOMEIP_SD_OPTION::IP6MULTI::TYPEID);
        }
        option.set_address_option(multicast);
        message->options().push_back(option);
        entry.set_option1st_count(message->options().size());
    }

    message->entries().push_back(entry);
    struct AvailableService* service_info = find_available_service_instance(service_id, instance_id);
    std::uint16_t source_appid = 0;
    std::uint16_t target_appid = app_id;
    if (service_info == nullptr) {
        LGSOMEIP_LOG_INFO << "ServiceManager::send_subscribe_eventgroup_ack / serviceInfo is Null / Change TTL to 0 "
                          << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                          << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl;

        entry.set_ttl(0);
        message->entries().back().set_ttl(0);
    } else {
        source_appid = service_info->app_id;
    }

    if (target_appid > 0 && target_ip == "") {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup_ack "
                           << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                           << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl
                           << " to Target " << format_named_id("AppID", target_appid, 4) << " to Source "
                           << format_named_id("AppID", source_appid, 4);
    } else {
        LGSOMEIP_LOG_DEBUG << "ServiceManager::send_subscribe_eventgroup_ack "
                           << format_service_instance_interface_major_version(service_id, instance_id, major_version)
                           << " " << format_named_id("EventGroupID", event_group_id, 4) << " TTL=" << ttl << " to Addr["
                           << target_ip << "]";
    }

    if (target_appid > 0 && target_ip == "") {
        message->set_request_id(source_appid << 16);
        message->set_flag(0xc0);
        MessageBuilder::build_byte_stream(send_buffer, &len, *message);
        packet_router_->send_internal_message(send_buffer, len, target_appid);
    } else if (target_ip != "") {
        static std::shared_ptr<lgsomeip::osabstraction::Address> target_addr = packet_router_->make_address();
        auto session_info = get_session_id_and_reboot_flag(target_ip);
        message->set_request_id(static_cast<std::uint32_t>(session_info.first));
        // Add unicast flag
        message->set_flag(session_info.second | 0x40);
        MessageBuilder::build_byte_stream(send_buffer, &len, *message);

        target_addr->set_ip_address(target_ip);
        target_addr->set_reliable(false);
        target_addr->set_port_address(get_configuration()->get_service_discovery_info()->get_port());

        packet_router_->send_external_sd_message(send_buffer, len, target_addr, LGSOMEIP_UNI_ENDPOINT);
    }
}

void ServiceManager::send_external_find_service_all(
    std::map<std::uint16_t, std::map<std::uint16_t, struct RequestedService*>>& find_list) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::send_external_find_service_all";

    std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    std::uint32_t len = 0;
    auto message = MessageBuilder::create<SOMEIPSD>();

    for (auto& service : find_list) {
        for (auto& instance : service.second) {
            auto item = instance.second;

            SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
            entry.set_service_id(service.first);
            entry.set_instance_id(instance.first);
            entry.set_major_version(item->major);
            entry.set_minor_version(item->minor);
            entry.set_ttl(3);

            MessageComposer::add_entry(message, &entry, nullptr, nullptr);
        }
    }

    auto session_info =
        get_session_id_and_reboot_flag(get_configuration()->get_service_discovery_info()->get_multicast());

    message->set_request_id(static_cast<std::uint32_t>(session_info.first));
    // Add unicast flag
    message->set_flag(session_info.second | 0x40);

    MessageBuilder::build_byte_stream(send_buffer, &len, *message);
    packet_router_->send_external_sd_message(send_buffer, len, nullptr, LGSOMEIP_MULTI_ENDPOINT);
}

void ServiceManager::send_external_find_service(std::uint16_t service, std::uint16_t instance, std::uint32_t ttl,
                                                std::uint8_t major_version, std::uint32_t minor_version) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::send_external_find_service "
                       << format_service_instance_interface_version(service, instance, major_version, minor_version)
                       << " TTL=" << ttl;

    std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    std::uint32_t len = 0;
    auto message = MessageBuilder::create<SOMEIPSD>();

    SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major_version);
    entry.set_minor_version(minor_version);
    entry.set_ttl(ttl);

    MessageComposer::add_entry(message, &entry, nullptr, nullptr);

    auto session_info =
        get_session_id_and_reboot_flag(get_configuration()->get_service_discovery_info()->get_multicast());
    message->set_request_id(static_cast<std::uint32_t>(session_info.first));
    // Add unicast flag
    message->set_flag(session_info.second | 0x40);

    MessageBuilder::build_byte_stream(send_buffer, &len, *message);
    packet_router_->send_external_sd_message(send_buffer, len, nullptr, LGSOMEIP_MULTI_ENDPOINT);
}

void ServiceManager::send_external_offer_service(
    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>>& offer_list,
    std::shared_ptr<lgsomeip::osabstraction::Address> address, std::uint32_t ttl, bool multicast) {
    LGSOMEIP_LOG_DEBUG << "ServiceManager::send_external_offer_service";

    std::uint8_t send_buffer[SOMEIP_UDP_MAX_PAYLOAD_SIZE];
    std::uint32_t len = 0;

    std::shared_ptr<lgsomeip::osabstraction::Address> sdaddr = nullptr;
    if (address != nullptr)
        sdaddr = address;

    auto message = MessageBuilder::create<SOMEIPSD>();

    for (auto& service : offer_list) {
        for (auto& instance : service.second) {
            auto offeritem = instance.second;

            SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
            entry.set_service_id(service.first);
            entry.set_instance_id(instance.first);
            entry.set_major_version(offeritem->major);
            entry.set_minor_version(offeritem->minor);
            entry.set_ttl(ttl);

            std::uint16_t option_type = (get_configuration()->get_ip_type() == 6) ? SOMEIP_SD_OPTION::IP6::TYPEID
                                                                                  : SOMEIP_SD_OPTION::IP4::TYPEID;
            auto op_type = static_cast<std::uint8_t>(option_type);
            SDOption option1[2]{op_type, op_type};
            if (offeritem->tcp_address != nullptr && offeritem->udp_address != nullptr) {
                option1[0].set_address_option(offeritem->tcp_address);
                option1[1].set_address_option(offeritem->udp_address);
                entry.set_option1st_count(2);
            } else if (offeritem->tcp_address != nullptr) {
                option1[0].set_address_option(offeritem->tcp_address);
                entry.set_option1st_count(1);
            } else if (offeritem->udp_address != nullptr) {
                option1[0].set_address_option(offeritem->udp_address);
                entry.set_option1st_count(1);
            }

            MessageComposer::add_entry(message, &entry, option1, nullptr);
        }
    }

    std::string dest_ip_addr = (sdaddr == nullptr ? get_configuration()->get_service_discovery_info()->get_multicast()
                                                  : sdaddr->get_ip_address());
    auto session_info = get_session_id_and_reboot_flag(dest_ip_addr);
    message->set_request_id(static_cast<std::uint32_t>(session_info.first));
    // Add unicast flag
    message->set_flag(session_info.second | 0x40);

    MessageBuilder::build_byte_stream(send_buffer, &len, *message);
    packet_router_->send_external_sd_message(send_buffer, len, sdaddr, multicast);
}

std::pair<std::uint16_t, std::uint8_t> ServiceManager::get_session_id_and_reboot_flag(std::string address) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    // emplace() won't overwrite if session_info_[address] already exists.
    session_info_.emplace(std::make_pair(address, std::make_pair(0u, 1u << 7)));

    // Update session Id
    std::uint16_t previous_session_id = session_info_[address].first;
    session_info_[address].first = (session_info_[address].first % 0xFFFF) + 1;

    // Change reboot flag
    if ((previous_session_id == 0xFFFF) && (session_info_[address].second != 0)) {
        session_info_[address].second = 0;
    }

    return session_info_[address];
}

bool ServiceManager::check_service_version(std::uint16_t major_version, std::uint16_t major_to_compare,
                                           std::uint32_t minor_version, std::uint32_t minor_to_compare,
                                           std::uint32_t minimum_minor_version, bool minimum_find_behavior) {
    // If versionDrivenFindBehavior is set to minimumMinorVersion
    // then the minorVersion shall be set to 0xFFFF FFFF
    // and all found services with a minor version smaller than the requiredMinorVersion shall not be considered.
    LGSOMEIP_LOG_DEBUG << "ServiceManager::check_service_version Version[" << MSGID_FORMAT2(major_version) << "."
                       << MSGID_FORMAT8(minor_version) << "] and major/minor to compare: ["
                       << MSGID_FORMAT2(major_to_compare) << "." << MSGID_FORMAT8(minor_to_compare) << "]"
                       << " minimumMinor = " << MSGID_FORMAT8(minimum_minor_version) << " , FindBehavior is "
                       << (minimum_find_behavior ? "minimumMinorVersion" : "exactOrAnyMinorVersion");

    if (minimum_find_behavior) {
        if ((major_version == major_to_compare || major_version == SOMEIP_DEFAULT_ANY_MAJOR ||
             major_to_compare == SOMEIP_DEFAULT_ANY_MAJOR) &&
            (minimum_minor_version <= minor_to_compare)) {
            return true;
        }
    } else {
        if ((major_version == major_to_compare || major_version == SOMEIP_DEFAULT_ANY_MAJOR ||
             major_to_compare == SOMEIP_DEFAULT_ANY_MAJOR) &&
            (minor_version == minor_to_compare || minor_version == SOMEIP_DEFAULT_ANY_MINOR ||
             minor_to_compare == SOMEIP_DEFAULT_ANY_MINOR)) {
            return true;
        }
    }

    return false;
}

template <typename T> std::string int_to_hex(T i, int size) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(size) << std::hex << i;
    return stream.str();
}

bool ServiceManager::write_current_state_someip_services(std::map<std::uint16_t, std::string> application_list) {
    struct RapidjsonDoc {
        rapidjson::Document doc;
        rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
        rapidjson::Value providedServices;
        rapidjson::Value requiredServices;
    };
    std::map<std::uint16_t, struct RapidjsonDoc> list_documents;

    LGSOMEIP_LOG_INFO << "ServiceManager::write_current_state_someip_services / start";

    for (auto& element : application_list) {
        LGSOMEIP_LOG_INFO << "ServiceManager::write_current_state_someip_services / "
                          << format_named_id("AppID", element.first, 4) << ", app_name: " << element.second;
        list_documents[element.first].doc.SetObject();
        list_documents[element.first].providedServices.SetArray();
        list_documents[element.first].requiredServices.SetArray();
    }

    LGSOMEIP_LOG_INFO
        << "ServiceManager::write_current_state_someip_services / Contents of available_service_list_ (Raw dump)";

    for (auto& service : available_service_list_) {
        std::uint16_t service_id = service.first;
        for (auto& instance : service.second) {
            std::uint16_t instance_id = instance.first;
            std::uint16_t app_id = instance.second.app_id;
            bool is_internal = instance.second.is_internal;
            bool is_in_config_file = instance.second.is_in_config_file;
            std::uint16_t major_version = instance.second.major;
            std::uint32_t minor_version = instance.second.minor;
            std::string tcp_addr;
            std::string udp_addr;

            auto config = configuration_->get_service_info(service_id, instance_id);

            auto& app_doc = list_documents[app_id];

            if (instance.second.tcp_address != nullptr) {
                tcp_addr = instance.second.tcp_address->to_string();
            }
            if (instance.second.udp_address != nullptr) {
                udp_addr = instance.second.udp_address->to_string();
            }

            if (Logger::instance().is_enabled(Logger::LogLevel::Info)) {
                std::ostringstream log_message;
                log_message << "Provider " << format_service_instance_id(service_id, instance_id) << " ";
                if (config != nullptr) {
                    log_message << "serviceName: " << config->get_service_name().c_str();
                }
                log_message << " " << format_named_id("AppID", app_id, 4)
                            << ", is_internal: " << (is_internal ? "true" : "false")
                            << ", is_in_config_file: " << (is_in_config_file ? "true" : "false")
                            << ", majorVersion: " << major_version << ", minorVersion: " << minor_version;
                if (!tcp_addr.empty()) {
                    log_message << ", tcpAddr: " << tcp_addr;
                }
                if (!udp_addr.empty()) {
                    log_message << ", udpAddr: " << udp_addr;
                }
                LGSOMEIP_LOG_INFO << log_message.str();
            }

            rapidjson::Value subscribed_services(rapidjson::kObjectType);
            std::map<std::uint16_t, std::vector<uint16_t>> subscribed_eventgroup_for_app;
            rapidjson::Value tmp_value_str;
            std::string tmp_str;

            auto& eventgrouplist = instance.second.subscribe;
            for (auto& eventgroup : eventgrouplist) {
                std::uint16_t eventgroup_id = eventgroup.first;
                rapidjson::Value subscibers(rapidjson::kObjectType);

                auto& subscribelist = eventgroup.second;
                auto subscribe_it = std::begin(subscribelist);
                while (subscribe_it != std::end(subscribelist)) {
                    auto& subscribe = *subscribe_it;

                    if (Logger::instance().is_enabled(Logger::LogLevel::Info)) {
                        std::ostringstream log_message;
                        log_message << " - " << format_named_id("EventGroupID", eventgroup_id, 4) << ", "
                                    << format_named_id("AppID", subscribe.app_id, 4)
                                    << ", ip_address: " << subscribe.ip_address;
                        if (subscribe.tcp_address != nullptr) {
                            log_message << ", tcp_address: " << subscribe.tcp_address->to_string();
                        }
                        if (subscribe.udp_address != nullptr) {
                            log_message << ", udp_address: " << subscribe.udp_address->to_string();
                        }
                        LGSOMEIP_LOG_INFO << log_message.str();
                    }

                    // Add this subscriber in the application document unless it is an internal one.
                    if (is_in_config_file == true && application_list.find(app_id) != application_list.end()) {
                        if (subscribe.app_id != 0 &&
                            application_list.find(subscribe.app_id) != application_list.end()) {
                            tmp_str = int_to_hex(subscribe.app_id, 4) + "_" + application_list[subscribe.app_id];
                            tmp_value_str.SetString(tmp_str.c_str(), tmp_str.length(), app_doc.alloc);
                            subscibers.AddMember("Internal(appId)", tmp_value_str, app_doc.alloc);
                        } else if (subscribe.app_id == 0) {
                            if (subscribe.tcp_address != nullptr) {
                                tmp_value_str.SetString(subscribe.tcp_address->to_string().c_str(),
                                                        subscribe.tcp_address->to_string().length(), app_doc.alloc);
                                subscibers.AddMember("External(TCP)", tmp_value_str, app_doc.alloc);
                            }
                            if (subscribe.udp_address != nullptr) {
                                tmp_value_str.SetString(subscribe.udp_address->to_string().c_str(),
                                                        subscribe.udp_address->to_string().length(), app_doc.alloc);
                                subscibers.AddMember("External(UDP)", tmp_value_str, app_doc.alloc);
                            }
                        }
                    }

                    if (is_in_config_file == true && subscribe.app_id != 0 &&
                        application_list.find(subscribe.app_id) != application_list.end()) {
                        // Add this subscription eventgroup into each application document
                        subscribed_eventgroup_for_app[subscribe.app_id].push_back(eventgroup_id);
                    }

                    subscribe_it++;
                }

                tmp_value_str.SetString(int_to_hex(eventgroup_id, 4).c_str(), int_to_hex(eventgroup_id, 4).length(),
                                        app_doc.alloc);
                subscribed_services.AddMember(tmp_value_str, subscibers, app_doc.alloc);
            }

            // Add this providedService in the application document unless it is an internal one.
            if (is_in_config_file == true) {
                if (config != nullptr && application_list.find(app_id) != application_list.end()) {
                    rapidjson::Value provided_service(rapidjson::kObjectType);
                    rapidjson::Value tmp_value_str;

                    tmp_value_str.SetString(config->get_service_name().c_str(), config->get_service_name().length(),
                                            app_doc.alloc);
                    provided_service.AddMember("name", tmp_value_str, app_doc.alloc);
                    tmp_value_str.SetString(int_to_hex(service_id, 4).c_str(), int_to_hex(service_id, 4).length(),
                                            app_doc.alloc);
                    provided_service.AddMember("service", tmp_value_str, app_doc.alloc);
                    tmp_value_str.SetString(int_to_hex(instance_id, 4).c_str(), int_to_hex(instance_id, 4).length(),
                                            app_doc.alloc);
                    provided_service.AddMember("instance", tmp_value_str, app_doc.alloc);
                    tmp_value_str.SetString(int_to_hex(major_version, 4).c_str(), int_to_hex(major_version, 4).length(),
                                            app_doc.alloc);
                    provided_service.AddMember("major_version", tmp_value_str, app_doc.alloc);
                    tmp_value_str.SetString(int_to_hex(minor_version, 8).c_str(), int_to_hex(minor_version, 8).length(),
                                            app_doc.alloc);
                    provided_service.AddMember("minorVersion", tmp_value_str, app_doc.alloc);
                    if (instance.second.tcp_address != nullptr) {
                        provided_service.AddMember("tcp_port", instance.second.tcp_address->get_port_address(),
                                                   app_doc.alloc);
                    }
                    if (instance.second.udp_address != nullptr) {
                        provided_service.AddMember("udp_port", instance.second.udp_address->get_port_address(),
                                                   app_doc.alloc);
                    }
                    provided_service.AddMember("subscribers", subscribed_services, app_doc.alloc);

                    app_doc.providedServices.PushBack(provided_service, app_doc.alloc);
                }
            }

            // Add the gathered subscription information into each application document
            for (auto& element : subscribed_eventgroup_for_app) {
                rapidjson::Value subscribed_service_for_app(rapidjson::kObjectType);
                rapidjson::Value subscribed_service_for_app_group(rapidjson::kArrayType);
                if (config != nullptr) {
                    tmp_value_str.SetString(config->get_service_name().c_str(), config->get_service_name().length(),
                                            app_doc.alloc);
                } else {
                    tmp_value_str.SetString("no-name", 7, app_doc.alloc);
                }
                subscribed_service_for_app.AddMember("name", tmp_value_str, app_doc.alloc);
                tmp_value_str.SetString(int_to_hex(service_id, 4).c_str(), int_to_hex(service_id, 4).length(),
                                        app_doc.alloc);
                subscribed_service_for_app.AddMember("service", tmp_value_str, app_doc.alloc);
                tmp_value_str.SetString(int_to_hex(instance_id, 4).c_str(), int_to_hex(instance_id, 4).length(),
                                        app_doc.alloc);
                subscribed_service_for_app.AddMember("instance", tmp_value_str, app_doc.alloc);
                tmp_value_str.SetString(int_to_hex(major_version, 4).c_str(), int_to_hex(major_version, 4).length(),
                                        app_doc.alloc);
                subscribed_service_for_app.AddMember("major_version", tmp_value_str, app_doc.alloc);
                tmp_value_str.SetString(int_to_hex(minor_version, 8).c_str(), int_to_hex(minor_version, 8).length(),
                                        app_doc.alloc);
                subscribed_service_for_app.AddMember("minorVersion", tmp_value_str, app_doc.alloc);

                if (is_in_config_file == true && application_list.find(app_id) != application_list.end()) {
                    tmp_str = int_to_hex(app_id, 4) + "_" + application_list[app_id];
                    tmp_value_str.SetString(tmp_str.c_str(), tmp_str.length(), app_doc.alloc);
                    subscribed_service_for_app.AddMember("provider(appId)", tmp_value_str, app_doc.alloc);
                } else if (is_in_config_file == true && app_id == 0) {
                    if (!tcp_addr.empty()) {
                        tmp_value_str.SetString(tcp_addr.c_str(), tcp_addr.length(), app_doc.alloc);
                        subscribed_service_for_app.AddMember("provider(TCP)", tmp_value_str, app_doc.alloc);
                    }
                    if (!udp_addr.empty()) {
                        tmp_value_str.SetString(udp_addr.c_str(), udp_addr.length(), app_doc.alloc);
                        subscribed_service_for_app.AddMember("provider(UDP)", tmp_value_str, app_doc.alloc);
                    }
                }

                for (auto group : element.second) {
                    tmp_value_str.SetString(int_to_hex(group, 4).c_str(), int_to_hex(group, 4).length(), app_doc.alloc);
                    subscribed_service_for_app_group.PushBack(tmp_value_str, app_doc.alloc);
                }

                subscribed_service_for_app.AddMember("subscribed_eventgroup", subscribed_service_for_app_group,
                                                     app_doc.alloc);

                list_documents[element.first].requiredServices.PushBack(subscribed_service_for_app, app_doc.alloc);
            }
        }
    }

    // Write the documents to each application file
    for (auto& element : application_list) {
        char write_buffer[65536];
        auto& app_doc = list_documents[element.first];

        app_doc.doc.AddMember("Provided_services", app_doc.providedServices, app_doc.alloc);
        app_doc.doc.AddMember("Required_services", app_doc.requiredServices, app_doc.alloc);

        // Create a folder to store the files
        std::string dump_dir = SOMEIP_SOCKET_PATH;
        dump_dir = dump_dir + "dumpservices/";
        if (access(dump_dir.c_str(), 0) != 0) {
            if (mkdir(dump_dir.c_str(), S_IRWXU) == 0) {
                // set permission of domain socket directory
                chmod(dump_dir.c_str(), S_IRWXU);
            }
        }
        // Make the file name for each application
        std::string file_name = dump_dir + element.second + ".json";
        FILE* fp = fopen(file_name.c_str(), "w");
        if (fp == nullptr) {
            LGSOMEIP_LOG_ERROR << "ServiceManager::write_current_state_someip_services / Cannot open " << file_name;
            continue;
        }

        // Write
        rapidjson::FileWriteStream os(fp, write_buffer, sizeof(write_buffer));
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        app_doc.doc.Accept(writer);

        fclose(fp);
    }

    LGSOMEIP_LOG_INFO << "ServiceManager::write_current_state_someip_services / end";

    return true;
}

} // namespace lgsomeip
