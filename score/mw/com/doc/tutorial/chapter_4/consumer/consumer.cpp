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
#include "score/mw/com/doc/tutorial/chapter_4/hello_world_service.h"
#include "score/mw/com/types.h"

#include "score/json/json_writer.h"

#include <atomic>
#include <csignal>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service consumer caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;

namespace
{

// Returns a human-readable name for a subscription state (for logging).
const char* ToString(const score::mw::com::SubscriptionState state) noexcept
{
    switch (state)
    {
        case score::mw::com::SubscriptionState::kSubscribed:
            return "kSubscribed";
        case score::mw::com::SubscriptionState::kSubscriptionPending:
            return "kSubscriptionPending";
        case score::mw::com::SubscriptionState::kNotSubscribed:
            return "kNotSubscribed";
        default:
            return "<unknown>";
    }
}

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

// Reads all currently available samples of the "message" event and logs them with the given tag.
void ReceiveSamples(HelloWorldProxy& proxy, const char* const tag)
{
    auto get_new_samples_result = proxy.message.GetNewSamples(
        [tag](auto&& sample) {
            const auto* buf = sample.Get()->data();
            std::cout << "[" << tag << "] Sample received: " << buf << std::endl;
        },
        1);
    if (!get_new_samples_result)
    {
        std::cerr << "[" << tag << "] Failed to get new samples: " << get_new_samples_result.error() << std::endl;
    }
}

// Bundles all state shared between the (asynchronously called) find-service handler and the main polling loop. We
// group it into a single struct so that the find-service handler lambda only has to capture a single pointer (the
// FindServiceHandler callback has a limited capture capacity).
struct SharedState
{
    std::mutex mutex{};
    bool proxies_created{false};

    // Set to true if the find-service handler hit an unrecoverable error while creating/subscribing the proxies. Such
    // errors are very unlikely to recover in a later handler invocation, so we signal them to the main loop, which then
    // terminates the application instead of looping forever without any proxies.
    std::atomic<bool> fatal_error{false};

    // Guards the service discovery so that it is stopped exactly once (either from within the find-service handler once
    // the instance was found, or on shutdown if it was never found).
    std::atomic<bool> search_stopped{false};

    // proxy_1 drives GetNewSamples() purely based on a one-time poll of the subscription state directly after
    // Subscribe(). Once it started receiving, it keeps polling GetNewSamples() unconditionally - even when the
    // subscription state later becomes kSubscriptionPending (during the provider's stop-offer phases).
    std::optional<HelloWorldProxy> proxy_poll{};
    std::atomic<bool> proxy_poll_receiving{false};

    // proxy_2 drives GetNewSamples() based on a registered subscription-state-change handler. It only calls
    // GetNewSamples() while the last observed subscription state is kSubscribed. When the state switches to
    // kSubscriptionPending it stops calling GetNewSamples().
    std::optional<HelloWorldProxy> proxy_handler{};
    std::atomic<score::mw::com::SubscriptionState> proxy_handler_state{
        score::mw::com::SubscriptionState::kNotSubscribed};
};

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // In this chapter the consumer performs an *explicit* instance search: The InstanceSpecifier
    // "MyHelloWorldServiceInstance" is mapped in the consumer configuration (mw_com_config.json) to the
    // HelloWorldService type *with* an explicit instanceId (1). Therefore, the search matches exactly that one
    // instance the provider offers.
    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    SharedState state{};

    // This handler is invoked asynchronously by score::mw::com whenever the set of matching service instances changes.
    // As soon as we are called with (at least) one matching handle, we immediately stop the search again (this shows
    // that a service-instance search may also be stopped from within a StartFindService callback) and create TWO
    // proxies for the SAME found service instance.
    //
    // Creating two proxies in the same application for the very same (provided) service instance is a rather
    // pathological use case, but score::mw::com supports it. We use it here only to demonstrate the two different ways
    // of dealing with the subscription state (polling vs. state-change handler) within a single sample application,
    // instead of having to write two separate consumer applications.
    auto find_service_handler = [&state](score::mw::com::ServiceHandleContainer<HelloWorldProxy::HandleType> handles,
                                         score::mw::com::FindServiceHandle find_service_handle) noexcept {
        std::lock_guard<std::mutex> lock{state.mutex};

        // The very first invocation of this handler is guaranteed to carry at least one matching handle (an empty
        // set is only ever reported later on, when a previously found instance vanishes). Since we do an explicit
        // single-instance search and stop the discovery right below, this handler is only ever called once, with a
        // non-empty set - so no empty-check is necessary here.

        // We found the matching instance. As we only act on it once, we can stop the search right here, from within
        // the callback (stopping a search from within a StartFindService callback is explicitly supported).
        if (!state.search_stopped.exchange(true))
        {
            score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle);
        }

        const auto& handle = handles.front();
        const auto instance_id_description = DescribeInstanceId(handle);
        std::cout << "Found HelloWorld service instance (instance id: " << instance_id_description
                  << "). Creating two proxies for it." << std::endl;

        // --- proxy_1: poll the subscription state once ---------------------------------------------------------
        auto proxy_1_result = HelloWorldProxy::Create(handle);
        if (!proxy_1_result)
        {
            std::cerr << "Failed to create proxy_1: " << proxy_1_result.error() << std::endl;
            state.fatal_error.store(true, std::memory_order_relaxed);
            return;
        }
        auto proxy_1 = std::move(proxy_1_result).value();

        const auto subscribe_1_result = proxy_1.message.Subscribe(1);
        if (!subscribe_1_result)
        {
            std::cerr << "proxy_1: Failed to subscribe to 'message' event: " << subscribe_1_result.error() << std::endl;
            state.fatal_error.store(true, std::memory_order_relaxed);
            return;
        }

        // Poll the current subscription state exactly once. Only if the subscribe succeeded AND we are already in
        // state kSubscribed do we enable cyclic reception on proxy_1.
        const auto polled_state = proxy_1.message.GetSubscriptionState();
        std::cout << "[proxy_1 poll] Subscription state directly after Subscribe(): " << ToString(polled_state)
                  << std::endl;
        if (polled_state == score::mw::com::SubscriptionState::kSubscribed)
        {
            state.proxy_poll_receiving.store(true, std::memory_order_relaxed);
            std::cout << "[proxy_1 poll] State is kSubscribed -> will start receiving samples cyclically." << std::endl;
        }
        else
        {
            std::cout << "[proxy_1 poll] State is not kSubscribed -> proxy_1 will NOT receive samples." << std::endl;
        }

        // --- proxy_2: register a subscription-state-change handler ---------------------------------------------
        auto proxy_2_result = HelloWorldProxy::Create(handle);
        if (!proxy_2_result)
        {
            std::cerr << "Failed to create proxy_2: " << proxy_2_result.error() << std::endl;
            state.fatal_error.store(true, std::memory_order_relaxed);
            return;
        }
        auto proxy_2 = std::move(proxy_2_result).value();

        // Register the state-change handler *before* subscribing so that we observe all state transitions. The
        // handler is called on an internal middleware thread. It must be short and must not call any method on the
        // same event instance - so we merely record the new state in an atomic and return true to keep the handler
        // registered.
        auto* const handler_state = &state.proxy_handler_state;
        const auto set_handler_result = proxy_2.message.SetSubscriptionStateChangeHandler(
            [handler_state](const score::mw::com::SubscriptionState new_state) noexcept -> bool {
                std::cout << "[proxy_2 handler] Subscription state changed to: " << ToString(new_state) << std::endl;
                handler_state->store(new_state, std::memory_order_relaxed);
                return true;
            });
        if (!set_handler_result)
        {
            std::cerr << "proxy_2: Failed to set subscription state change handler: " << set_handler_result.error()
                      << std::endl;
            state.fatal_error.store(true, std::memory_order_relaxed);
            return;
        }

        const auto subscribe_2_result = proxy_2.message.Subscribe(1);
        if (!subscribe_2_result)
        {
            std::cerr << "proxy_2: Failed to subscribe to 'message' event: " << subscribe_2_result.error() << std::endl;
            state.fatal_error.store(true, std::memory_order_relaxed);
            return;
        }

        state.proxy_poll.emplace(std::move(proxy_1));
        state.proxy_handler.emplace(std::move(proxy_2));
        state.proxies_created = true;
    };

    auto find_service_handle_result =
        HelloWorldProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        exit(1);
    }

    // Main loop: drive reception of the two proxies according to their respective strategy.
    while (g_running.load(std::memory_order_relaxed))
    {
        // If the find-service handler hit an unrecoverable error while setting up the proxies, terminate: it is very
        // unlikely that a later invocation would succeed, so there is no point in looping forever.
        if (state.fatal_error.load(std::memory_order_relaxed))
        {
            std::cerr << "Unrecoverable error while setting up the proxies. Shutting down." << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::lock_guard<std::mutex> lock{state.mutex};
        if (!state.proxies_created)
        {
            continue;
        }

        // proxy_1: keep calling GetNewSamples() unconditionally once reception was enabled by the initial
        // subscription-state poll. During the provider's stop-offer phases the subscription state is
        // kSubscriptionPending; GetNewSamples() is still a valid call, it just does not deliver new data during those
        // phases.
        if (state.proxy_poll_receiving.load(std::memory_order_relaxed))
        {
            ReceiveSamples(state.proxy_poll.value(), "proxy_1 poll");
        }

        // proxy_2: only call GetNewSamples() while the last state-change notification told us we are kSubscribed. When
        // the provider stops offering, the handler reports kSubscriptionPending and proxy_2 stops calling
        // GetNewSamples() until the state returns to kSubscribed.
        if (state.proxy_handler_state.load(std::memory_order_relaxed) == score::mw::com::SubscriptionState::kSubscribed)
        {
            ReceiveSamples(state.proxy_handler.value(), "proxy_2 handler");
        }
        else
        {
            std::cout << "[proxy_2 handler] State is not kSubscribed -> skipping GetNewSamples()." << std::endl;
        }
    }

    // Stop the asynchronous service discovery before shutting down - unless it was already stopped from within the
    // find-service handler (once the instance was found).
    if (!state.search_stopped.exchange(true))
    {
        score::cpp::ignore = HelloWorldProxy::StopFindService(find_service_handle_result.value());
    }
    std::cout << "HelloWorld service consumer going down." << std::endl;

    return state.fatal_error.load(std::memory_order_relaxed) ? 1 : 0;
}
