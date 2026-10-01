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

#include <atomic>
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

#define SOMEIP_EVENT_GROUP 0x4455
#define SOMEIP_EVENT_ID_1 0x8777
#define SOMEIP_EVENT_ID_2 0x8778

std::string appname = "subscribe-2";
ApplicationManager appMgmt(appname);
int cnt = 0;

void on_message(std::shared_ptr<Message> msg) {
    std::cout << "subscribe-2 / on_message Called : MessageID = 0x" << std::hex << msg->get_message_id()
              << " / Message Len = " << std::dec << msg->get_length() << std::endl;
}

void on_availability(std::uint16_t serviceid, std::uint16_t instanceid, bool available) {
    std::cout << "subscribe-2 / on_availability Called!! / state = " << available << std::endl;
    if (available) {
        appMgmt.subscribe(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_GROUP, 0x01);
    } else {
        appMgmt.unsubscribe(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_GROUP);
    }
}

void on_state(std::uint16_t state) {
    std::cout << "subscribe-2 / on_state Called!! / state = " << state << std::endl;

    appMgmt.request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1, {SOMEIP_EVENT_GROUP});
    appMgmt.request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2, {SOMEIP_EVENT_GROUP});

    appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1, on_message);
    appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2, on_message);
    appMgmt.request_service(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID);
    appMgmt.register_availability_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, on_availability);
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
