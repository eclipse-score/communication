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

#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <unistd.h>

#include <message/Message.h>
#include <runtime/ApplicationManager.h>
#include <utils/time/TimeCheck.h>

using namespace std;
using namespace lgsomeip;

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0002 // WRONG MESSAGE ID (Server does not know this method id)

std::string appname = "wrongmethod-test";
ApplicationManager appMgmt(appname);
bool response_done = false;

std::mutex method_mutex;
std::condition_variable condition_var;

void on_message(std::shared_ptr<Message> msg) {
    std::cout << "[Wrong Method Request Test] Response was arrived even though wrong method id used. What happens?"
              << std::endl;

    response_done = true;
    condition_var.notify_one();
}

void on_availability(std::uint16_t serviceid, std::uint16_t instanceid, bool available) {
    std::cout << "[Wrong Method Request Test] available is " << available << std::endl;
    if (available) {
        appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
    } else {
        appMgmt.unregister_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID);
    }
}

void on_state(std::uint16_t state) {
    std::cout << "[Wrong Method Request Test] state is " << state << std::endl;
    if (state) {
        appMgmt.request_service(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID);
        appMgmt.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_METHOD_ID, on_message);
    }
}

int main() {
    appMgmt.init();
    appMgmt.register_application_state_handler(on_state);
    appMgmt.start();

    auto message = MessageBuilder::create<SOMEIP>();
    message->set_message_id(SOMEIP_SERVICE_ID << 16 | SOMEIP_METHOD_ID);
    message->set_interface_version(0x00);
    message->set_message_type(SOMEIP_MESSAGE_TYPE::REQUEST);

    appMgmt.send(message);

    std::unique_lock<std::mutex> lock(method_mutex);
    condition_var.wait(lock, [&] { return response_done; });

    std::cout << "[Wrong Method Request Test] Done" << std::endl;
    appMgmt.stop();

    return 0;
}
