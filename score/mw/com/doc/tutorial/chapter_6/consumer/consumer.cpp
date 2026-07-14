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
#include "../hello_world_service.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>

// The interval at which the consumer polls for new samples. It is deliberately a bit longer than the provider's send
// cycle (100 ms in chapter 6), so that on a busy provider there are usually new samples waiting when we poll.
constexpr std::chrono::milliseconds kPollInterval{200};

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;

// Deduce the sample type of the "message" event from the proxy member (public SampleType alias of ProxyEvent) instead
// of referencing internal (impl-namespace) types. The GetNewSamples() receiver is handed a SamplePtr<SampleType>.
using MessageSampleType = decltype(HelloWorldProxy::message)::SampleType;
using MessageSamplePtr = score::mw::com::SamplePtr<MessageSampleType>;

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service consumer caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, const char** argv)
{
    // The consumer takes exactly one command-line argument: the "max_sample_count" it uses when subscribing. This value
    // expresses how many samples (SamplePtrs) the consumer wants to be able to hold in parallel.
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <max_sample_count>" << std::endl;
        return EXIT_FAILURE;
    }
    const auto parsed_max_sample_count = std::atoi(argv[1]);
    if (parsed_max_sample_count <= 0)
    {
        std::cerr << "Invalid max_sample_count '" << argv[1] << "'. It must be a positive integer." << std::endl;
        return EXIT_FAILURE;
    }
    const auto max_sample_count = static_cast<std::size_t>(parsed_max_sample_count);
    std::cout << "Consumer starting with max_sample_count = " << max_sample_count << "." << std::endl;

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // Stable storage for the proxy of the discovered service instance. It is populated by the find-service handler
    // (running on an internal middleware thread) and afterwards only accessed by the main (polling) thread. It lives
    // for the whole lifetime of the application.
    std::optional<HelloWorldProxy> proxy{};

    // Signals to the main thread that the proxy has been created and subscribed and is now ready to be polled. It is
    // set (with release semantics) by the find-service handler after a successful set-up and read (with acquire
    // semantics) by the main thread, which establishes the required happens-before relationship for accessing 'proxy'.
    std::atomic<bool> proxy_ready{false};

    auto find_service_handler = [&proxy, &proxy_ready, max_sample_count](
                                    score::mw::com::ServiceHandleContainer<HelloWorldProxy::HandleType> handles,
                                    score::mw::com::FindServiceHandle find_service_handle) noexcept {
        // The very first invocation is guaranteed to carry at least one matching handle. We only need a single proxy,
        // so we stop the search right here; this handler is therefore only ever called once.
        score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle);

        std::cout << "Found HelloWorld service instance. Creating proxy and subscribing with max_sample_count = "
                  << max_sample_count << "." << std::endl;

        auto proxy_result = HelloWorldProxy::Create(handles.front());
        if (!proxy_result)
        {
            std::cerr << "Failed to create proxy: " << proxy_result.error() << ". Terminating." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        proxy.emplace(std::move(proxy_result).value());

        // Subscribe with the requested max_sample_count. This is the value the provider has to account for when
        // sizing 'numberOfSampleSlots': it must be at least the sum of the max_sample_count of all subscribers
        // (+ 1 as safety margin, see the tutorial README). If our max_sample_count (together with the count already
        // reserved by other subscribers) exceeds the configured 'numberOfSampleSlots', this Subscribe() call fails.
        const auto subscribe_result = proxy.value().message.Subscribe(max_sample_count);
        if (!subscribe_result)
        {
            std::cerr << "Failed to subscribe with max_sample_count = " << max_sample_count << ": "
                      << subscribe_result.error()
                      << ". This typically means that the sum of the subscribers' max_sample_count exceeds the "
                         "provider's configured numberOfSampleSlots. Terminating."
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::cout << "Subscribed to 'message' event." << std::endl;

        proxy_ready.store(true, std::memory_order_release);
    };

    auto find_service_handle_result =
        HelloWorldProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // The container of currently held samples. In contrast to the previous chapters - where the consumer released a
    // received SamplePtr immediately at the end of the GetNewSamples() callback - this consumer keeps the SamplePtrs
    // by moving them out of the callback into this deque. It therefore holds up to 'max_sample_count' samples in
    // parallel, which is exactly what it announced in its Subscribe() call. The oldest sample is at the front.
    std::deque<MessageSamplePtr> held_samples{};

    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(kPollInterval);

        // As long as the find-service handler has not yet created the proxy and subscribed, there is nothing to poll.
        if (!proxy_ready.load(std::memory_order_acquire))
        {
            continue;
        }
        auto& event = proxy.value().message;

        // If we already hold the maximum number of samples, we have to release the oldest one before we can receive a
        // new sample - otherwise we would exceed our own max_sample_count.
        if (held_samples.size() >= max_sample_count)
        {
            held_samples.pop_front();
        }

        // We may receive at most as many new samples as we currently have free capacity for.
        const std::size_t free_capacity = max_sample_count - held_samples.size();
        auto get_new_samples_result = event.GetNewSamples(
            [&held_samples](MessageSamplePtr sample) noexcept {
                std::cout << "Sample received (polling): " << sample.Get()->data()
                          << " - keeping it (currently held: " << (held_samples.size() + 1U) << ")." << std::endl;
                // Keep the sample by moving it out of the callback into our container.
                held_samples.push_back(std::move(sample));
            },
            free_capacity);
        if (!get_new_samples_result)
        {
            std::cerr << "Failed to get new samples: " << get_new_samples_result.error() << std::endl;
        }
    }

    // Stop the asynchronous service discovery before shutting down (no-op if it was already stopped by the handler).
    score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle_result.value());
    std::cout << "HelloWorld service consumer going down." << std::endl;

    return EXIT_SUCCESS;
}
