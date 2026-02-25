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

#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include <score/utility.hpp>
#include <csignal>
#include <iostream>

namespace score::mw::com
{

namespace
{

score::cpp::stop_source k_stop_test{score::cpp::nostopstate_t{}};

void SigTermHandlerFunction(int signal)
{
    if (signal == SIGTERM || signal == SIGINT)
    {
        std::cout << "Stop requested\n";
        score::cpp::ignore = k_stop_test.request_stop();
    }
}

}  // namespace

bool SetupStopTokenSigTermHandler(score::cpp::stop_source& stop_test)
{
    k_stop_test = stop_test;
    const bool setup_failed = ((std::signal(SIGTERM, SigTermHandlerFunction) == SIG_ERR) ||
                               (std::signal(SIGINT, SigTermHandlerFunction) == SIG_ERR));
    return !setup_failed;
}

}  // namespace score::mw::com
