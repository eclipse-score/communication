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

// Client of the SOME/IP RPC benchmark, on the lgsomeip API: closed-loop Echo calls to a remote server.
//
// Each call is stamped with CLOCK_REALTIME around the request and its response and logged to a CSV, which
// tools/rpc_latency.py pairs with the request and response frames of a capture on the same target.
// A request is a 4-byte length field and the data, whose first four bytes carry the call counter big-endian: the key
// that pairing uses.

#include <lgsomeip/LgsomeipApi.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace lgsomeip::api;

constexpr service_t kService = 0x0104;
constexpr instance_t kInstance = 0x0001;
constexpr major_version_t kMajor = 0x01;
constexpr method_t kEcho = 0x0001;

// Data bytes per call; 1396 plus the 4-byte length field and the 16-byte header is the largest message that stays a
// single datagram without SOME/IP-TP.
constexpr std::array<std::uint32_t, 4> kSizes{8U, 64U, 512U, 1396U};
constexpr std::uint32_t kCallsPerSize = 1000U;
constexpr std::uint32_t kWarmupCalls = 50U;
constexpr std::chrono::microseconds kCallGap{1000};
constexpr std::chrono::seconds kDiscoveryTimeout{30};
constexpr std::chrono::seconds kCallTimeout{1};

struct CallRecord
{
    std::uint32_t size;
    std::uint32_t counter;
    std::uint64_t t_req_ns;
    std::uint64_t t_resp_ns;  // 0 when the call failed
};

std::shared_ptr<Application> app;
std::mutex mutex;
std::condition_variable cv;
bool available = false;
bool answered = false;
std::size_t answer_size = 0;

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "rpc_client: " << message << std::endl;
    std::_Exit(1);
}

std::uint64_t realtime_ns()
{
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

void put_be32(Payload& payload, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        payload[offset + i] = static_cast<byte_t>(value >> (8 * (3 - i)));
    }
}

// One Echo call of `size` data bytes; a timeout, an error or a reply of another size counts as a failure.
CallRecord call(Message& request, std::uint32_t size, std::uint32_t counter)
{
    put_be32(request.payload, 4, counter);
    std::unique_lock<std::mutex> lock{mutex};
    answered = false;
    lock.unlock();
    const std::uint64_t t_req = realtime_ns();
    app->send(request);
    lock.lock();
    const bool got = cv.wait_for(lock, kCallTimeout, [] {
        return answered;
    });
    const std::uint64_t t_resp = realtime_ns();
    const bool ok = got && (answer_size == request.payload.size());
    return {size, counter, t_req, ok ? t_resp : 0U};
}

void write_call_log(const std::vector<CallRecord>& records)
{
    const char* env = std::getenv("SOMEIP_BENCH_SENDLOG");
    const std::string path = (env != nullptr && env[0] != '\0') ? env : "/var/log/someip_rpc_calls.csv";
    std::FILE* fh = std::fopen(path.c_str(), "w");
    if (fh == nullptr)
    {
        fail("cannot open call log " + path);
    }
    // A short log would pass the analysis with fewer samples and no drops, so a failed write fails the run.
    bool ok = std::fputs("size,counter,t_req_ns,t_resp_ns\n", fh) >= 0;
    for (const auto& r : records)
    {
        ok = ok && (std::fprintf(fh,
                                 "%u,%u,%llu,%llu\n",
                                 static_cast<unsigned>(r.size),
                                 static_cast<unsigned>(r.counter),
                                 static_cast<unsigned long long>(r.t_req_ns),
                                 static_cast<unsigned long long>(r.t_resp_ns)) > 0);
    }
    ok = (std::fclose(fh) == 0) && ok;
    if (!ok)
    {
        fail("cannot write call log " + path);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    app = Runtime::instance().create_application("rpc_client", argc > 1 ? argv[1] : "");
    if (!app->init())
    {
        fail("init failed");
    }
    app->register_availability_handler(kService, kInstance, [](service_t, instance_t, bool is_available) {
        std::lock_guard<std::mutex> lock{mutex};
        available = is_available;
        cv.notify_all();
    });
    app->register_message_handler(kService, kInstance, kEcho, [](const std::shared_ptr<Message>& message) {
        std::lock_guard<std::mutex> lock{mutex};
        answer_size = (message->type == MessageType::Response) ? message->payload.size() : 0;
        answered = true;
        cv.notify_all();
    });
    app->register_application_state_handler([](bool is_registered) {
        if (is_registered)
        {
            app->request_service(kService, kInstance, kMajor);
        }
    });
    app->start();

    {
        std::unique_lock<std::mutex> lock{mutex};
        if (!cv.wait_for(lock, kDiscoveryTimeout, [] {
                return available;
            }))
        {
            fail("service not discovered within 30 s");
        }
    }

    Message request;
    request.service = kService;
    request.instance = kInstance;
    request.method = kEcho;
    request.interface_version = kMajor;

    std::uint32_t counter = 0U;
    const auto resize = [&request](std::uint32_t size) {
        request.payload.assign(4U + size, 0U);
        put_be32(request.payload, 0, size);
    };
    // The warm-up settles the first-call costs (the server's endpoint lookup, ARP, page faults) out of the samples.
    resize(kSizes.front());
    for (std::uint32_t i = 0U; i < kWarmupCalls; ++i)
    {
        static_cast<void>(call(request, kSizes.front(), counter++));
        std::this_thread::sleep_for(kCallGap);
    }

    std::vector<CallRecord> records{};
    records.reserve(kSizes.size() * kCallsPerSize);
    std::uint32_t failed = 0U;
    for (const auto size : kSizes)
    {
        resize(size);
        for (std::uint32_t i = 0U; i < kCallsPerSize; ++i)
        {
            records.push_back(call(request, size, counter++));
            failed += (records.back().t_resp_ns == 0U) ? 1U : 0U;
            std::this_thread::sleep_for(kCallGap);
        }
    }

    write_call_log(records);
    std::cout << "rpc_client: " << records.size() << " calls, " << failed << " failed" << std::endl;
    // The orchestrator waits for this marker, and it prints after the call log is on disk.
    std::cout << "BENCHMARK DONE" << std::endl;
    std::fflush(nullptr);
    std::_Exit(0);
}
