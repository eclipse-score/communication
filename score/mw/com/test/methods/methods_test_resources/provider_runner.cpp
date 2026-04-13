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
#include "score/mw/com/test/methods/methods_test_resources/provider_runner.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/test_method_datatype.h"

#include "score/mw/com/types.h"
#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace score::mw::com::test
{

namespace
{
std::array<std::atomic<std::uint16_t>, kMaxRegisteredMethos> gMethodCallCounts{0U, 0U, 0U, 0U, 0U};
std::array<std::atomic<std::uint16_t>, kMaxRegisteredMethos> gConcurrentCalls{0U, 0U, 0U, 0U, 0U};
std::array<std::atomic<std::uint16_t>, kMaxRegisteredMethos> gOccuranceOfConcurrentCalls{0U, 0U, 0U, 0U, 0U};

using InpArgType = MultiMethodSkeleton::InpArgType;
using ReturnType = MultiMethodSkeleton::ReturnType;

auto handler_maker(InpArgType idx)
{
    return [idx](InpArgType val) -> ReturnType {
        const auto count = gMethodCallCounts.at(idx).fetch_add(1) + 1;
        auto current_call_numner = gConcurrentCalls.at(idx).fetch_add(1);
        if (current_call_numner > 0)
        {
            gOccuranceOfConcurrentCalls.at(idx).fetch_add(1);
        }

        // Simulate some processing work to increase the time window for concurrent execution
        // This makes it more likely that multiple handlers will be executing at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::cout << "Provider: method" << idx << " called (count=" << count << ") with val=" << val << '\n';
        gConcurrentCalls.at(idx).fetch_add(-1);
        return val * (kMethodResultMultiplierBase + idx);
    };
}

void RegisterMethodHandlers(MultiMethodSkeleton& skeleton)
{
    skeleton.method0.RegisterHandler(handler_maker(0));
    skeleton.method1.RegisterHandler(handler_maker(1));
    skeleton.method2.RegisterHandler(handler_maker(2));
    skeleton.method3.RegisterHandler(handler_maker(3));
    skeleton.method4.RegisterHandler(handler_maker(4));
    std::cout << "Provider: All 5 method handlers registered successfully.\n";
}

auto CreateAndOfferSkeleton() -> score::Result<MultiMethodSkeleton>
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierSV});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Provider: Invalid instance specifier\n";
        return score::MakeUnexpected<MultiMethodSkeleton>(instance_specifier_result.error());
    }

    auto skeleton_result = MultiMethodSkeleton::Create(instance_specifier_result.value());
    if (!skeleton_result.has_value())
    {
        std::cerr << "Provider: Could not create skeleton: " << skeleton_result.error().Message() << '\n';
        return skeleton_result;
    }

    auto& skeleton = skeleton_result.value();
    RegisterMethodHandlers(skeleton);

    auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Provider: Could not offer service: " << offer_result.error().Message() << '\n';
        return score::MakeUnexpected<MultiMethodSkeleton>(offer_result.error());
    }

    std::cout << "Provider: Service offered successfully\n";
    return skeleton_result;
}
}  // namespace

void run_provider(const score::cpp::stop_token& stop_token,
                  const std::vector<std::size_t>& expected_method_call_count,
                  std::size_t num_consumer)
{
    auto skeleton_result = CreateAndOfferSkeleton();
    fail_test_if(!skeleton_result.has_value(), "Provider: Failed to create and offer skeleton.");

    std::vector<std::unique_ptr<ProcessSynchronizer>> process_synchronizers;
    for (std::size_t i{0}; i < num_consumer; ++i)
    {
        process_synchronizers.emplace_back(
            ProcessSynchronizer::CreateUniquePtr(CreateInterprocessNotificationShmPath(i)));

        fail_test_if(!process_synchronizers.at(i), "Provider: Could not create ProcessSynchronizer ", i);
    }

    std::cout << "Provider: Ready for method calls from multiple proxies\n";

    for (std::size_t i{0}; i < process_synchronizers.size(); ++i)
    {
        std::cout << "Provider: Waiting for Consumer" << i << " to finish...\n";
        if (!process_synchronizers.at(i)->WaitWithAbort(stop_token))
        {
            fail_test("Provider: WaitWithAbort for Consumer", i, " was stopped by stop_token\n");
        }
    }
    std::cout << "Provider: All consumers have finished...\n\n";

    std::size_t idx{0};
    for (const auto& method_call_count : gMethodCallCounts)
    {
        std::cout << "idx: " << idx << " count: " << method_call_count.load();
        std::cout << " Recorded number of concurrent calls: " << gOccuranceOfConcurrentCalls.at(idx) << "\n";
        if (method_call_count.load() != expected_method_call_count.at(idx))
        {
            std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            std::cout << "Provider: method" << idx << " called unexpected amount of times.\n";
            std::cout << "idx: " << idx << " count: " << method_call_count.load() << "\n";
            std::cout << "Recorded number of concurrent calls: " << gOccuranceOfConcurrentCalls.at(idx) << "\n";
            std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            fail_test();
        }
        idx++;
    }
    std::cout << "Provider: Shutting down.\n";
}

auto ParseConsumerRepetitionFromArgs(int argc, const char** argv, std::size_t num_consumers)
    -> std::pair<std::vector<std::size_t>, CommandLineArgsMapType>
{
    std::vector<std::size_t> consumer_repetition{};

    std::vector<std::pair<std::string, std::string>> arg_names_and_descriptions{};
    for (std::size_t consumer_id = 0; consumer_id < num_consumers; ++consumer_id)
    {
        const auto arg_name = std::string{"num-proxies-per-consumer"} + std::to_string(consumer_id);
        const auto arg_description =
            std::string{"number of times consumer"} + std::to_string(consumer_id) + std::string{" calls its methods"};
        arg_names_and_descriptions.emplace_back(arg_name, arg_description);
    }

    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, arg_names_and_descriptions);

    // for (std::size_t consumer_id{0}; consumer_id < num_consumer; ++consumer_id)
    for (auto& [arg_name, _] : arg_names_and_descriptions)
    {
        auto arg_value_opt = GetValueIfProvided<std::size_t>(args, arg_name);
        if (!arg_value_opt.has_value())
        {
            std::stringstream strstr;
            strstr << "Provider: --" << arg_name << " parameter is not optional.\n";
            fail_test(strstr.str());
        }
        consumer_repetition.emplace_back(arg_value_opt.value());
    }
    return {consumer_repetition, args};
}

auto CalculateExpectedMethodCallCounts(const std::vector<std::size_t>& consumer_repetition,
                                       std::vector<std::vector<std::size_t>> enabled_method_ids)
    -> std::vector<std::size_t>
{
    std::vector<std::size_t> expected_method_call_count(kMaxRegisteredMethos, 0U);

    for (std::size_t consumer_id{0}; consumer_id < enabled_method_ids.size(); ++consumer_id)
    {
        for (const auto method_id : enabled_method_ids.at(consumer_id))
        {
            expected_method_call_count.at(method_id) += consumer_repetition.at(consumer_id);
        }
    }
    return expected_method_call_count;
}

}  // namespace score::mw::com::test
