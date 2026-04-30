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
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/consumer_runner.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include <iostream>

int main(int argc, const char** argv)
{
    const auto test_configuration = score::mw::com::test::ReadCommandLineArguments(argc, argv);

    const auto& enabled_method_ids = score::mw::com::test::kEnabledMethodIDs.at(test_configuration.consumer_id);
    score::mw::com::test::InitializeRuntime(test_configuration.service_instance_manifest);
    score::mw::com::test::run_consumer(test_configuration, enabled_method_ids);
    return EXIT_SUCCESS;
}
