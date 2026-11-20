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

// Enhanced Testability Service (ETS) test application for the test_cases/test_ets_*.py suites, on the lgsomeip API.
//
// It follows the ETS definition of the OPEN Alliance Automotive Ethernet ECU Test Specification Layer 3-7 v3.0,
// section 5.1.4:
//   service 0x0103 / instance 0x0091 / version 2.1, UDP 34501, TCP 34502
//   events  TestEventUINT8 0x8001, TestEventUINT8Array 0x8002, TestEventUINT8E2E 0x8004 (eventgroup 5),
//           TestEventUINT8Multicast 0x800B (eventgroup 6)
//   fields  InterfaceVersion 0x8005, TestFieldUINT8 0x8006, TestFieldUINT8Array 0x8007 (eventgroup 5)
//   client  mode against the ETS instance 0x00F4 the tester offers (clientService* methods)
// Method IDs follow the suites' someip_itf/ets/constants.py. Where the specification's Table 1 gives a method a
// different ID, the method answers on that ID as well (kSpecAliases).
//
// The lgsomeip API carries raw payload bytes, so each handler parses its own arguments; a request too short for its
// signature is answered with E_MALFORMED_MESSAGE.

#include <lgsomeip/LgsomeipApi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <vector>

namespace
{

using namespace lgsomeip::api;
using namespace std::chrono_literals;

constexpr service_t kService = 0x0103;
constexpr instance_t kInstance = 0x0091;
constexpr instance_t kRemoteInstance = 0x00F4;  // the ETS instance the tester offers, for client mode
constexpr major_version_t kMajor = 2;
constexpr minor_version_t kMinor = 1;

constexpr eventgroup_t kEventgroup = 5;
constexpr eventgroup_t kMulticastEventgroup = 6;
constexpr event_t kEventUint8 = 0x8001;
constexpr event_t kEventUint8Array = 0x8002;
constexpr event_t kEventUint8E2E = 0x8004;
constexpr event_t kFieldInterfaceVersion = 0x8005;
constexpr event_t kFieldUint8 = 0x8006;
constexpr event_t kFieldUint8Array = 0x8007;
constexpr event_t kEventUint8Multicast = 0x800B;

constexpr method_t kEchoUint8Array = 9;

constexpr byte_t kFieldUint8Init = 0x55;
const Payload kFieldUint8ArrayInit{0, 0, 0, 3, 0x0A, 0x0B, 0x0C};  // uint32 length, then the elements
const Payload kInterfaceVersion{1, 0, 0, 0, 0};                    // uint8 major, uint32 minor

std::shared_ptr<Application> app;
std::atomic<bool> registered{false};
std::atomic<bool> stop_requested{false};

std::mutex state_mutex;  // guards the fields and the client-mode state below
byte_t field_uint8 = kFieldUint8Init;
Payload field_uint8_array = kFieldUint8ArrayInit;
byte_t last_unicast_value = 0;
byte_t last_multicast_value = 0;
bool client_active = false;
bool remote_available = false;  // the daemon reported the tester's ETS available, so it holds a route to it
std::condition_variable client_cv;
std::optional<Payload> client_echo_response;

// A handler returns the response payload, or nullopt for a request its signature cannot parse.
using Handler = std::function<std::optional<Payload>(const Payload&)>;

std::uint32_t be32(const Payload& p, std::size_t at)
{
    return (std::uint32_t{p[at]} << 24) | (std::uint32_t{p[at + 1]} << 16) | (std::uint32_t{p[at + 2]} << 8) |
           p[at + 3];
}

void put32(Payload& p, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        p.push_back(static_cast<byte_t>(value >> shift));
    }
}

// Echoes the first `size` bytes: every fixed-size argument list, whatever its types.
Handler echo_fixed(std::size_t size)
{
    return [size](const Payload& in) -> std::optional<Payload> {
        if (in.size() < size)
        {
            return std::nullopt;
        }
        return Payload(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(size));
    };
}

// Echoes a uint32 length field and the bytes it declares: dynamic arrays, strings and unions.
std::optional<Payload> echo_dynamic(const Payload& in)
{
    if (in.size() < 4 || be32(in, 0) > in.size() - 4)
    {
        return std::nullopt;
    }
    return Payload(in.begin(), in.begin() + 4 + static_cast<std::ptrdiff_t>(be32(in, 0)));
}

template <typename T>
T reverse_bits(T value)
{
    T result = 0;
    for (std::size_t i = 0; i < sizeof(T) * 8; ++i)
    {
        result = static_cast<T>(result | (((value >> i) & T{1}) << (sizeof(T) * 8 - 1 - i)));
    }
    return result;
}

void notify_fields()
{
    std::lock_guard<std::mutex> lock{state_mutex};
    app->notify(kService, kInstance, kFieldInterfaceVersion, kInterfaceVersion, true);
    app->notify(kService, kInstance, kFieldUint8, {field_uint8}, true);
    app->notify(kService, kInstance, kFieldUint8Array, field_uint8_array, true);
}

void offer()
{
    app->offer_service(kService, kInstance, kMajor, kMinor);
}

// Runs `work` on its own thread; the triggers and the client-mode methods are fire-and-forget and must not block
// the dispatch thread they arrive on.
// ponytail: detached threads, joined by nothing; move to a worker queue if shutdown ordering ever matters.
void run_async(std::function<void()> work)
{
    std::thread(std::move(work)).detach();
}

// triggerEvent*: one sample at once, then one every debounce*100 ms for duration/debounce more samples.
void run_trigger(event_t event, bool as_array, byte_t duration, byte_t debounce)
{
    const unsigned samples = debounce == 0 ? 1U : static_cast<unsigned>(duration) / debounce + 1U;
    Payload array;
    for (unsigned i = 0; i < samples && !stop_requested; ++i)
    {
        if (i > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(debounce * 100U));
        }
        array.push_back(static_cast<byte_t>(i));
        Payload payload;
        if (as_array)
        {
            put32(payload, static_cast<std::uint32_t>(array.size()));
            payload.insert(payload.end(), array.begin(), array.end());
        }
        else
        {
            payload = {static_cast<byte_t>(i)};
        }
        app->notify(kService, kInstance, event, payload, true);
    }
}

Handler trigger(event_t event, bool as_array = false)
{
    return [event, as_array](const Payload& in) -> std::optional<Payload> {
        if (in.size() < 3)
        {
            return std::nullopt;
        }
        const byte_t duration = in[1];
        const byte_t debounce = in[2];
        run_async([=]() {
            run_trigger(event, as_array, duration, debounce);
        });
        return Payload{};
    };
}

void on_remote_message(const std::shared_ptr<Message>& message)
{
    std::lock_guard<std::mutex> lock{state_mutex};
    if (message->type == MessageType::Notification && !message->payload.empty())
    {
        (message->method == kEventUint8 ? last_unicast_value : last_multicast_value) = message->payload[0];
    }
    else if (message->type == MessageType::Response || message->type == MessageType::Error)
    {
        client_echo_response = message->payload;
        client_cv.notify_all();
    }
}

void client_unsubscribe()
{
    app->unsubscribe(kService, kRemoteInstance, kEventgroup);
    app->unsubscribe(kService, kRemoteInstance, kMulticastEventgroup);
}

// clientServiceCallEchoUINT8Array: forwards the array to the remote ETS's echoUINT8Array and returns its answer.
std::optional<Payload> client_echo(const Payload& in)
{
    std::unique_lock<std::mutex> lock{state_mutex};
    // A request sent before the daemon holds a route to the remote instance is dropped without an answer.
    client_cv.wait_for(lock, 5s, []() {
        return remote_available;
    });
    client_echo_response.reset();
    Message request;
    request.service = kService;
    request.instance = kRemoteInstance;
    request.method = kEchoUint8Array;
    request.interface_version = kMajor;
    request.payload = in;
    lock.unlock();
    app->send(request);
    lock.lock();
    client_cv.wait_for(lock, 10s, []() {
        return client_echo_response.has_value();
    });
    return client_echo_response.value_or(Payload{0, 0, 0, 0});
}

const std::map<method_t, Handler> kMethods{
    {1,  // resetInterface
     [](const Payload&) -> std::optional<Payload> {
         {
             std::lock_guard<std::mutex> lock{state_mutex};
             field_uint8 = kFieldUint8Init;
             field_uint8_array = kFieldUint8ArrayInit;
             last_unicast_value = 0;
             last_multicast_value = 0;
         }
         notify_fields();
         return Payload{};
     }},
    {2,  // suspendInterface(start, duration): stop offering after `start` s, offer again `duration` s later
     [](const Payload& in) -> std::optional<Payload> {
         if (in.size() < 2)
         {
             return std::nullopt;
         }
         const byte_t start = in[0];
         const byte_t duration = in[1];
         run_async([=]() {
             std::this_thread::sleep_for(std::chrono::seconds(start));
             app->stop_offer_service(kService, kInstance);
             std::this_thread::sleep_for(std::chrono::seconds(duration));
             if (!stop_requested)
             {
                 offer();
             }
         });
         return Payload{};
     }},
    {3, trigger(kEventUint8)},
    {4, trigger(kEventUint8Array, true)},
    {58, trigger(kEventUint8Multicast)},
    {8, echo_fixed(1)},   // echoUINT8
    {9, echo_dynamic},    // echoUINT8Array
    {14, echo_fixed(1)},  // echoINT8
    {23, echo_fixed(1)},  // echoENUM
    {26, echo_fixed(1)},  // echoTYPEDEF
    {31,                  // checkByteOrder(uint8, uint16) -> uint32 sum
     [](const Payload& in) -> std::optional<Payload> {
         if (in.size() < 3)
         {
             return std::nullopt;
         }
         Payload out;
         put32(out, std::uint32_t{in[0]} + ((std::uint32_t{in[1]} << 8) | in[2]));
         return out;
     }},
    {35,  // echoCommonDatatypes(bool, u8, u16, u32, i8, i16, i32, float) -> the same values in reverse order
     [](const Payload& in) -> std::optional<Payload> {
         if (in.size() < 19)
         {
             return std::nullopt;
         }
         Payload out;
         for (const auto& [offset, size] : {std::pair{15, 4}, {11, 4}, {9, 2}, {8, 1}, {4, 4}, {2, 2}, {1, 1}, {0, 1}})
         {
             out.insert(out.end(), in.begin() + offset, in.begin() + offset + size);
         }
         return out;
     }},
    {37,
     [](const Payload&) -> std::optional<Payload> {
         return kInterfaceVersion;
     }},
    {38,  // TestFieldUINT8 getter
     [](const Payload&) -> std::optional<Payload> {
         std::lock_guard<std::mutex> lock{state_mutex};
         return Payload{field_uint8};
     }},
    {39,  // TestFieldUINT8 setter
     [](const Payload& in) -> std::optional<Payload> {
         if (in.empty())
         {
             return std::nullopt;
         }
         {
             std::lock_guard<std::mutex> lock{state_mutex};
             field_uint8 = in[0];
         }
         app->notify(kService, kInstance, kFieldUint8, {in[0]}, true);
         return Payload{in[0]};
     }},
    {40,  // TestFieldUINT8Array getter
     [](const Payload&) -> std::optional<Payload> {
         std::lock_guard<std::mutex> lock{state_mutex};
         return field_uint8_array;
     }},
    {41,  // TestFieldUINT8Array setter
     [](const Payload& in) -> std::optional<Payload> {
         auto value = echo_dynamic(in);
         if (!value)
         {
             return std::nullopt;
         }
         {
             std::lock_guard<std::mutex> lock{state_mutex};
             field_uint8_array = *value;
         }
         app->notify(kService, kInstance, kFieldUint8Array, *value, true);
         return value;
     }},
    {47,  // clientServiceActivate(start)
     [](const Payload& in) -> std::optional<Payload> {
         const byte_t start = in.empty() ? 0 : in[0];
         run_async([=]() {
             std::this_thread::sleep_for(std::chrono::seconds(start));
             app->request_service(kService, kRemoteInstance, kMajor);
             std::lock_guard<std::mutex> lock{state_mutex};
             client_active = true;
         });
         return Payload{};
     }},
    {48,  // clientServiceDeactivate(stop)
     [](const Payload& in) -> std::optional<Payload> {
         const byte_t stop = in.empty() ? 0 : in[0];
         run_async([=]() {
             std::this_thread::sleep_for(std::chrono::seconds(stop));
             client_unsubscribe();
             app->release_service(kService, kRemoteInstance);
             std::lock_guard<std::mutex> lock{state_mutex};
             client_active = false;
             last_unicast_value = 0;
             last_multicast_value = 0;
         });
         return Payload{};
     }},
    {49, client_echo},
    {50,  // clientServiceSubscribeEventgroup(start, duration)
     [](const Payload& in) -> std::optional<Payload> {
         if (in.size() < 2)
         {
             return std::nullopt;
         }
         const byte_t start = in[0];
         const byte_t duration = in[1];
         run_async([=]() {
             std::this_thread::sleep_for(std::chrono::seconds(start));
             {
                 std::lock_guard<std::mutex> lock{state_mutex};
                 if (!client_active)
                 {
                     return;
                 }
             }
             app->request_event(kService, kRemoteInstance, kEventUint8, {kEventgroup});
             app->request_event(kService, kRemoteInstance, kEventUint8Multicast, {kMulticastEventgroup});
             app->subscribe(kService, kRemoteInstance, kEventgroup, kMajor);
             app->subscribe(kService, kRemoteInstance, kMulticastEventgroup, kMajor);
             std::this_thread::sleep_for(std::chrono::seconds(duration));
             client_unsubscribe();
         });
         return Payload{};
     }},
    {53, echo_dynamic},   // echoUINT8Array2Dim
    {54, echo_fixed(5)},  // echoStaticUINT8Array
    {55, echo_dynamic},   // echoUINT8ArrayMinSize
    {60,                  // clientServiceGetLastValueOfEventUDPUnicast
     [](const Payload&) -> std::optional<Payload> {
         std::lock_guard<std::mutex> lock{state_mutex};
         return Payload{last_unicast_value};
     }},
    {61,  // clientServiceGetLastValueOfEventUDPMulticast
     [](const Payload&) -> std::optional<Payload> {
         std::lock_guard<std::mutex> lock{state_mutex};
         return Payload{last_multicast_value};
     }},
    {62, echo_dynamic},    // echoUINT8Array8BitLength (deployed with a 4-byte length field)
    {63, echo_dynamic},    // echoUINT8Array16BitLength (deployed with a 4-byte length field)
    {64, echo_fixed(10)},  // echoSTRUCTSimple: array[2] of {float, uint8}
    {65,                   // echoBitfields(u8, u16, u32) -> each with its bits reversed
     [](const Payload& in) -> std::optional<Payload> {
         if (in.size() < 7)
         {
             return std::nullopt;
         }
         const auto b16 = reverse_bits(static_cast<std::uint16_t>((in[1] << 8) | in[2]));
         Payload out{reverse_bits(in[0]), static_cast<byte_t>(b16 >> 8), static_cast<byte_t>(b16 & 0xFF)};
         put32(out, reverse_bits(be32(in, 3)));
         return out;
     }},
    {66, echo_fixed(8)},   // echoFLOAT64
    {67, echo_dynamic},    // echoUNION
    {68, echo_dynamic},    // echoUTF16DYNAMIC
    {69, echo_fixed(64)},  // echoUTF16FIXED
    {70, echo_dynamic},    // echoUTF8DYNAMIC
    {71, echo_fixed(64)},  // echoUTF8FIXED
    {72, echo_fixed(1)},   // echoUINT8RELIABLE
    {73, echo_fixed(17)},  // echoUINT8E2E(len, counter, dataID, crc: uint32; value: uint8)
};

// OPEN Alliance TC8 v3.0 Table 1 method ID -> the ID above that implements it, where the two differ.
const std::map<method_t, method_t> kSpecAliases{
    {0x0A, 72},
    {0x0B, 73},
    {0x12, 66},
    {0x13, 71},
    {0x14, 69},
    {0x15, 70},
    {0x16, 68},
    {0x19, 67},
};

// The methods deployed fire-and-forget: resetInterface, suspendInterface, the triggers and the client-mode controls.
const std::set<method_t> kFireAndForget{1, 2, 3, 4, 47, 48, 50, 58};

void on_request(const Handler& handler, const std::shared_ptr<Message>& request)
{
    if (request->type != MessageType::Request && request->type != MessageType::RequestNoReturn)
    {
        return;
    }
    if (request->type == MessageType::Request && kFireAndForget.count(request->method) != 0)
    {
        return;  // AS__SWS_CM_10302 discards it, and PRS_SOMEIP_00537 forbids an ERROR reply
    }
    const auto result = handler(request->payload);
    if (request->type != MessageType::Request)
    {
        return;  // fire-and-forget: never answered, not even with an error
    }
    Message response = *request;
    response.type = result ? MessageType::Response : MessageType::Error;
    response.return_code = result ? ReturnCode::Ok : ReturnCode::MalformedMessage;
    response.payload = result.value_or(Payload{});
    app->send(response);
}

void on_state(bool is_registered)
{
    if (is_registered && !registered.exchange(true))
    {
        for (const event_t event : {kEventUint8, kEventUint8Array, kEventUint8E2E})
        {
            app->offer_event(kService, kInstance, event, {kEventgroup});
        }
        for (const event_t field : {kFieldInterfaceVersion, kFieldUint8, kFieldUint8Array})
        {
            app->offer_event(kService, kInstance, field, {kEventgroup}, true);
        }
        app->offer_event(kService, kInstance, kEventUint8Multicast, {kMulticastEventgroup});
        offer();
    }
    registered = is_registered;
}

}  // namespace

int main(int argc, char** argv)
{
    std::signal(SIGTERM, [](int) {
        stop_requested = true;
    });
    std::signal(SIGINT, [](int) {
        stop_requested = true;
    });

    app = Runtime::instance().create_application("ets", argc > 1 ? argv[1] : "");
    if (!app->init())
    {
        std::cerr << "ets_app: init failed" << std::endl;
        return 1;
    }

    for (const auto& [method, handler] : kMethods)
    {
        app->register_message_handler(kService, kInstance, method, [&handler = handler](const auto& request) {
            on_request(handler, request);
        });
    }
    for (const auto& [spec_id, method] : kSpecAliases)
    {
        app->register_message_handler(
            kService, kInstance, spec_id, [&handler = kMethods.at(method)](const auto& request) {
                on_request(handler, request);
            });
    }
    for (const method_t remote :
         {kEchoUint8Array, static_cast<method_t>(kEventUint8), static_cast<method_t>(kEventUint8Multicast)})
    {
        app->register_message_handler(kService, kRemoteInstance, remote, on_remote_message);
    }
    app->register_availability_handler(kService, kRemoteInstance, [](service_t, instance_t, bool available) {
        std::lock_guard<std::mutex> lock{state_mutex};
        remote_available = available;
        client_cv.notify_all();
    });
    app->register_application_state_handler(on_state);
    app->start();

    // Seeds the fields once the daemon has processed the offer, then publishes TestEventUINT8E2E at 1 Hz.
    // ponytail: the E2E event carries a bare counter, not PROFILE_04 protection; lg-someip ships Profile 01 and a
    // custom profile only.
    byte_t counter = 0;
    bool fields_seeded = false;
    while (!stop_requested)
    {
        std::this_thread::sleep_for(1s);
        if (!registered)
        {
            continue;
        }
        if (!fields_seeded)
        {
            notify_fields();
            fields_seeded = true;
        }
        app->notify(kService, kInstance, kEventUint8E2E, {counter++}, true);
    }

    app->stop_offer_service(kService, kInstance);
    app->stop();
    return 0;
}
