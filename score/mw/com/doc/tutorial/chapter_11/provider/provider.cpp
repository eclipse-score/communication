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
#include "../long_running_service.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

using score::mw::com::tutorial::LongRunningServiceResultProxy;
using score::mw::com::tutorial::LongRunningServiceSkeleton;
using score::mw::com::tutorial::SerializedInstanceIdentifier;

namespace
{

// The provider offers a single instance of the LongRunningService.
constexpr std::string_view kInstanceSpecifierString{"MyLongRunningServiceInstance"};

// A method provider is purely reactive: the registered handler is invoked by the middleware whenever a consumer calls
// PostLongRunningJob(). There is nothing to poll in main(), so the main thread simply blocks on this condition variable
// until a termination signal (SIGINT/SIGTERM) requests the shut down.
std::mutex g_shutdown_mutex{};
std::condition_variable g_shutdown_cv{};
std::atomic<bool> g_shutdown_requested{false};

void SignalHandler(int /*signum*/)
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}

// Cache of the *deserialized* callback InstanceIdentifiers, keyed by their serialized form. A consumer calls
// PostLongRunningJob() repeatedly, always passing the same serialized InstanceIdentifier of its callback instance.
// InstanceIdentifier::Create() must be called at most ONCE per distinct service-type deployment (it registers that
// deployment in the runtime configuration; a second registration of the same type would terminate the process).
// Therefore we deserialize each distinct InstanceIdentifier exactly once and hand out (cheap) copies afterwards.
std::mutex g_callback_cache_mutex{};
std::unordered_map<std::string, score::mw::com::InstanceIdentifier> g_callback_identifiers{};
// Cache of the callback proxies, keyed by the same serialized InstanceIdentifier. Creating a proxy performs a
// FindService() plus the shared-memory/message-passing setup for the method, so we do it once per distinct callback
// instance and reuse it afterwards. Held via shared_ptr so the (detached) timer threads can keep a proxy alive for as
// long as a result is still pending.
std::unordered_map<std::string, std::shared_ptr<LongRunningServiceResultProxy>> g_callback_proxies{};

// Serializes the SetJobResult() callback invocations (the proxy method is not meant to be called concurrently, and the
// method request/response queue is sized 1).
std::mutex g_callback_call_mutex{};

// Deserializes (once) and caches the callback InstanceIdentifier for the given serialized form. This is the heart of
// the chapter: the serialized string is turned back into a fully self-contained InstanceIdentifier via
// InstanceIdentifier::Create(), even though this callback instance does NOT appear in the provider's own configuration.
// Returns false if deserialization failed.
bool EnsureCallbackIdentifier(const std::string& serialized_instance_id)
{
    std::lock_guard<std::mutex> lock{g_callback_cache_mutex};
    if (g_callback_identifiers.find(serialized_instance_id) != g_callback_identifiers.end())
    {
        return true;
    }

    auto instance_identifier_result = score::mw::com::InstanceIdentifier::Create(std::string{serialized_instance_id});
    if (!instance_identifier_result.has_value())
    {
        std::cerr << "Failed to create InstanceIdentifier from serialized string: "
                  << instance_identifier_result.error() << std::endl;
        return false;
    }
    g_callback_identifiers.emplace(serialized_instance_id, std::move(instance_identifier_result).value());
    return true;
}

// Creates - or returns the cached - callback proxy for the given serialized InstanceIdentifier. Uses the (already
// deserialized and cached) InstanceIdentifier to perform a *targeted* FindService(): as the InstanceIdentifier fully
// describes the callback instance, this returns exactly the one matching handle - no ambiguous "find any" search is
// needed. The whole find/create is serialized by g_callback_cache_mutex, so concurrent timer threads will not create
// the same proxy twice.
std::shared_ptr<LongRunningServiceResultProxy> GetOrCreateCallbackProxy(const std::string& serialized_instance_id)
{
    std::lock_guard<std::mutex> lock{g_callback_cache_mutex};

    const auto cached = g_callback_proxies.find(serialized_instance_id);
    if (cached != g_callback_proxies.end())
    {
        return cached->second;
    }

    const auto identifier = g_callback_identifiers.find(serialized_instance_id);
    if (identifier == g_callback_identifiers.end())
    {
        std::cerr << "No cached InstanceIdentifier for the callback instance." << std::endl;
        return nullptr;
    }

    // A targeted, synchronous find for exactly this callback instance identifier (using a copy of the cached
    // identifier).
    auto find_result =
        LongRunningServiceResultProxy::FindService(score::mw::com::InstanceIdentifier{identifier->second});
    if (!find_result.has_value())
    {
        std::cerr << "FindService for the callback instance identifier failed: " << find_result.error() << std::endl;
        return nullptr;
    }
    if (find_result.value().empty())
    {
        std::cerr << "FindService for the callback instance identifier returned no handle." << std::endl;
        return nullptr;
    }

    auto proxy_result = LongRunningServiceResultProxy::Create(find_result.value().front());
    if (!proxy_result.has_value())
    {
        std::cerr << "Failed to create callback proxy: " << proxy_result.error() << std::endl;
        return nullptr;
    }

    auto proxy = std::make_shared<LongRunningServiceResultProxy>(std::move(proxy_result).value());
    g_callback_proxies.emplace(serialized_instance_id, proxy);
    std::cout << "Created and cached a new callback proxy for a consumer's callback instance." << std::endl;
    return proxy;
}

// Reconstructs the serialized InstanceIdentifier string (a NUL-padded fixed-size buffer) into a std::string.
std::string ToSerializedString(const SerializedInstanceIdentifier& buffer)
{
    const std::size_t length = ::strnlen(buffer.data(), buffer.size());
    return std::string{buffer.data(), length};
}

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto service_instance_result = LongRunningServiceSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_result.has_value(),
                                                "Failed to create LongRunningServiceSkeleton instance!");
    auto service_instance = std::move(service_instance_result).value();

    // Register the implementation of PostLongRunningJob(). The handler receives the bool return value as its first
    // out-parameter (by reference), followed by the three input arguments by const-reference. It does NOT run the job
    // synchronously: it just deserializes/caches the consumer's callback InstanceIdentifier and arms a timer. The
    // actual FindService/proxy creation and the callback call happen later, on the timer thread (outside this
    // middleware handler thread). Handlers MUST be registered before OfferService.
    const auto handler_result = service_instance.post_long_running_job_.RegisterHandler(
        [](bool& queued,
           const std::uint32_t& job_argument,
           const std::uint32_t& job_number,
           const SerializedInstanceIdentifier& serialized_callback_id) -> void {
            const std::string serialized_instance_id = ToSerializedString(serialized_callback_id);
            std::cout << "PostLongRunningJob(job_argument=" << job_argument << ", job_number=" << job_number
                      << ") received. Resolving callback instance ..." << std::endl;

            if (!EnsureCallbackIdentifier(serialized_instance_id))
            {
                std::cerr << "Could not deserialize the callback InstanceIdentifier for job_number=" << job_number
                          << ". Job NOT queued." << std::endl;
                queued = false;
                return;
            }

            // Arm a "timer" that fires after a random delay between 1s and 10s, then delivers the result via the
            // callback. We simply use a detached thread that sleeps. On first fire it lazily creates (and caches) the
            // callback proxy for the given serialized InstanceIdentifier; subsequent jobs reuse the cached proxy.
            std::thread{[serialized_instance_id, job_number]() {
                std::mt19937 random_engine{std::random_device{}()};
                const std::chrono::seconds delay{std::uniform_int_distribution<std::uint32_t>{1U, 10U}(random_engine)};
                std::this_thread::sleep_for(delay);

                auto callback_proxy = GetOrCreateCallbackProxy(serialized_instance_id);
                if (callback_proxy == nullptr)
                {
                    std::cerr << "Could not obtain a callback proxy for job_number=" << job_number
                              << ". Result NOT delivered." << std::endl;
                    return;
                }

                const std::uint32_t job_result = std::uniform_int_distribution<std::uint32_t>{}(random_engine);

                std::cout << "Long running job_number=" << job_number << " finished (result=" << job_result
                          << "). Calling SetJobResult() on the consumer's callback instance ..." << std::endl;

                std::lock_guard<std::mutex> lock{g_callback_call_mutex};
                const auto call_result = callback_proxy->set_job_result_(job_result, job_number);
                if (!call_result.has_value())
                {
                    std::cerr << "SetJobResult() call for job_number=" << job_number
                              << " failed: " << call_result.error() << std::endl;
                }
            }}.detach();

            queued = true;
            std::cout << "Job_number=" << job_number << " queued. Result will be delivered via callback." << std::endl;
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(handler_result.has_value(),
                                                "Failed to register 'PostLongRunningJob' method handler!");

    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer LongRunningServiceSkeleton instance!");
    std::cout << "OfferService: LongRunningService instance is now offered. Waiting for PostLongRunningJob calls ..."
              << std::endl;

    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    std::cout << "LongRunningService provider going down." << std::endl;
    return EXIT_SUCCESS;
}
