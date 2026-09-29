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
#include "score/mw/com/test/fields/getter_setter_if_available/consumer.h"

#include <cstdlib>

int main(int argc, const char** argv)
{
    const auto config = score::mw::com::test::ParseConfig(argc, argv);

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration{config.config_file_path});

    score::mw::com::test::run_consumer(config.mode);
    return EXIT_SUCCESS;
}
