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
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/fields/field_initial_value/test_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace score::mw::com::test
{

namespace
{

const std::string kInterprocessNotificationShmPath{"/field_initial_value_interprocess_notification"};

void run_service(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Service: Unable to create ProcessSynchronizer");
    }

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        FailTest("Service: Unable to create instance specifier");
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto service_result = TestDataSkeleton::Create(std::move(instance_specifier));
    if (!service_result.has_value())
    {
        FailTest("Service: Unable to construct TestDataSkeleton: ", service_result.error());
    }

    TestDataSkeleton& lola_service{service_result.value()};

    const auto update_result = lola_service.test_field.Update(kTestValue);
    if (!update_result.has_value())
    {
        FailTest("Service: Unable to update test field: ", update_result.error());
    }
    const auto offer_result = lola_service.OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Service: Unable to offer service for TestDataSkeleton: ", offer_result.error());
    }

    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Service: WaitWithAbort was stopped by stop_token instead of notification");
    }

    lola_service.StopOfferService();
}

}  // namespace

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    score::mw::com::test::run_service(stop_source.get_token());
    return EXIT_SUCCESS;
}
