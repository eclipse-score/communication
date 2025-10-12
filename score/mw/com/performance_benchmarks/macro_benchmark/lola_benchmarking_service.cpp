/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/performance_benchmarks/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/performance_benchmarks/common_test_resources/shared_memory_object_guard.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/config_parser.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/lola_interface.h"
#include "score/mw/com/types.h"
#include "score/mw/log/logging.h"

#include <score/stop_token.hpp>

#include <boost/program_options.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

// LogContext BSrv -> BenchmarkService ;)
constexpr std::string_view kLogContext{"BSrv"};

namespace score::mw::com::test
{

namespace
{

bool RunService(const ServiceConfig& config, score::cpp::stop_token test_stop_token)
{
    auto proxy_is_done_flag_optional = GetSharedFlag();
    if (!proxy_is_done_flag_optional.has_value())
    {
        return false;
    }

    auto& proxy_is_done_flag = proxy_is_done_flag_optional.value();
    const score::mw::com::test::SharedMemoryObjectGuard<CounterType> interprocess_notification_guard{proxy_is_done_flag};

    // zero out the value before starting the service. If this object was not cleaned up propperly an old file from
    // previous run might exist which will hold a value. This will cause bugs.
    proxy_is_done_flag.GetObject() = 0;

    using score::mw::com::test::TestDataSkeleton;

    auto instance_specifier_result = InstanceSpecifier::Create(kLoLaBenchmarkInstanceSpecifier);
    if (!instance_specifier_result.has_value())
    {
        log::LogError(kLogContext) << "instance specifier could not be created.";
        return false;
    }

    auto skeleton_result = TestDataSkeleton::Create(std::move(instance_specifier_result.value()));

    if (!skeleton_result.has_value())
    {
        mw::log::LogInfo(kLogContext) << "Could not create a skeleton. Error: " << skeleton_result.error();
        return false;
    }

    auto& skeleton = skeleton_result.value();
    score::mw::log::LogInfo(kLogContext) << "Skeleton was created.";

    auto offer_service_result = skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        score::mw::log::LogInfo(kLogContext) << "Could not offer Service."
                                           << "Error: " << offer_service_result.error();
        return false;
    }

    score::mw::log::LogInfo(kLogContext) << "Service instance is offered.";

    score::mw::log::LogInfo(kLogContext)
        << "Entering the event send loop.\n"
        << "================================================================================";
    while (!test_stop_token.stop_requested())
    {

        auto sample_allocatee_ptr_result = skeleton.test_event.Allocate();

        if (!sample_allocatee_ptr_result.has_value())
        {
            score::mw::log::LogError(kLogContext)
                << "Could not allocate a sample. " << sample_allocatee_ptr_result.error();
            return false;
        }

        auto sample_allocatee_ptr{std::move(sample_allocatee_ptr_result).value()};

        std::fill(sample_allocatee_ptr->begin(), sample_allocatee_ptr->end(), 1U);

        skeleton.test_event.Send(std::move(sample_allocatee_ptr));

        if (proxy_is_done_flag.GetObject() >= config.number_of_clients)
        {
            score::mw::log::LogError(kLogContext) << "All Proxies have been shutdown.";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{config.send_cycle_time_ms});
    }
    score::mw::log::LogInfo(kLogContext)
        << "Update loop finished\n"
        << "================================================================================";

    skeleton.StopOfferService();
    score::mw::log::LogInfo(kLogContext) << "Stopped offering Service.";

    return true;
}

}  // namespace

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    score::mw::log::LogInfo(kLogContext) << "Starting Service ...";

    score::cpp::stop_source test_stop_source{};
    if (!score::mw::com::test::GetStopTokenAndSetUpSigTermHandler(test_stop_source))
    {
        return EXIT_FAILURE;
    }

    score::mw::log::LogInfo(kLogContext) << "Service: " << "Reading command line arguments.";
    auto args_maybe = score::mw::com::test::ParseCommandLineArgs(argc, argv, kLogContext);
    if (!args_maybe.has_value())
    {
        score::mw::log::LogError(kLogContext)
            << "Service: " << "Something went wrong with command line argument parsing.";
        return EXIT_FAILURE;
    }

    auto args = args_maybe.value();

    score::mw::com::test::InitializeRuntime(args.service_instance_manifest);
    auto config = score::mw::com::test::ParseServiceConfig(args.config_path, kLogContext);

    if (score::mw::com::test::RunService(config, test_stop_source.get_token()))
    {
        return EXIT_SUCCESS;
    };

    return EXIT_FAILURE;
}
