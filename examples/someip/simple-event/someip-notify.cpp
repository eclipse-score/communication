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
#include <thread>
#include <atomic>
#include <unistd.h>

#include <message/Message.h>
#include <runtime/ApplicationManager.h>

using namespace std;
using namespace lgsomeip;

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0001
#define SOMEIP_MAJOR_VERSION 0x01

#define SOMEIP_EVENT_GROUP 0x4455
#define SOMEIP_EVENT_ID_1 0x8777
#define SOMEIP_EVENT_ID_2 0x8778

std::string appname = "notify";
std::shared_ptr<ApplicationManager> appMgmt;

std::atomic<bool> runstate{true};
std::shared_ptr<std::thread> runthread{nullptr};

void run_notify() {
    std::shared_ptr<Payload> message = std::make_shared<Payload>();
    std::uint8_t data[100] = "DATA";
    message->set_payload(data, 4);

    while (runstate) {
        data[0] = (data[0] == 'D') ? 'F' : 'D';
        std::cout << "notify / change message!!" << std::endl;
        message->set_payload(data, 4);

        std::cout << "notify / runNotify / Send Notification" << std::endl;
        appMgmt->notify(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1, message);
        appMgmt->notify(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2, message);
        sleep(1);
    }
}

void on_availability(std::uint16_t serviceid, std::uint16_t instanceid, bool available) {
    std::cout << "notify App / on_availability Called!! / state = " << available << std::endl;
}

void on_state(std::uint16_t state) {
    std::cout << "notify App / on_state Called!! / state = " << state << std::endl;
    if (state) {
        std::set<std::uint16_t> eventgroups{SOMEIP_EVENT_GROUP};
        appMgmt->offer_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1, eventgroups);
        appMgmt->offer_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2, eventgroups);

        appMgmt->offer_service(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, 0);

        runstate = true;
        runthread = std::make_shared<std::thread>(run_notify);
    } else {
        runstate = false;
        runthread->join();
        runthread.reset();
    }
}

int main() {
    appMgmt = std::make_shared<ApplicationManager>(appname);
    appMgmt->init();
    appMgmt->register_application_state_handler(on_state);
    appMgmt->start();

    while (true) {
        sleep(1);
    }

    return 0;
}
