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
#include <cstring>
#include <iostream>
#include <signal.h>

#include <message/Message.h>
#include <runtime/ServiceManager.h>
#include <utils/log/logger.h>

using namespace std;
using namespace lgsomeip;

static std::condition_variable signal_handler_condition;
static std::mutex signal_handler_mutex;
static bool signal_received = false;

void handle_signal(int signal_number) {
    std::lock_guard<std::mutex> its_lock(signal_handler_mutex);

    signal_received = true;
    signal_handler_condition.notify_all();
}

int main(int argc, char* argv[]) {
    // Handle the following signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    try {
        std::string config_path("");
        if (argc >= 2) {
            config_path = argv[1];
        }
        if (config_path == "-v") {
            LGSOMEIP_LOG_INFO << "LG SOME/IP Version : " << SOMEIP_VERSION << "\n";
            return 0;
        }

        std::string appname = "daemon";
        ServiceManager svc_mgmt(appname, config_path);

        svc_mgmt.init();
        svc_mgmt.start();

        LGSOMEIP_LOG_INFO << "LG SOME/IP Daemon v" << SOMEIP_VERSION << " Start!!";

        std::unique_lock<std::mutex> its_lock(signal_handler_mutex);
        while (!signal_received) {
            signal_handler_condition.wait(its_lock);
        }
        LGSOMEIP_LOG_INFO << "LG SOME/IP Daemon is shutting down!!!";
        svc_mgmt.stop();
    } catch (const std::exception& exception) {
        std::cerr << "LG SOME/IP Daemon failed to start: " << exception.what() << std::endl;
        return 1;
    }

    return 0;
}
