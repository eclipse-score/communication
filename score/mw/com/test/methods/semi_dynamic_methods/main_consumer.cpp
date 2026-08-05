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
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/methods/semi_dynamic_methods/consumer.h"

int main(int argc, const char** argv)
{
    score::mw::com::test::SetupAssertHandler();
    std::vector<score::safecpp::zstring_view> arguments{};
    for (int i = 0; i < argc; ++i)
    {
        arguments.emplace_back(argv[i], std::strlen(argv[i]));
    }

    score::mw::com::runtime::InitializeRuntime(arguments);
    score::mw::com::test::run_consumer();
    return EXIT_SUCCESS;
}
