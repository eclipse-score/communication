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
#include "score/mw/com/doc/tutorial/chapter_3/hello_world_service.h"
#include "score/mw/com/types.h"

#include "score/json/json_writer.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service consumer caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;

namespace
{

// Bundles a proxy for a discovered service instance together with a human-readable description of its instance id
// (used for logging).
struct DiscoveredInstance
{
    HelloWorldProxy proxy;
    std::string instance_id_description;
};

// Returns a binding-independent, human-readable description of the service instance a handle refers to.
//
// The only portable (public API) way to obtain a representation of the instance id from a handle is:
//   HandleType::GetInstanceId() -> ServiceInstanceId::Serialize() -> score::json::Object
// We then stringify that json object so it can be logged.
std::string DescribeInstanceId(const HelloWorldProxy::HandleType& handle)
{
    score::json::JsonWriter json_writer{};
    auto serialized_instance_id = json_writer.ToBuffer(handle.GetInstanceId().Serialize());
    if (!serialized_instance_id.has_value())
    {
        return std::string{"<unknown>"};
    }

    // The json writer pretty-prints with newlines and indentation. Collapse any run of whitespace into a single space
    // so that the instance id can be logged as a compact single-line description.
    const std::string pretty_printed = std::move(serialized_instance_id).value();
    std::string compact{};
    compact.reserve(pretty_printed.size());
    bool previous_was_space{false};
    for (const char character : pretty_printed)
    {
        const bool is_space = (character == ' ') || (character == '\n') || (character == '\r') || (character == '\t');
        if (is_space)
        {
            if (!previous_was_space)
            {
                compact.push_back(' ');
            }
        }
        else
        {
            compact.push_back(character);
        }
        previous_was_space = is_space;
    }
    return compact;
}

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // The consumer performs a "find-any" search: The InstanceSpecifier "MyHelloWorldServiceInstance" is mapped in the
    // consumer configuration (mw_com_config.json) to the HelloWorldService type *without* an explicit instanceId.
    // Therefore, the search will match *any* instance of the HelloWorldService type.
    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // State shared between the (asynchronously called) find-service handler and the main polling loop below.
    std::mutex discovered_instances_mutex{};
    std::vector<DiscoveredInstance> discovered_instances{};
    std::set<HelloWorldProxy::HandleType> known_handles{};

    // This handler is invoked asynchronously by score::mw::com whenever the set of matching service instances changes.
    // It receives *all* currently matching handles. We create a proxy for every newly discovered instance.
    auto find_service_handler = [&discovered_instances_mutex, &discovered_instances, &known_handles](
                                    score::mw::com::ServiceHandleContainer<HelloWorldProxy::HandleType> handles,
                                    score::mw::com::FindServiceHandle) noexcept {
        std::lock_guard<std::mutex> lock{discovered_instances_mutex};
        for (const auto& handle : handles)
        {
            // The handler is called with the full set of matching handles each time. Skip the ones we already handle.
            if (known_handles.find(handle) != known_handles.end())
            {
                continue;
            }

            const auto instance_id_description = DescribeInstanceId(handle);
            std::cout << "Found new HelloWorld service instance (instance id: " << instance_id_description
                      << "). Creating proxy." << std::endl;

            auto proxy_result = HelloWorldProxy::Create(handle);
            if (!proxy_result)
            {
                std::cerr << "Failed to create HelloWorldProxy: " << proxy_result.error() << std::endl;
                continue;
            }
            auto hello_world_proxy = std::move(proxy_result).value();

            const auto subscribe_result = hello_world_proxy.message.Subscribe(1);
            if (!subscribe_result)
            {
                std::cerr << "Failed to subscribe to 'message' event of instance " << instance_id_description << ": "
                          << subscribe_result.error() << std::endl;
                continue;
            }

            known_handles.insert(handle);
            discovered_instances.push_back(DiscoveredInstance{std::move(hello_world_proxy), instance_id_description});
        }
    };

    auto find_service_handle_result =
        HelloWorldProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        exit(1);
    }

    // Main loop: poll the "message" event of every discovered service instance for new samples.
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::lock_guard<std::mutex> lock{discovered_instances_mutex};
        for (auto& discovered_instance : discovered_instances)
        {
            const auto& instance_id_description = discovered_instance.instance_id_description;
            auto get_new_samples_result = discovered_instance.proxy.message.GetNewSamples(
                [&instance_id_description](auto&& sample) {
                    const auto* buf = sample.Get()->data();
                    std::cout << "Sample received from instance " << instance_id_description << ": " << buf
                              << std::endl;
                },
                1);
            if (!get_new_samples_result)
            {
                std::cerr << "Failed to get new samples from instance " << instance_id_description << ": "
                          << get_new_samples_result.error() << std::endl;
            }
        }
    }

    // Stop the asynchronous service discovery before shutting down.
    score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle_result.value());
    std::cout << "HelloWorld service consumer going down." << std::endl;
}
