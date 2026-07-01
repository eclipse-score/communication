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
#include "score/mw/com/doc/tutorial/chapter_11/long_running_service.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

using score::mw::com::tutorial::LongRunningServiceProxy;
using score::mw::com::tutorial::LongRunningServiceResultSkeleton;
using score::mw::com::tutorial::SerializedInstanceIdentifier;

namespace
{

// The consumer consumes this instance of the LongRunningService ...
constexpr std::string_view kServiceInstanceSpecifierString{"MyLongRunningServiceInstance"};
// ... and provides this instance of the callback service (LongRunningServiceResult).
constexpr std::string_view kResultInstanceSpecifierString{"MyLongRunningServiceResultInstance"};

// The consumer posts a new job every 500ms.
constexpr std::chrono::milliseconds kPostCycleTime{500};

// The main thread blocks/loops until a termination signal (SIGINT/SIGTERM) requests the shut down.
std::mutex g_shutdown_mutex{};
std::condition_variable g_shutdown_cv{};
std::atomic<bool> g_shutdown_requested{false};

// Signals that the LongRunningService proxy has been discovered and created (populated by the find-service handler).
std::mutex g_proxy_ready_mutex{};
std::condition_variable g_proxy_ready_cv{};

void RequestShutdown()
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_all();
    g_proxy_ready_cv.notify_all();
}

void SignalHandler(int /*signum*/)
{
    RequestShutdown();
}

// Turns the (default-constructed, i.e. NUL-padded) fixed-size buffer holding the serialized InstanceIdentifier of our
// callback instance into the buffer we pass as the third argument of PostLongRunningJob(). Aborts if the serialized
// form does not fit (which would mean SerializedInstanceIdentifier is too small).
SerializedInstanceIdentifier SerializeCallbackInstanceIdentifier(const std::string_view serialized_view)
{
    SerializedInstanceIdentifier buffer{};
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        serialized_view.size() < buffer.size(),
        "Serialized InstanceIdentifier does not fit into SerializedInstanceIdentifier buffer!");
    std::memcpy(buffer.data(), serialized_view.data(), serialized_view.size());
    return buffer;
}

}  // namespace

int main(int argc, const char* argv[])
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    // 1. Create and offer our own callback service instance (LongRunningServiceResult). The provider will later call
    //    SetJobResult() on exactly this instance.
    auto result_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kResultInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(result_specifier.has_value(),
                                                "Failed to create callback InstanceSpecifier!");

    auto result_skeleton_result = LongRunningServiceResultSkeleton::Create(result_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(result_skeleton_result.has_value(),
                                                "Failed to create LongRunningServiceResultSkeleton instance!");
    auto result_skeleton = std::move(result_skeleton_result).value();

    // Register the SetJobResult() handler. It has no return value (void method), so the handler only receives the two
    // input arguments by const-reference. It simply logs the delivered result together with its job_number. Handlers
    // MUST be registered before OfferService().
    const auto set_result_handler_result = result_skeleton.set_job_result_.RegisterHandler(
        [](const std::uint32_t& job_result, const std::uint32_t& job_number) -> void {
            std::cout << "Callback SetJobResult received: job_number=" << job_number << " has result=" << job_result
                      << "." << std::endl;
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(set_result_handler_result.has_value(),
                                                "Failed to register 'SetJobResult' method handler!");

    auto result_offer_result = result_skeleton.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(result_offer_result.has_value(),
                                                "Failed to offer LongRunningServiceResultSkeleton instance!");
    std::cout << "Callback service instance offered." << std::endl;

    // 2. Resolve the InstanceIdentifier of our just-offered callback instance and serialize it. ResolveInstanceIDs()
    //    performs the InstanceSpecifier -> InstanceIdentifier lookup in our configuration; we expect exactly ONE
    //    InstanceIdentifier (the "instances" array holds exactly one element - see the chapter text). Its ToString()
    //    representation is what we hand to the provider so it can later find *this* callback instance.
    auto resolve_result = score::mw::com::runtime::ResolveInstanceIDs(result_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(resolve_result.has_value(),
                                                "Failed to resolve callback InstanceIdentifier!");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(resolve_result.value().size() == 1U,
                                                "Expected exactly one InstanceIdentifier for the callback instance!");
    const std::string_view serialized_callback_id_view = resolve_result.value().front().ToString();
    const SerializedInstanceIdentifier serialized_callback_id =
        SerializeCallbackInstanceIdentifier(serialized_callback_id_view);
    std::cout << "Serialized callback InstanceIdentifier (" << serialized_callback_id_view.size()
              << " bytes) prepared for PostLongRunningJob calls." << std::endl;

    // 3. Discover the LongRunningService and create a proxy for it.
    auto service_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kServiceInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_specifier.has_value(),
                                                "Failed to create service InstanceSpecifier!");

    std::optional<LongRunningServiceProxy> proxy{};
    auto find_service_handler = [&proxy](
                                    score::mw::com::ServiceHandleContainer<LongRunningServiceProxy::HandleType> handles,
                                    score::mw::com::FindServiceHandle find_service_handle) noexcept {
        score::cpp::ignore = LongRunningServiceProxy::StopFindService(find_service_handle);

        std::cout << "Found LongRunningService instance. Creating proxy." << std::endl;
        auto proxy_result = LongRunningServiceProxy::Create(handles.front());
        if (!proxy_result.has_value())
        {
            std::cerr << "Failed to create LongRunningService proxy: " << proxy_result.error() << ". Terminating."
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
        {
            std::lock_guard<std::mutex> lock{g_proxy_ready_mutex};
            proxy.emplace(std::move(proxy_result).value());
        }
        g_proxy_ready_cv.notify_one();
    };

    auto find_service_handle_result =
        LongRunningServiceProxy::StartFindService(std::move(find_service_handler), service_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(find_service_handle_result.has_value(),
                                                "Failed to start service discovery for LongRunningService!");

    // Wait until the proxy has been created (or a shutdown was requested in the meantime).
    {
        std::unique_lock<std::mutex> lock{g_proxy_ready_mutex};
        g_proxy_ready_cv.wait(lock, [&proxy] {
            return proxy.has_value() || g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    // 4. Cyclically post long running jobs until a shutdown is requested. The results are delivered asynchronously via
    //    the SetJobResult() callback handler registered above (invoked on an internal middleware thread).
    if (proxy.has_value())
    {
        std::mt19937 random_engine{std::random_device{}()};
        std::uniform_int_distribution<std::uint32_t> value_distribution{};
        std::uint32_t job_number{0U};

        while (!g_shutdown_requested.load(std::memory_order_relaxed))
        {
            const std::uint32_t job_argument = value_distribution(random_engine);
            std::cout << "Posting long running job_number=" << job_number << " (job_argument=" << job_argument << ")."
                      << std::endl;
            auto call_result = proxy->post_long_running_job_(job_argument, job_number, serialized_callback_id);
            if (!call_result.has_value())
            {
                std::cerr << "PostLongRunningJob call failed: " << call_result.error() << std::endl;
            }
            else
            {
                std::cout << "  PostLongRunningJob(job_number=" << job_number
                          << ") queued at provider: " << std::boolalpha << *(call_result.value()) << std::endl;
            }
            ++job_number;

            std::unique_lock<std::mutex> lock{g_shutdown_mutex};
            g_shutdown_cv.wait_for(lock, kPostCycleTime, [] {
                return g_shutdown_requested.load(std::memory_order_relaxed);
            });
        }
    }

    score::cpp::ignore = LongRunningServiceProxy::StopFindService(find_service_handle_result.value());
    std::cout << "LongRunningService consumer going down." << std::endl;
    return EXIT_SUCCESS;
}
