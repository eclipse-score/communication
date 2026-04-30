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
#include "score/mw/com/test/methods/methods_test_resources/consumer_runner.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/test_method_datatype.h"

#include "score/mw/com/types.h"

#include <score/jthread.hpp>

#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::test
{

namespace
{

/// \brief create method names in the format "method0", "method1", ..., "methodN" and store them in the provided vector
auto MakeAllRegistrableMethodNames() -> std::vector<std::string>
{
    std::vector<std::string> method_names;
    method_names.reserve(kMaxRegisteredMethos);
    for (std::size_t method_id = 0U; method_id < kMaxRegisteredMethos; ++method_id)
    {
        method_names.emplace_back(std::string{"method"} + std::to_string(method_id));
    }
    return method_names;
}

/// \brief create a static vector of all possible method names and return method names corresponding to the method IDs
auto GetEnabledMethods(const std::vector<std::string>& method_names, const std::vector<std::size_t>& enabled_methods)
    -> std::vector<std::string_view>
{

    std::vector<std::string_view> enabled_method_names;
    for (const auto method_id : enabled_methods)
    {
        // clang-format off
        FailTestIf(method_id > method_names.size(), "Trying to create more methods than allowed", method_id, " is larger than ", method_names.size());
        // clang-format on
        enabled_method_names.emplace_back(method_names.at(method_id));
    }
    return enabled_method_names;
}

void ValidateMethodCallResult(score::Result<MethodReturnTypePtr<MultiMethodProxy::ReturnType>>& result,
                              std::size_t consumer_id,
                              std::size_t method_id,
                              std::int32_t input_value)
{
    if (!result.has_value())
    {
        FailTest("Consumer", consumer_id, ": method", method_id, " call failed: ", result.error().Message());
    }

    const auto actual = *(result.value());
    const auto expected = input_value * (kMethodResultMultiplierBase + static_cast<std::int32_t>(method_id));

    // clang-format off
    FailTestIf(actual != expected,
         "Consumer" , consumer_id , ": method" , method_id , " returned " , actual , " but expected " , expected);
    // clang-format on

    std::cout << "Consumer" << consumer_id << ": method" << method_id << " returned correct value: " << actual << '\n';
}

// template <CopyMode copy_mode, typename Method>
template <typename Proxy>
void CallMethod(Proxy& proxy, CopyMode copy_mode, std::size_t method_id, std::size_t consumer_id, std::int32_t input)
{
    std::cout << "Consumer" << consumer_id << ": Calling a method" << method_id << " with the value " << input << '\n';

    auto& method = [&proxy, consumer_id, method_id]() -> auto& {
        if (method_id == 0U)
        {
            return proxy.method0;
        }
        if (method_id == 1U)
        {
            return proxy.method1;
        }
        if (method_id == 2U)
        {
            return proxy.method2;
        }
        if (method_id == 3U)
        {
            return proxy.method3;
        }
        if (method_id == 4U)
        {
            return proxy.method4;
        }
        // clang-format off
        FailTest("Consumer", consumer_id, ": Invalid method ID ", method_id, " can not be larger than ", kMaxRegisteredMethos);
        // clang-format on

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }();

    auto result = [&method, copy_mode, input]() -> score::Result<MethodReturnTypePtr<MultiMethodProxy::ReturnType>> {
        if (copy_mode == CopyMode::ZERO_COPY)
        {
            auto allocated_args_result = method.Allocate();
            if (!allocated_args_result.has_value())
            {
                std::cerr << "Conumer: Failiure during zero copy call. " << allocated_args_result.error() << '\n';
                return score::Unexpected(allocated_args_result.error());
            }

            auto& [arg_ptr] = allocated_args_result.value();
            *arg_ptr = input;

            return method(std::move(arg_ptr));
        }

        return method(input);
    }();

    ValidateMethodCallResult(result, consumer_id, method_id, input);
}

}  // namespace

auto ReadCommandLineArguments(int argc, const char** argv) -> ConsumerTestConfiguration
{

    auto args = score::mw::com::test::ParseCommandLineArguments(
        argc, argv, {{"consumer_id", ""}, {"num-proxies-per-consumer", ""}, {"service-instance-manifest", ""}});

    ConsumerTestConfiguration test_configuration{};

    const auto consumer_id_opt = GetValueIfProvided<std::size_t>(args, "consumer_id");
    if (!consumer_id_opt.has_value())
    {
        FailTest("Consumer: --consumer_id parameter is not optional.\n");
    }
    test_configuration.consumer_id = consumer_id_opt.value();
    if (test_configuration.consumer_id > 3)
    {
        FailTest("Consumer: consumer_id value ", test_configuration.consumer_id, " is invalid (must be between 0-2)");
    }

    const auto num_proxies_per_process_opt = GetValueIfProvided<std::size_t>(args, "num-proxies-per-consumer");
    if (num_proxies_per_process_opt.has_value())
    {
        test_configuration.num_proxies_per_process = num_proxies_per_process_opt.value();
        if (!(test_configuration.num_proxies_per_process > 0))
        {
            FailTest("Consumer: num-proxies-per-consumer value ",
                      test_configuration.num_proxies_per_process,
                      " must be greater than 0.");
        }
    }

    std::string defuault_pat{"./etc/mw_com_config.json"};
    test_configuration.service_instance_manifest =
        GetValueIfProvided<std::string>(args, "service-instance-manifest").value_or(defuault_pat);

    return test_configuration;
}

void run_consumer(const ConsumerTestConfiguration& test_configuration,
                  const std::vector<std::size_t>& enabled_method_ids)
{
    std::cout << "Starting consumer with ID: " << test_configuration.consumer_id << "\n";

    const auto notification_path = CreateInterprocessNotificationShmPath(test_configuration.consumer_id);

    auto process_synchronizer = ProcessSynchronizer::Create(notification_path);
    if (!process_synchronizer.has_value())
    {
        FailTest("Consumer", test_configuration.consumer_id, ": Could not create the ProcessSynchronizer.\n");
    }

    // Precreation of this vector of strings is necessary because proxy container expects a vector of string views. Thus
    // the GetEnabledMethods function needs to return string views to valid strings. Thus this names need to be created
    // in the outer scope of proxy_runner and must outlive the proxy, created by proxy container.
    auto method_names = MakeAllRegistrableMethodNames();

    auto proxy_runner = [&enabled_method_ids, &method_names](const ConsumerTestConfiguration& test_configuration,
                                                             std::size_t proxy_id) -> void {
        auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierSV});
        // clang-format off
        FailTestIf(!instance_specifier_result.has_value(),
                     "Consumer", test_configuration.consumer_id, ": Proxy", proxy_id, " Got Invalid instance specifier");
        // clang-format on

        ProxyContainer<MultiMethodProxy> proxy_container;

        if (bool proxy_creation_succeeded = proxy_container.CreateProxy(
                instance_specifier_result.value(), GetEnabledMethods(method_names, enabled_method_ids));
            !proxy_creation_succeeded)
        {
            FailTest("Consumer", test_configuration.consumer_id, ": Could not create the proxy with ID ", proxy_id);
        }

        std::cout << "Consumer" << test_configuration.consumer_id << ": Proxy " << proxy_id
                  << " created successfully. Calling Methods\n";

        auto& proxy = proxy_container.GetProxy();
        for (const auto& method_id : enabled_method_ids)
        {
            const auto some_random_input_value = static_cast<std::int32_t>(proxy_id + method_id);
            CopyMode copy_mode = (method_id % 2 == 0) ? CopyMode::ZERO_COPY : CopyMode::WITH_COPY;
            CallMethod(proxy, copy_mode, method_id, test_configuration.consumer_id, some_random_input_value);
        }
    };

    {
        std::vector<score::cpp::jthread> threads;
        threads.resize(test_configuration.num_proxies_per_process);
        for (std::size_t proxy_id = 0U; proxy_id < test_configuration.num_proxies_per_process; ++proxy_id)
        {
            threads.emplace_back(proxy_runner, test_configuration, proxy_id);
        }
    }

    std::cout << "Consumer" << test_configuration.consumer_id << ": Exiting with result.\n";

    process_synchronizer->Notify();
    std::cout << "Consumer" << test_configuration.consumer_id << ": Notified provider of completion.\n";

    std::cout << "Consumer" << test_configuration.consumer_id << ": All threads have completed. Exiting.\n";
}

}  // namespace score::mw::com::test
