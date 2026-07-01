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

#include "score/json/json_writer.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <utility>

// The number of leading array elements the consumer fills and asks the provider to average.
constexpr std::size_t kNumberOfMeanElements{500U};

// The main thread blocks on this condition variable until either the method calls have completed or a termination
// signal (SIGINT/SIGTERM) requests the shut down.
static std::mutex g_shutdown_mutex{};
static std::condition_variable g_shutdown_cv{};
static std::atomic<bool> g_shutdown_requested{false};

static void RequestShutdown()
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}

static void SignalHandler(int /*signum*/)
{
    RequestShutdown();
}

using CalculatorProxy = score::mw::com::tutorial::CalculatorProxy;

namespace
{

// Returns a binding-independent, human-readable description of the service instance a handle refers to. The only
// portable (public API) way to obtain a representation of the instance id from a handle is:
//   HandleType::GetInstanceId() -> ServiceInstanceId::Serialize() -> score::json::Object
std::string DescribeInstanceId(const CalculatorProxy::HandleType& handle)
{
    score::json::JsonWriter json_writer{};
    auto serialized_instance_id = json_writer.ToBuffer(handle.GetInstanceId().Serialize());
    if (!serialized_instance_id.has_value())
    {
        return std::string{"<unknown>"};
    }

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

// Demonstrates the "copy" call operator: the arguments (two small scalars) are passed by const-reference and the
// middleware copies them into the shared-memory slot for us. This is the most convenient way to call a method whose
// arguments are cheap to copy.
void CallAdd(CalculatorProxy& proxy, std::mt19937& random_engine)
{
    std::uniform_int_distribution<std::uint32_t> value_distribution{};
    const std::uint32_t lhs = value_distribution(random_engine);
    const std::uint32_t rhs = value_distribution(random_engine);

    std::cout << "Calling add(" << lhs << ", " << rhs << ") using the copy call operator ..." << std::endl;
    auto call_result = proxy.add_(lhs, rhs);
    if (!call_result.has_value())
    {
        std::cerr << "'add' call failed: " << call_result.error() << ". Terminating." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    // The result is returned as a MethodReturnTypePtr; dereference it to access the actual value.
    std::cout << "add returned: " << *(call_result.value()) << std::endl;
}

// Demonstrates the "zero-copy" call operator: for a (potentially large) argument we first Allocate() the argument
// directly inside the shared-memory slot, fill it in place and then invoke the method with the obtained MethodInArgPtr.
// This avoids copying the large IntegerArray.
void CallArithmeticMean(CalculatorProxy& proxy, std::mt19937& random_engine)
{
    std::cout << "Calling arithmetic_mean(...) using the zero-copy call operator ..." << std::endl;

    // Step 1: Allocate the argument storage. On success we get a tuple with one MethodInArgPtr<IntegerArray> pointing
    // right into the shared-memory slot the argument will be transported in.
    auto allocate_result = proxy.arithmetic_mean_.Allocate();
    if (!allocate_result.has_value())
    {
        std::cerr << "Could not allocate 'arithmetic_mean' argument: " << allocate_result.error() << ". Terminating."
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }
    auto& [array_ptr] = allocate_result.value();

    // Step 2: Fill the array *in place* (through the pointer into the shared-memory slot - no copy of the array).
    std::uniform_int_distribution<std::uint32_t> value_distribution{0U, 100U};
    for (std::size_t index = 0U; index < kNumberOfMeanElements; ++index)
    {
        array_ptr->internal_array_[index] = value_distribution(random_engine);
    }
    array_ptr->SetNumberOfElements(kNumberOfMeanElements);

    // Step 3: Invoke the method with the filled-in argument pointer.
    auto call_result = proxy.arithmetic_mean_(std::move(array_ptr));
    if (!call_result.has_value())
    {
        std::cerr << "'arithmetic_mean' call failed: " << call_result.error() << ". Terminating." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::cout << "arithmetic_mean over " << kNumberOfMeanElements << " elements returned: " << *(call_result.value())
              << std::endl;
}

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyCalculatorServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // Stable storage for the discovered proxy. It is populated by the find-service handler and lives for the whole
    // lifetime of the application.
    std::optional<CalculatorProxy> proxy{};

    // This handler is invoked asynchronously as soon as the searched service instance becomes available. It creates the
    // proxy, stops the discovery (a single instance is all we need) and then performs the two method calls. Any
    // unrecoverable error terminates the application immediately.
    auto find_service_handler = [&proxy](score::mw::com::ServiceHandleContainer<CalculatorProxy::HandleType> handles,
                                         score::mw::com::FindServiceHandle find_service_handle) noexcept {
        // The very first invocation is guaranteed to carry at least one matching handle. We stop the search right
        // here, so this handler is only ever called once.
        score::cpp::ignore = CalculatorProxy::StopFindService(find_service_handle);

        const auto& handle = handles.front();
        std::cout << "Found Calculator service instance (instance id: " << DescribeInstanceId(handle)
                  << "). Creating proxy." << std::endl;

        auto proxy_result = CalculatorProxy::Create(handle);
        if (!proxy_result.has_value())
        {
            std::cerr << "Failed to create proxy: " << proxy_result.error() << ". Terminating." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        proxy.emplace(std::move(proxy_result).value());

        std::mt19937 random_engine{std::random_device{}()};

        // 1. Call "add" using the by-value / copy call operator.
        CallAdd(proxy.value(), random_engine);

        // 2. Call "arithmetic_mean" using the zero-copy Allocate() call operator.
        CallArithmeticMean(proxy.value(), random_engine);

        std::cout << "All method calls completed. Shutting down." << std::endl;
        RequestShutdown();
    };

    auto find_service_handle_result =
        CalculatorProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result.has_value())
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // The method calls happen event-driven in the find-service handler (on an internal middleware thread). The main
    // thread simply blocks until the calls have completed or a termination signal requests the shut down.
    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    // Stop the asynchronous service discovery before shutting down. This is a no-op if the find-service handler already
    // stopped it (i.e. if the service instance had been found).
    score::cpp::ignore = CalculatorProxy::StopFindService(find_service_handle_result.value());
    std::cout << "Calculator service consumer going down." << std::endl;

    return EXIT_SUCCESS;
}
