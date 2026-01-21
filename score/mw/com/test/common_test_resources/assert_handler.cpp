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

#include "score/mw/com/test/common_test_resources/assert_handler.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{

namespace
{

void assert_handler(const score::cpp::handler_parameters& params)
{
    std::cerr << "Assertion \"" << params.condition << "\" failed";
    if (params.message != nullptr)
    {
        std::cerr << ": " << params.message;
    }
    std::cerr << " (" << params.file << ':' << params.line << ")" << std::endl;
    std::cerr.flush();

    score::mw::log::LogFatal("AsHa") << params.condition << params.message << params.file << params.line;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    const char* const no_abort = std::getenv("ASSERT_NO_CORE");
    if (no_abort != nullptr)
    {
        std::cerr << "Would not coredump on \"" << no_abort << "\"" << std::endl;
        if (std::strcmp(no_abort, params.condition) == 0)
        {
            std::cerr << "... matched." << std::endl;
            std::cerr.flush();
            std::quick_exit(1);
        }
        std::cerr << "... not matched." << std::endl;
    }
    std::cerr.flush();
}

}  // namespace

void SetupAssertHandler()
{
    score::cpp::set_assertion_handler(assert_handler);
    // in addition, delay the calls to std::terminate() till the datarouter is able to read the logs
    std::set_terminate([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::abort();
    });
}

}  // namespace score::mw::com::test
