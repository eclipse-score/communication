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

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0001
#define SOMEIP_MAJOR_VERSION 0x01
#define SOMEIP_MINOR_VERSION 0x000000

std::string appname = "response";
ApplicationManager appMgmt(appname);

void on_message(std::shared_ptr<Message> msg) {
    std::cout << "Response App / on_message Called!!" << std::endl;
    if (msg->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
        auto message = MessageBuilder::create_response_message(*msg);
        appMgmt.send(message);
    }
}

void on_availability(std::uint16_t serviceid, std::uint16_t instanceid, bool available) {
    std::cout << "Response App / on_availability Called!! / state = " << available << std::endl;
}

void on_state(std::uint16_t state) {
    std::cout << "Response App / on_state Called!! / state = " << state << std::endl;
    appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message, true);
    appMgmt.offer_service(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, SOMEIP_MINOR_VERSION);
}

int main() {
    appMgmt.init();
    appMgmt.register_application_state_handler(on_state);
    appMgmt.start();

    while (true) {
        sleep(1);
    }

    return 0;
}
