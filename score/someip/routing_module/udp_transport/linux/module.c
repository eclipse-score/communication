/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "score/someip/routing_module/udp_transport/linux/transport.h"
#include "score/someip/routing_module/udp_transport/log.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t running = 1;

static void stop_running(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int main(void)
{
    someip_routing_prepare_logging();
    struct sigaction action;
    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = stop_running;
    (void)sigemptyset(&action.sa_mask);
    if ((sigaction(SIGINT, &action, NULL) != 0) || (sigaction(SIGTERM, &action, NULL) != 0))
    {
        someip_log(SOMEIP_LOG_ERROR, "installing signal handlers failed");
        return EXIT_FAILURE;
    }
    int error = someip_linux_transport_init();
    if (error != 0)
    {
        someip_log(SOMEIP_LOG_ERROR, "initializing Linux UDP transport failed: %d", error);
        return EXIT_FAILURE;
    }
    error = someip_routing_start();
    if (error != 0)
    {
        someip_log(SOMEIP_LOG_ERROR, "starting routing module failed: %d", error);
        someip_linux_transport_close();
        return EXIT_FAILURE;
    }
    while (running)
    {
        error = someip_linux_transport_poll(1000);
        if (error != 0)
        {
            someip_log(SOMEIP_LOG_ERROR, "polling Linux UDP transport failed: %d", error);
            break;
        }
    }
    someip_routing_stop();
    someip_linux_transport_close();
    return (error == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
