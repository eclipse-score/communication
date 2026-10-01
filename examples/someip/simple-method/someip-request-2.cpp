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

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0001
#define SOMEIP_MAJOR_VERSION 0x01
#define SOMEIP_MINOR_VERSION 0x000000

std::string appname = "request-2";
ApplicationManager appMgmt(appname);

std::map<std::uint32_t, TimeCheck> timecheck;
int cnt = 0;
int sendcnt = 0;

void on_message(std::shared_ptr<Message> msg) {
    std::uint32_t reqid = msg->get_request_id();
    timecheck[reqid].end();

    if (cnt % 1000 == 0) {
        std::cout << appname << " / on_message Called : MessageID = 0x" << std::hex << msg->get_message_id()
                  << " / response time = " << std::dec << timecheck[reqid].get_duration() * 1000000 << " microsecond"
                  << std::endl;
    }

    cnt++;
}

void on_availability(std::uint16_t serviceid, std::uint16_t instanceid, bool available) {
    std::cout << appname << " on_availability Called!! / state = " << available << std::endl;
    if (available) {
        appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
    } else {
        appMgmt.unregister_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID);
    }
}

void on_state(std::uint16_t state) {
    std::cout << appname << " on_state Called!! / state = " << state << std::endl;

    appMgmt.request_service(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, SOMEIP_MINOR_VERSION);
    appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
}

int main() {
    appMgmt.init();
    appMgmt.register_application_state_handler(on_state);
    appMgmt.start();

    while (true) {
        auto message =
            MessageBuilder::create_request_message(SOMEIP_SERVICE_ID, SOMEIP_METHOD_ID, SOMEIP_MAJOR_VERSION);

        appMgmt.send(message);
        std::uint32_t reqid = message->get_request_id();
        timecheck[reqid].start();

        usleep(50);

        sendcnt++;
        if (sendcnt == 100000)
            break;
    }

    sleep(1);

    std::cout << appname << " Message : send cnt = " << sendcnt << " - recv cnt = " << cnt << std::endl;
    appMgmt.stop();

    return 0;
}
