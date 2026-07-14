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
#include "score/mw/com/doc/tutorial/chapter_8/calculator_service.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

// In this chapter the provider offers a single instance of the CalculatorService.
constexpr std::string_view kInstanceSpecifierString{"MyCalculatorServiceInstance"};

// A method provider is purely reactive: the registered handlers are invoked by the middleware whenever a consumer calls
// a method. There is nothing to poll in main(), so - just like the event-driven consumer of chapter 5 - the main thread
// simply blocks on this condition variable until a termination signal (SIGINT/SIGTERM) requests the shut down. The
// signal handler sets the flag and notifies the condition variable.
static std::mutex g_shutdown_mutex{};
static std::condition_variable g_shutdown_cv{};
static std::atomic<bool> g_shutdown_requested{false};

static void SignalHandler(int /*signum*/)
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto service_instance_result = score::mw::com::tutorial::CalculatorSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_result.has_value(),
                                                "Failed to create CalculatorSkeleton instance!");
    auto service_instance = std::move(service_instance_result).value();

    // Register the implementation of the "add" method. With the rebased method API a handler no longer *returns* the
    // result; instead the return value is passed to the handler as its first argument by (mutable) reference - i.e. as
    // an out-parameter - followed by the (deserialized) input arguments by const-reference. The handler writes the
    // result into that out-parameter and returns void. Handlers MUST be registered before the service is offered.
    const auto add_handler_result = service_instance.add_.RegisterHandler(
        [](std::uint64_t& result, const std::uint32_t& lhs, const std::uint32_t& rhs) -> void {
            result = static_cast<std::uint64_t>(lhs) + static_cast<std::uint64_t>(rhs);
            std::cout << "add(" << lhs << ", " << rhs << ") = " << result << std::endl;
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(add_handler_result.has_value(),
                                                "Failed to register 'add' method handler!");

    // Register the implementation of the "arithmetic_mean" method. As above, the return value is the first
    // out-parameter (by reference); the (potentially large) IntegerArray argument follows by const-reference, so no
    // copy of the array happens on the provider side either. We only evaluate the first GetNumberOfElements() entries.
    const auto mean_handler_result = service_instance.arithmetic_mean_.RegisterHandler(
        [](std::uint32_t& result, const score::mw::com::tutorial::IntegerArray& array) -> void {
            const std::size_t count = array.GetNumberOfElements();
            if (count == 0U)
            {
                std::cout << "arithmetic_mean called with an empty array -> 0" << std::endl;
                result = 0U;
                return;
            }

            std::uint64_t sum{0U};
            for (std::size_t index = 0U; index < count; ++index)
            {
                sum += array.internal_array_[index];
            }
            result = static_cast<std::uint32_t>(sum / count);
            std::cout << "arithmetic_mean over " << count << " elements = " << result << std::endl;
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(mean_handler_result.has_value(),
                                                "Failed to register 'arithmetic_mean' method handler!");

    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer CalculatorSkeleton instance!");
    std::cout << "OfferService: Calculator service instance is now offered. Waiting for method calls ..." << std::endl;

    // A method provider is purely reactive: the registered handlers are invoked by the middleware (on an internal
    // thread) whenever a consumer calls a method. There is nothing to poll here, so the main thread simply blocks until
    // a termination signal requests the shut down.
    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    std::cout << "Calculator service provider going down." << std::endl;
    return EXIT_SUCCESS;
}
