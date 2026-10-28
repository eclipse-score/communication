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

#include <cstdint>
#include <iostream>
#include <unistd.h>

#include <message/Message.h>
#include <runtime/ApplicationManager.h>
#include <utils/time/TimeCheck.h>

using namespace std;
using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0001
#define SOMEIP_MAJOR_VERSION 0x01
#define SOMEIP_MINOR_VERSION 0x00000001

std::string application_name;
ApplicationManager* application_manager;
uint32_t service_id = 0x1001;

std::map<std::uint16_t, std::chrono::time_point<std::chrono::steady_clock>> time_check;
int received_count = 0;
int sent_count = 0;

void on_message(std::shared_ptr<Message> msg) {
    std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
    std::uint32_t request_id = msg->get_request_id();
    std::chrono::duration<double> difference = end - time_check[request_id];
    time_check.erase(request_id);

    if (received_count % 1000 == 0) {
        std::cout << application_name << " / on_message Called, response time = " << std::dec
                  << difference.count() * 1000000 << " microsecond" << std::endl;
    }

    received_count++;
}

void on_availability(std::uint16_t service_id, std::uint16_t instance_id, bool available) {
    std::cout << application_name << " / on_availability Called!! / state = " << available << std::endl;
    if (available) {
        application_manager->register_message_handler(service_id, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
    } else {
        application_manager->unregister_message_handler(service_id, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID);
    }
}

void on_state(std::uint16_t state) {
    std::cout << application_name << " / on_state Called!! / state = " << state << std::endl;

    application_manager->request_service(service_id, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, SOMEIP_MINOR_VERSION);
    application_manager->register_message_handler(service_id, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
}

int main(int argc, char** argv) {
    service_id = (uint16_t)strtol(argv[1], NULL, 16) + 0x1000; // get the parameter how many services will be started
    std::cout << "service_id = " << std::dec << service_id << "service_id hex = " << std::hex << service_id << std::dec
              << std::endl;
    std::string app_number = argv[1];  // get the parameter how many apps will be started
    int send_interval = stoi(argv[2]); // get the parameter as send interval(microseconds)
    application_name = "request" + app_number;
    application_manager = new ApplicationManager(application_name);
    application_manager->init();
    application_manager->register_application_state_handler(on_state);
    application_manager->start();
    while (true) {
        auto message = MessageBuilder::create<SOMEIP>();
        message->set_message_id(service_id << 16 | 0x0001);
        message->set_interface_version(0x00);
        message->set_message_type(SOMEIP_MESSAGE_TYPE::REQUEST);
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
        application_manager->send(message);
        std::uint32_t request_id = message->get_request_id();
        time_check[request_id] = start;

        usleep(send_interval);

        sent_count++;
        if (sent_count == 1000000)
            break;
    }

    sleep(1);

    std::cout << application_name << " / Message : send cnt = " << sent_count << " - recv cnt = " << received_count
              << std::endl;
    application_manager->stop();

    return 0;
}
