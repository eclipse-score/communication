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

/// \brief QM consumer main entry point for mixed criticality method tests.
/// This consumer runs at QM ASIL level and calls methods on a QM provider.

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/consumer_runner.h"
#include "score/mw/com/test/methods/mixed_criticality/common_resources.h"

#include <score/stop_token.hpp>
#include <cstdlib>

namespace
{
}  // namespace

int main(int argc, const char** argv)
{
    auto service_instance_manifest = score::mw::com::test::GetServiceInstanceManifestPath(argc, argv);
    score::mw::com::test::InitializeRuntime(service_instance_manifest);

    score::mw::com::test::SetupAssertHandler();
    score::cpp::stop_source stop_source{};

    auto consumer_id{0U};
    auto num_proxies_per_process{1U};
    score::mw::com::test::ConsumerTestConfiguration test_config{
        consumer_id, num_proxies_per_process, service_instance_manifest};
    score::mw::com::test::run_consumer(test_config, score::mw::com::test::kEnabledMethodIDs[0]);
    return EXIT_SUCCESS;
}
