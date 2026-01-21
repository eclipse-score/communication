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

#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/field_initial_value/test_datatype.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <thread>
#include <utility>

namespace score::mw::com::test
{

namespace
{

using namespace std::chrono_literals;

int run_service(const std::chrono::milliseconds& cycle_time, const score::cpp::stop_token& stop_token)
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -3;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto service_result = TestDataSkeleton::Create(std::move(instance_specifier));
    if (!service_result.has_value())
    {
        std::cerr << "Unable to construct TestDataSkeleton: " << service_result.error() << ", bailing!\n";
        return -4;
    }

    TestDataSkeleton& lola_service{service_result.value()};

    lola_service.test_field.Update(kTestValue);
    const auto offer_result = lola_service.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for TestDataSkeleton: " << offer_result.error() << ", bailing!\n";
        return -5;
    }

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(cycle_time);
    }

    lola_service.StopOfferService();

    return 0;
}

}  // namespace

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
