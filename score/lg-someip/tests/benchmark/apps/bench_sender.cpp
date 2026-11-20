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

// Sender of the SOME/IP send-path latency benchmark, on the lgsomeip API.
//
// Offers service 0x0103 / instance 0x0091 and notifies every event type and size, driven by the SOMEIP_BENCH_*
// environment. T_app (CLOCK_REALTIME) is stamped immediately before each notify() and written to a send-log CSV, which
// tools/wire_latency.py pairs against the egress frames of a capture on the same target to give T_wire - T_app.
// Event ids, wire lengths and the position of the send counter in each payload are those the analyzer pairs on.

#include <lgsomeip/LgsomeipApi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace lgsomeip::api;

constexpr service_t kService = 0x0103;
constexpr instance_t kInstance = 0x0091;
constexpr major_version_t kMajor = 0x02;
constexpr minor_version_t kMinor = 0x01;
constexpr eventgroup_t kEventgroup = 0x0005;

// Base event ids, as the send-log and the analyzer's event table carry them; the wire id has the 0x8000 bit set.
constexpr std::uint16_t kEvtUint8 = 0x65;
constexpr std::uint16_t kEvtBytes = 0x66;
constexpr std::uint16_t kEvtUint16 = 0x78;
constexpr std::uint16_t kEvtUint32 = 0x79;
constexpr std::uint16_t kEvtUint64 = 0x7a;
constexpr std::uint16_t kEvtInt8 = 0x7b;
constexpr std::uint16_t kEvtInt16 = 0x7c;
constexpr std::uint16_t kEvtInt32 = 0x7d;
constexpr std::uint16_t kEvtInt64 = 0x7e;
constexpr std::uint16_t kEvtFloat = 0x7f;
constexpr std::uint16_t kEvtDouble = 0x80;
constexpr std::uint16_t kEvtBool = 0x81;
constexpr std::uint16_t kEvtStruct = 0x82;
constexpr std::uint16_t kEvtFixedArray = 0x83;
constexpr std::uint16_t kEvtNestedStruct = 0x84;

constexpr std::uint16_t kAllEvents[] = {kEvtUint8,
                                        kEvtBytes,
                                        kEvtUint16,
                                        kEvtUint32,
                                        kEvtUint64,
                                        kEvtInt8,
                                        kEvtInt16,
                                        kEvtInt32,
                                        kEvtInt64,
                                        kEvtFloat,
                                        kEvtDouble,
                                        kEvtBool,
                                        kEvtStruct,
                                        kEvtFixedArray,
                                        kEvtNestedStruct};

constexpr std::uint32_t kIterations = 1000;
// Unlogged sends before the first measured one: the first send of a process costs about 1 ms more than any later one,
// which would otherwise be the first bucket's max. The analyzer ignores their frames, as no send-log row matches them.
constexpr std::uint32_t kWarmUpSends = 10;
constexpr std::size_t kFixedArrayWireLen = 32768;
constexpr std::size_t kNestedStructWireLen = 1024;  // 4 + 8 + 43 + 969
constexpr std::size_t kNestedPadOffset = 4 + 8 + 43;

std::uint32_t env_u32(const char* name, std::uint32_t fallback)
{
    const char* env = std::getenv(name);
    return env != nullptr ? static_cast<std::uint32_t>(std::strtoul(env, nullptr, 10)) : fallback;
}

// CLOCK_REALTIME, because libpcap stamps packets in that domain.
std::uint64_t now_realtime_ns()
{
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

// Writes the low `width` bytes of `value` big-endian at `out`.
void put_be(byte_t* out, std::uint64_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out[i] = static_cast<byte_t>(value >> (8 * (width - 1 - i)));
    }
}

Payload be(std::uint64_t value, std::size_t width)
{
    Payload payload(width);
    put_be(payload.data(), value, width);
    return payload;
}

std::uint32_t float_bits(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint64_t double_bits(double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct SendRecord
{
    std::uint16_t event_id;
    std::uint32_t size;  // SOME/IP wire payload length
    std::uint32_t counter;
    std::uint64_t t_app_ns;
};

std::shared_ptr<Application> app;
std::atomic<bool> registered{false};
std::vector<SendRecord> records;
std::uint32_t counter = 0;

// One measured bucket: repetitions x kIterations sends of one event, `fill` writing the counter into the payload.
void run_bucket(std::uint16_t event_id, Payload payload, const std::function<void(Payload&, std::uint32_t)>& fill)
{
    static const std::uint32_t repetitions = std::max(1U, env_u32("SOMEIP_BENCH_REPETITIONS", 5));
    static const std::chrono::microseconds gap{env_u32("SOMEIP_BENCH_SEND_GAP_US", 1000)};
    const auto wire_event = static_cast<event_t>(0x8000U | event_id);
    for (std::uint32_t i = 0; i < repetitions * kIterations; ++i)
    {
        const std::uint32_t ctr = counter++;
        fill(payload, ctr);
        const std::uint64_t t_app = now_realtime_ns();
        // force: consecutive values may repeat (Bool, Uint8 wrap), and an unforced notify of an unchanged value is
        // not sent.
        app->notify(kService, kInstance, wire_event, payload, true);
        records.push_back({event_id, static_cast<std::uint32_t>(payload.size()), ctr, t_app});
        std::this_thread::sleep_for(gap);
    }
}

void run_scalar(std::uint16_t event_id, std::size_t width, const std::function<std::uint64_t(std::uint32_t)>& value)
{
    run_bucket(event_id, Payload(width), [&](Payload& p, std::uint32_t ctr) {
        put_be(p.data(), value(ctr), width);
    });
}

void write_send_log()
{
    const char* env = std::getenv("SOMEIP_BENCH_SENDLOG");
    const std::string path = (env != nullptr && env[0] != '\0') ? env : "/var/log/someip_benchmark_sendlog.csv";
    std::FILE* fh = std::fopen(path.c_str(), "w");
    if (fh == nullptr)
    {
        std::cerr << "bench_sender: cannot open send-log " << path << std::endl;
        return;
    }
    std::fputs("event_id,size,counter,t_app_ns\n", fh);
    for (const auto& r : records)
    {
        std::fprintf(fh,
                     "%u,%u,%u,%llu\n",
                     static_cast<unsigned>(r.event_id),
                     static_cast<unsigned>(r.size),
                     static_cast<unsigned>(r.counter),
                     static_cast<unsigned long long>(r.t_app_ns));
    }
    std::fclose(fh);
    std::cout << "bench_sender: wrote " << records.size() << " send records to " << path << std::endl;
}

}  // namespace

int main(int argc, char** argv)
{
    app = Runtime::instance().create_application("bench_sender", argc > 1 ? argv[1] : "");
    if (!app->init())
    {
        std::cerr << "bench_sender: init failed" << std::endl;
        return 1;
    }
    app->register_application_state_handler([](bool is_registered) {
        if (is_registered && !registered.exchange(true))
        {
            for (const auto event : kAllEvents)
            {
                app->offer_event(kService, kInstance, static_cast<event_t>(0x8000U | event), {kEventgroup});
            }
            app->offer_service(kService, kInstance, kMajor, kMinor);
        }
    });
    app->start();
    while (!registered)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    // A grace timer, not a handshake: the orchestrator raises it so discovery and the subscription finish first.
    std::this_thread::sleep_for(std::chrono::milliseconds{env_u32("SOMEIP_BENCH_SUBSCRIBE_WAIT_MS", 3000)});

    records.reserve(static_cast<std::size_t>(kIterations) * env_u32("SOMEIP_BENCH_REPETITIONS", 5) * 20U);

    // QNX rounds a sleep up to the next clock tick, so on a 1 ms tick the default gap gives one send every 2 ms.
    const std::chrono::microseconds gap{env_u32("SOMEIP_BENCH_SEND_GAP_US", 1000)};
    for (std::uint32_t i = 0; i < kWarmUpSends; ++i)
    {
        app->notify(kService, kInstance, static_cast<event_t>(0x8000U | kEvtUint8), be(i, 1), true);
        std::this_thread::sleep_for(gap);
    }

    const auto identity = [](std::uint32_t ctr) -> std::uint64_t {
        return ctr;
    };
    run_scalar(kEvtUint8, 1, identity);
    run_scalar(kEvtUint16, 2, identity);
    run_scalar(kEvtUint32, 4, identity);
    run_scalar(kEvtUint64, 8, identity);
    run_scalar(kEvtInt8, 1, identity);
    run_scalar(kEvtInt16, 2, identity);
    run_scalar(kEvtInt32, 4, identity);
    run_scalar(kEvtInt64, 8, identity);
    run_scalar(kEvtFloat, 4, [](std::uint32_t ctr) -> std::uint64_t {
        return float_bits(static_cast<float>(ctr));
    });
    run_scalar(kEvtDouble, 8, [](std::uint32_t ctr) {
        return double_bits(static_cast<double>(ctr));
    });
    run_scalar(kEvtBool, 1, [](std::uint32_t ctr) -> std::uint64_t {
        return ctr & 1U;
    });

    // StructureSimple: float 1.0, then a byte carrying the counter.
    Payload simple = be(float_bits(1.0F), 4);
    simple.push_back(0);
    run_bucket(kEvtStruct, simple, [](Payload& p, std::uint32_t ctr) {
        p[4] = static_cast<byte_t>(ctr);
    });

    run_bucket(kEvtFixedArray, Payload(kFixedArrayWireLen), [](Payload& p, std::uint32_t ctr) {
        put_be(p.data(), ctr, 4);
    });

    // StructureNested: uint32 id, uint64 timestamp, the 43-byte inner struct (float 1.0, double 2.0 and bool true as
    // its last three members), then a 969-byte pad whose first four bytes carry the counter as well.
    Payload nested(kNestedStructWireLen);
    put_be(nested.data() + kNestedPadOffset - 13, float_bits(1.0F), 4);
    put_be(nested.data() + kNestedPadOffset - 9, double_bits(2.0), 8);
    nested[kNestedPadOffset - 1] = 1;
    run_bucket(kEvtNestedStruct, nested, [](Payload& p, std::uint32_t ctr) {
        put_be(p.data(), ctr, 4);
        put_be(p.data() + kNestedPadOffset, ctr, 4);
    });

    // Dynamic byte array: a 4-byte length field, then the data with the counter in its first four bytes.
    for (const std::uint32_t size : {64U, 256U, 1024U, 4096U, 16384U, 32768U})
    {
        Payload bytes(4 + size);
        put_be(bytes.data(), size, 4);
        run_bucket(kEvtBytes, bytes, [](Payload& p, std::uint32_t ctr) {
            put_be(p.data() + 4, ctr, 4);
        });
    }

    app->stop_offer_service(kService, kInstance);
    write_send_log();
    // The orchestrator waits for this marker, which prints after the send-log is on disk.
    std::cout << "BENCHMARK DONE" << std::endl;
    std::fflush(nullptr);
    std::_Exit(0);
}
