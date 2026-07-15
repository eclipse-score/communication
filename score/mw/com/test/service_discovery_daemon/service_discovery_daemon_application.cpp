/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/service_discovery/service_discovery_daemon.h"
#include "score/mw/com/service_discovery/service_discovery_message_passing_server.h"
#include "score/mw/com/service_discovery/service_discovery_registry.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main()
{
    score::mw::com::test::SetupAssertHandler();

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Service discovery daemon app: Unable to set signal handler for SIGINT and/or SIGTERM."
                  << std::endl;
    }

    score::mw::com::service_discovery::ServiceDiscoveryRegistry registry{};
    score::mw::com::service_discovery::ServiceDiscoveryDaemon daemon{registry};
    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingServer server{daemon};

    if (!server.Start())
    {
        std::cerr << "Service discovery daemon app: Failed to start message passing server." << std::endl;
        return EXIT_FAILURE;
    }

    while (!stop_source.get_token().stop_requested())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }

    server.Stop();
    return EXIT_SUCCESS;
}
