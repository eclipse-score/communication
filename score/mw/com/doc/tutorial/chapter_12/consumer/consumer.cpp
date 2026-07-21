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
#include "proxy_component.h"
#include "score/mw/com/doc/tutorial/chapter_12/hello_world_service.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>

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

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    std::optional<score::mw::com::tutorial::HelloWorldProxy::HandleType> service_handle{};
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto service_handle_container_result =
            score::mw::com::tutorial::HelloWorldProxy::FindService(instance_specifier.value());
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

    auto proxy_result = score::mw::com::tutorial::HelloWorldProxy::Create(service_handle.value());
    if (!proxy_result)
    {
        std::cerr << "Failed to create HelloWorldProxy: " << proxy_result.error() << std::endl;
        exit(1);
    }

    score::mw::com::tutorial::ProxyComponent proxy_component(std::move(proxy_result).value());

    const auto subscribe_result = proxy_component.Subscribe();
    if (!subscribe_result)
    {
        std::cerr << "Failed to subscribe to HelloWorldService instance 'message' event!" << std::endl;
        exit(1);
    }

    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        proxy_component.GetNewSamples();
    }
    std::cout << "HelloWorld service consumer going down." << std::endl;
}
