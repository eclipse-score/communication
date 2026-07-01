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
#include "score/mw/com/doc/tutorial/chapter_5/hello_world_service.h"
#include "score/mw/com/types.h"

#include "score/json/json_writer.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

// The maximum number of samples the consumer may hold at a time. It is passed to Subscribe() and used as the upper
// bound in the GetNewSamples() calls.
constexpr std::size_t kMaxSampleCount{5U};

// The consumer is purely event-driven: it does no polling at all. The main thread just blocks on this condition
// variable until a termination signal (SIGINT/SIGTERM) requests the shut down. The signal handler sets the flag and
// notifies the condition variable.
static std::mutex g_shutdown_mutex{};
static std::condition_variable g_shutdown_cv{};
static std::atomic<bool> g_shutdown_requested{false};

static void SignalHandler(int /*signum*/)
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;

namespace
{

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

    // The consumer performs an explicit single-instance search: The InstanceSpecifier "MyHelloWorldServiceInstance" is
    // mapped in the consumer configuration (mw_com_config.json) to the HelloWorldService type with an explicit
    // instanceId (1). Therefore, the search matches exactly the one instance the provider offers.
    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // Stable storage for the proxy of the discovered service instance. It is populated by the find-service handler and,
    // from then on, only ever accessed by the (event-driven) receive handler. It lives for the whole lifetime of the
    // application.
    std::optional<HelloWorldProxy> proxy{};

    // This handler is invoked asynchronously by score::mw::com as soon as the searched service instance becomes
    // available. It creates the proxy, registers an event-receive handler and subscribes to the "message" event. As we
    // only need a single proxy, we stop the discovery right away from within this callback. Any unrecoverable set-up
    // error terminates the application immediately.
    auto find_service_handler = [&proxy](score::mw::com::ServiceHandleContainer<HelloWorldProxy::HandleType> handles,
                                         score::mw::com::FindServiceHandle find_service_handle) noexcept {
        // The very first invocation of this handler is guaranteed to carry at least one matching handle, and we stop
        // the search right here, so this handler is only ever called once, with a non-empty set.
        score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle);

        const auto& handle = handles.front();
        std::cout << "Found HelloWorld service instance (instance id: " << DescribeInstanceId(handle)
                  << "). Creating proxy." << std::endl;

        auto proxy_result = HelloWorldProxy::Create(handle);
        if (!proxy_result)
        {
            std::cerr << "Failed to create proxy: " << proxy_result.error() << ". Terminating." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        // Store the proxy at its final, stable location *before* registering the receive handler, so that the
        // handler can safely refer to it.
        proxy.emplace(std::move(proxy_result).value());
        auto& event = proxy.value().message;

        // Register the event-receive handler *before* subscribing. The handler is of type
        // score::mw::com::EventReceiveHandler (a score::cpp::callback<void(void)>). It is invoked (on an internal
        // middleware thread) whenever a new sample of the "message" event has been received. Inside the handler, a
        // call to GetNewSamples() is guaranteed to provide at least one new sample.
        const auto set_receive_handler_result = event.SetReceiveHandler([&proxy]() noexcept {
            auto& receive_event = proxy.value().message;
            auto get_new_samples_result = receive_event.GetNewSamples(
                [](auto&& sample) {
                    const auto* buf = sample.Get()->data();
                    std::cout << "Sample received (event-driven): " << buf << std::endl;
                },
                kMaxSampleCount);
            if (!get_new_samples_result)
            {
                std::cerr << "Failed to get new samples: " << get_new_samples_result.error() << std::endl;
            }
        });
        if (!set_receive_handler_result)
        {
            std::cerr << "Failed to set receive handler: " << set_receive_handler_result.error() << ". Terminating."
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // Now subscribe. Since we react purely event-driven (via the receive handler), there is no need to check the
        // subscription state here: the receive handler will only ever be called once a new event sample has actually
        // been sent by the provider.
        const auto subscribe_result = event.Subscribe(kMaxSampleCount);
        if (!subscribe_result)
        {
            std::cerr << "Failed to subscribe to 'message' event: " << subscribe_result.error() << ". Terminating."
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::cout << "Subscribed to 'message' event. Waiting for event-driven sample reception ..." << std::endl;
    };

    auto find_service_handle_result =
        HelloWorldProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // There is nothing to poll here: all sample reception happens event-driven in the receive handler (on an internal
    // middleware thread). The main thread simply blocks until a termination signal requests the shut down.
    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    // Stop the asynchronous service discovery before shutting down. This is a no-op if the find-service handler already
    // stopped it (i.e. if the service instance had been found).
    score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle_result.value());
    std::cout << "HelloWorld service consumer going down." << std::endl;

    return EXIT_SUCCESS;
}
