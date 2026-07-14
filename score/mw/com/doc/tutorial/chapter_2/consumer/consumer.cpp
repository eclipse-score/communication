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
#include "score/mw/com/doc/tutorial/chapter_2/hello_world_service.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service consumer caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;

int main(int argc, const char* argv[])
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    std::optional<HelloWorldProxy::HandleType> service_handle{};
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto service_handle_container_result = HelloWorldProxy::FindService(instance_specifier.value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_handle_container_result.has_value(),
                                                    "HelloWorldProxy::FindService failed!");
        if (service_handle_container_result.value().empty())
        {
            std::cout << "No HelloWorld service instance found yet. Retrying..." << std::endl;
        }
        else
        {
            std::cout << "HelloWorld service instance found." << std::endl;
            service_handle = service_handle_container_result.value()[0];
            break;
        }
    }

    auto proxy_result = HelloWorldProxy::Create(service_handle.value());
    if (!proxy_result)
    {
        std::cerr << "Failed to create HelloWorldProxy: " << proxy_result.error() << std::endl;
        exit(1);
    }

    auto hello_world_proxy = std::move(proxy_result).value();
    const auto subscribe_result = hello_world_proxy.message.Subscribe(1);
    if (!subscribe_result)
    {
        std::cerr << "Failed to subscribe to HelloWorldService instance 'message' event: " << subscribe_result.error()
                  << std::endl;
        exit(1);
    }

    size_t receive_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto get_new_samples_result = hello_world_proxy.message.GetNewSamples(
            [&receive_counter](auto&& sample) {
                const auto* buf = sample.Get()->data();
                std::cout << "Sample received. Event \"message\" update received: " << buf << std::endl;
                ++receive_counter;
            },
            1);
        if (!get_new_samples_result)
        {
            std::cerr << "Failed to get new samples: " << get_new_samples_result.error() << std::endl;
        }
    }
    std::cout << "HelloWorld service consumer going down." << std::endl;
}
