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
uint16_t service_id = 0x1001;

void on_message(std::shared_ptr<Message> msg) {
    if (msg->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
        auto message = MessageBuilder::create_response_message(*msg);
        message->set_payload(msg->get_payload(), msg->get_length() - 8);
        application_manager->send(message);
    }
}

void on_availability(std::uint16_t service_id, std::uint16_t instance_id, bool available) {
    std::cout << "Response App / on_availability Called!! / state = " << available << std::endl;
}

void on_state(std::uint16_t state) {
    std::cout << "Response App / on_state Called!! / state = " << state << std::endl;
    application_manager->register_message_handler(service_id, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
    application_manager->offer_service(service_id, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, SOMEIP_MINOR_VERSION);
}

int main(int argc, char** argv) {
    service_id = (uint16_t)strtol(argv[1], NULL, 16) + 0x1000;
    std::string app_number = argv[1];
    application_name = "response" + app_number;
    application_manager = new ApplicationManager(application_name);
    application_manager->init();
    application_manager->register_application_state_handler(on_state);
    application_manager->start();

    while (true) {
        sleep(1);
    }

    return 0;
}
