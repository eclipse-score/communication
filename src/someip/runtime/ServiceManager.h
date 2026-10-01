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

#ifndef LG_SOMEIP_SERVICE_MANAGER_H
#define LG_SOMEIP_SERVICE_MANAGER_H

#include <cstdint>

#include <config/Configuration.h>
#include <memory>
#include <message/Message.h>
#include <runtime/ApplicationCommonTypes.h>
#include <runtime/ApplicationConstant.h>
#include <packetrouter/ServiceRouter.h>
#include <utils/time/TimerMux.h>
#include <chrono>
#include <thread>

#define LGSOMEIP_MULTI_ENDPOINT 1
#define LGSOMEIP_UNI_ENDPOINT 0

namespace lgsomeip {

class PacketRouterHost;

class ServiceManager {
public:
    const static std::uint32_t BUFFER_LEN = 65536 + 100;     // To support 64Kbytes including header (about 100bytes)
    const static std::uint8_t RETRY_CONNECTION_MAX_NUM = 3;  // Maximum retry num
    const static std::uint8_t RETRY_CONNECTION_PERIOD = 100; // Retry period (ms)

    ServiceManager(std::string name, std::string config_path = "",
                   std::shared_ptr<ServiceRouter> packet_router = nullptr);
    ~ServiceManager();

    void init();
    void start();
    void stop();

    std::uint16_t get_application_id();
    std::string get_application_name();

    std::shared_ptr<Configuration> get_configuration() {
        return configuration_;
    }

    void on_disconnected_application(std::uint16_t app_id);
    void on_disconnected_service(std::shared_ptr<lgsomeip::osabstraction::Address> addr);

private:
    std::string application_name_;
    std::uint16_t application_id_;

    std::shared_ptr<ServiceRouter> packet_router_;
    std::shared_ptr<Configuration> configuration_;

public:
    void on_internal_message(std::shared_ptr<MessageSD> message);
    void on_external_message(std::shared_ptr<Endpoint> endpoint, std::shared_ptr<MessageSD> message, bool reboot);

private:
    bool check_sd_message(std::shared_ptr<MessageSD> message, std::string sender_ip_address);
    void on_internal_find_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                  std::uint32_t minor_version, std::uint32_t ttl, std::uint32_t request_id);
    void on_external_find_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                  std::uint32_t minor_version, std::uint32_t ttl, std::uint32_t request_id,
                                  bool unicast_flag_set);
    void on_internal_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                   std::uint32_t minor_version, std::uint32_t ttl, std::uint32_t request_id);
    void on_external_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                   std::uint32_t minor_version, std::uint32_t ttl,
                                   std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                   std::shared_ptr<lgsomeip::osabstraction::Address> udp_address,
                                   std::shared_ptr<lgsomeip::osabstraction::Address> received_address);
    void on_internal_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                          std::uint16_t event_group_id, std::uint8_t major_version, std::uint32_t ttl,
                                          std::uint32_t request_id);
    void on_external_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                          std::uint16_t event_group_id, std::uint8_t major_version, std::uint32_t ttl,
                                          std::uint32_t request_id,
                                          std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                                          std::shared_ptr<lgsomeip::osabstraction::Address> udp_address);
    void on_subscribe_eventgroup_ack(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                                     std::uint8_t major_version, std::uint32_t ttl, bool from_internal,
                                     std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr);
    void handle_stop_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                   std::uint32_t minor_version, AvailableService* service_info,
                                   ServiceInfo* config = nullptr);
    void handle_new_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                  std::uint32_t minor_version, std::uint32_t ttl, ServiceInfo* config,
                                  std::uint16_t app_id,
                                  std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address = nullptr,
                                  std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address = nullptr);
    void update_subscribe_info(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                               std::uint8_t major_version, std::uint32_t ttl, std::uint32_t request_id,
                               std::vector<RequestedSubscribe>::iterator subscription);
    void handle_stop_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                          std::uint16_t event_group_id, std::uint8_t major_version, std::uint32_t ttl,
                                          std::uint16_t app_id, AvailableService* service_info,
                                          std::vector<RequestedSubscribe>::iterator subscription,
                                          std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address = nullptr,
                                          std::shared_ptr<lgsomeip::osabstraction::Address> udp_address = nullptr);
    void handle_new_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id,
                                         std::uint16_t event_group_id, std::uint8_t major_version, std::uint32_t ttl,
                                         std::uint32_t request_id, AvailableService* service_info,
                                         std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address = nullptr,
                                         std::shared_ptr<lgsomeip::osabstraction::Address> udp_address = nullptr);

private:
    struct AvailableService* find_available_service_instance(std::uint16_t service_id, std::uint16_t instance_id);
    std::map<std::uint16_t, struct AvailableService*> find_available_service_instance_all(std::uint16_t service_id);

    bool find_option_address(std::shared_ptr<lgsomeip::osabstraction::Address>& tcp_address,
                             std::shared_ptr<lgsomeip::osabstraction::Address>& udp_address, SDOption* first_options,
                             int first_option_count, SDOption* second_options, int second_option_count);
    void check_subscribe_error(SDEntry* entry, std::shared_ptr<lgsomeip::osabstraction::Address> tcp_address,
                               std::shared_ptr<lgsomeip::osabstraction::Address> udp_address,
                               std::string sender_ip_address);
    void remove_service_info(std::shared_ptr<lgsomeip::osabstraction::Address> address);

public:
    void send_internal_offer_service_all(std::uint16_t service_id, std::uint16_t instance_id, std::uint32_t ttl,
                                         std::uint8_t major_version = SOMEIP_DEFAULT_ANY_MAJOR,
                                         std::uint32_t minor_version = SOMEIP_DEFAULT_ANY_MINOR);
    void send_internal_offer_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint32_t ttl,
                                     std::uint8_t major, std::uint32_t minor, std::uint32_t app_id);
    void send_subscribe_eventgroup(std::uint16_t service_id, std::uint16_t instance_id, std::uint32_t event_group_id,
                                   std::uint32_t ttl, std::uint8_t major_version, std::uint16_t app_id,
                                   bool send_alone = true);
    void send_subscribe_eventgroup_ack(std::uint16_t service_id, std::uint16_t instance_id,
                                       std::uint32_t event_group_id, std::uint32_t ttl, std::uint8_t major_version,
                                       std::uint16_t app_id,
                                       std::shared_ptr<lgsomeip::osabstraction::Address> multicast = nullptr,
                                       std::string target_ip = "");
    void
    send_external_offer_service(std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>>& offer_list,
                                std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr,
                                std::uint32_t ttl = 3, bool multicast = true);

    void send_external_find_service(std::uint16_t service_id, std::uint16_t instance_id, std::uint32_t ttl,
                                    std::uint8_t major, std::uint32_t minor);
    void send_external_find_service_all(
        std::map<std::uint16_t, std::map<std::uint16_t, struct RequestedService*>>& find_list);

    // Write the current state of SOME/IP services to files
    bool write_current_state_someip_services(std::map<std::uint16_t, std::string> application_list);

private:
    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService>> available_service_list_;
    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService>> repetition_offer_list_;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> deleted_repetition_offer_list_; // serviceid, instanceid

    std::map<std::uint16_t, std::map<std::uint16_t, std::vector<struct RequestedService>>> request_service_list_;
    std::map<std::uint16_t, std::map<std::uint16_t, std::vector<struct RequestedService>>> repetition_find_list_;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> deleted_repetition_find_list_; // serviceid, instanceid

    std::recursive_mutex available_list_mutex_;
    std::mutex repetition_offer_list_mutex_;
    std::mutex request_list_mutex_;

    std::pair<std::uint16_t, std::uint8_t> get_session_id_and_reboot_flag(std::string address);
    bool check_service_version(std::uint16_t major_version, std::uint16_t major_to_compare, std::uint32_t minor_version,
                               std::uint32_t minor_to_compare, std::uint32_t minimum_minor_version,
                               bool minimum_find_behavior);

    // std::map<address, std::pair<session_id, reboot_flag>>
    std::map<std::string, std::pair<std::uint16_t, std::uint8_t>> session_info_;
    std::mutex session_mutex_;

private:
    friend class PacketRouterHost;

    void on_timer();
    void on_send_cyclic_offer_find_service();
    void on_send_cyclic_repetition_offer_service(std::shared_ptr<MessageSD>& message);
    void on_send_cyclic_main_offer_service(std::shared_ptr<MessageSD>& message);
    void on_send_cyclic_repetition_find_service(std::shared_ptr<MessageSD>& message);

    void on_check_ttl_service();
    void on_check_ttl_subscribe();
    void on_send_magic_cookies();
    void join_add_connection_threads();
    std::shared_ptr<TimerMux> timer_{nullptr};

    std::mutex add_connection_threads_mutex_;
    std::vector<std::thread> add_connection_threads_;

    std::shared_ptr<SOMEIPSD::type> gathered_subscribe_ack_list_;
    std::shared_ptr<SOMEIPSD::type> gathered_subscribe_list_;
    std::map<std::uint16_t, std::map<std::uint16_t, struct AvailableService*>> gathered_offer_list_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SERVICE_MANAGER_H
