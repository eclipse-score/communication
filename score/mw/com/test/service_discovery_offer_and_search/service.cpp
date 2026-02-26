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

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/test/common_test_resources/common_service.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/common_test_resources/sync_utils.h"

#include "score/mw/com/test/service_discovery_offer_and_search/test_datatype.h"

#include "score/result/error.h"
#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

namespace score::mw::com::test
{

using namespace std::chrono_literals;

int run_service(const std::chrono::milliseconds& cycle_time, const score::cpp::stop_token& stop_token)
{
    auto first_service_result = Service<TestDataSkeleton>::Create(kInstanceSpecifierStringServiceFirst);
    if (!first_service_result.has_value())
    {
        std::cerr << first_service_result.error().Message() << std::endl;
        return 1;
    }
    const auto first_offer_service_result = first_service_result.value().OfferService(kTestValue);
    if (!first_offer_service_result.has_value())
    {
        std::cerr << "Could not offer first service, terminating\n";
        return 3;
    }

    auto second_service_result = Service<TestDataSkeleton>::Create(kInstanceSpecifierStringServiceSecond);
    if (!second_service_result.has_value())
    {
        std::cerr << second_service_result.error().Message() << std::endl;
        return 2;
    }
    const auto second_offer_service_result = second_service_result.value().OfferService(kTestValue);
    if (!second_offer_service_result.has_value())
    {
        std::cerr << "Could not offer second service, terminating\n";
        return 3;
    }

    // Offer Service first then Sync. with Client to start searching for service
    score::mw::com::test::SyncCoordinator sync_coordinator(std::string{kFileName});
    sync_coordinator.Signal();
    std::cout << "Sending Sync. Signal to Client" << std::endl;

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(cycle_time);
    }

    return 0;
}

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::CYCLE_TIME};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto cycle_time = run_parameters.GetCycleTime();
    const auto stop_token = test_runner.GetStopToken();

    return score::mw::com::test::run_service(cycle_time, stop_token);
}
