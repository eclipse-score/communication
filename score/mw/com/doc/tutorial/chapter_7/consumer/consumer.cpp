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
#include "score/mw/com/doc/tutorial/chapter_7/tire_pressure_service.h"
#include "score/mw/com/types.h"

#include "score/json/json_writer.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <iostream>
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

using TirePressureProxy = score::mw::com::AsProxy<score::mw::com::tutorial::TirePressureInterface>;
// Deduce the concrete proxy-field type from one of the proxy's field members instead of naming the internal
// (impl-namespace) type directly - user applications must not reference score::mw::com::impl types. All four fields
// have the same type, so any of them can be used here.
using TirePressureField = decltype(TirePressureProxy::tire_pressure_front_left);

namespace
{

// Returns a binding-independent, human-readable description of the service instance a handle refers to.
//
// The only portable (public API) way to obtain a representation of the instance id from a handle is:
//   HandleType::GetInstanceId() -> ServiceInstanceId::Serialize() -> score::json::Object
// We then stringify that json object so it can be logged.
std::string DescribeInstanceId(const TirePressureProxy::HandleType& handle)
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

// Registers an event-receive handler on the given field and subscribes to it. A field is (from the proxy's point of
// view) just an event that additionally always carries a current value. Therefore the notifier part of its API
// (SetReceiveHandler / Subscribe / GetNewSamples) is exactly the same as for a plain event.
//
// The receive handler references the field via a pointer (8 bytes) and the field name via a string literal / const
// char* (8 bytes). This keeps the callback small enough to fit into the score::cpp::callback storage. On any
// unrecoverable set-up error the whole application terminates immediately.
void SetupField(TirePressureField& field, const char* const field_name)
{
    // Register the field-receive handler *before* subscribing. It is invoked (on an internal middleware thread)
    // whenever a new value of the field has been received. Inside the handler, a call to GetNewSamples() is guaranteed
    // to provide at least one new sample.
    const auto set_receive_handler_result = field.SetReceiveHandler([&field, field_name]() noexcept {
        auto get_new_samples_result = field.GetNewSamples(
            [field_name](auto&& sample) {
                std::cout << "Field '" << field_name << "' update received (event-driven): " << *sample.Get() << " bar"
                          << std::endl;
            },
            kMaxSampleCount);
        if (!get_new_samples_result)
        {
            std::cerr << "Failed to get new samples for field '" << field_name
                      << "': " << get_new_samples_result.error() << std::endl;
        }
    });
    if (!set_receive_handler_result)
    {
        std::cerr << "Failed to set receive handler for field '" << field_name
                  << "': " << set_receive_handler_result.error() << ". Terminating." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Now subscribe. Because a field always carries a current value, the receive handler is guaranteed to be called
    // once the subscription switches to kSubscribed - even if the provider does not update the field afterwards. This
    // is the essential difference to a plain event: the *initial value* semantics of a field cause the receive handler
    // to fire right after a successful subscription.
    const auto subscribe_result = field.Subscribe(kMaxSampleCount);
    if (!subscribe_result)
    {
        std::cerr << "Failed to subscribe to field '" << field_name << "': " << subscribe_result.error()
                  << ". Terminating." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::cout << "Subscribed to field '" << field_name << "'." << std::endl;
}

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // The consumer performs an explicit single-instance search: The InstanceSpecifier "MyTirePressureServiceInstance"
    // is mapped in the consumer configuration (mw_com_config.json) to the TirePressureService type with an explicit
    // instanceId (1). Therefore, the search matches exactly the one instance the provider offers.
    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyTirePressureServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // Stable storage for the proxy of the discovered service instance. It is populated by the find-service handler and,
    // from then on, only ever accessed by the (event-driven) field-receive handlers. It lives for the whole lifetime of
    // the application.
    std::optional<TirePressureProxy> proxy{};

    // This handler is invoked asynchronously by score::mw::com as soon as the searched service instance becomes
    // available. It creates the proxy, registers a field-receive handler for and subscribes to *each* of the four
    // fields. As we only need a single proxy, we stop the discovery right away from within this callback. Any
    // unrecoverable set-up error terminates the application immediately.
    auto find_service_handler = [&proxy](score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType> handles,
                                         score::mw::com::FindServiceHandle find_service_handle) noexcept {
        // The very first invocation of this handler is guaranteed to carry at least one matching handle, and we stop
        // the search right here, so this handler is only ever called once, with a non-empty set.
        score::cpp::ignore = TirePressureProxy::StopFindService(find_service_handle);

        const auto& handle = handles.front();
        std::cout << "Found TirePressure service instance (instance id: " << DescribeInstanceId(handle)
                  << "). Creating proxy." << std::endl;

        auto proxy_result = TirePressureProxy::Create(handle);
        if (!proxy_result)
        {
            std::cerr << "Failed to create proxy: " << proxy_result.error() << ". Terminating." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        // Store the proxy at its final, stable location *before* registering the receive handlers, so that the
        // handlers can safely refer to its fields.
        proxy.emplace(std::move(proxy_result).value());
        auto& tire_pressure_service = proxy.value();

        // Register a receive handler for and subscribe to each of the four fields.
        SetupField(tire_pressure_service.tire_pressure_front_left, "tire_pressure_front_left");
        SetupField(tire_pressure_service.tire_pressure_front_right, "tire_pressure_front_right");
        SetupField(tire_pressure_service.tire_pressure_rear_left, "tire_pressure_rear_left");
        SetupField(tire_pressure_service.tire_pressure_rear_right, "tire_pressure_rear_right");

        std::cout << "Waiting for event-driven field updates ..." << std::endl;
    };

    auto find_service_handle_result =
        TirePressureProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // There is nothing to poll here: all field-value reception happens event-driven in the receive handlers (on an
    // internal middleware thread). The main thread simply blocks until a termination signal requests the shut down.
    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    // Stop the asynchronous service discovery before shutting down. This is a no-op if the find-service handler already
    // stopped it (i.e. if the service instance had been found).
    score::cpp::ignore = TirePressureProxy::StopFindService(find_service_handle_result.value());
    std::cout << "TirePressure service consumer going down." << std::endl;

    return EXIT_SUCCESS;
}
