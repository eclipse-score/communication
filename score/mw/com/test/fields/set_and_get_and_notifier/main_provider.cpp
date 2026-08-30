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

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/common_resources.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/provider.h"

#include <cstdlib>
#include <iostream>

int main(int argc, const char** argv)
{
    const auto config = score::mw::com::test::ParseConfig(argc, argv);

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{config.config_file_path});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    score::mw::com::test::run_provider(stop_source.get_token(), config.mode);
    return EXIT_SUCCESS;
}
