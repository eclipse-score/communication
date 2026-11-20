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
 *
 ********************************************************************************/

#include "Multiplexer.h"

#include "score/io_dispatcher/io_dispatcher_factory.h"
#include "score/os/utils/thread.h"

#include <utils/log/logger.h>
#include <utils/time/TimerManager.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace lgsomeip
{
namespace osabstraction
{
namespace
{

// Covers the sockets of a daemon with many TCP connections.
// The Linux backend walks every slot on each wake, so a larger capacity slows every pass.
constexpr std::uint16_t kMaxEventSources{256U};

// PacketRouterProxy retries its connect from the timeout callback at this rate.
constexpr std::chrono::milliseconds kIdleWait{1000};

}  // namespace

struct Multiplexer::Backend
{
    std::unique_ptr<io_dispatcher::IoDispatcher> dispatcher;
    std::unordered_map<int, io_dispatcher::EventSourceHandle> handles;
    std::function<void()> timeout_callback;
    std::mutex mutex;
};

Multiplexer::Multiplexer() : backend_{std::make_unique<Backend>()}
{
    auto dispatcher = io_dispatcher::MakeIoDispatcher(kMaxEventSources);
    if (!dispatcher.has_value())
    {
        // Not LSAR_RUNTIME_ERROR: SomeipException.h includes message/MessageConstant.h, outside :exception_headers.
        throw std::runtime_error("Multiplexer::Multiplexer / io_dispatcher not created / " +
                                 std::string{dispatcher.error().Message()});
    }
    backend_->dispatcher = std::move(dispatcher).value();
}

Multiplexer::~Multiplexer()
{
    stop();
    join();
}

void Multiplexer::start()
{
    running_ = true;
    thread_ = std::make_shared<std::thread>(&Multiplexer::run, this);
    score::os::set_thread_name(*thread_, "SomeipIoDispMux");
}

void Multiplexer::stop()
{
    running_.store(false);
    backend_->dispatcher->Unblock();
}

void Multiplexer::join()
{
    if (thread_ != nullptr && thread_->joinable())
    {
        thread_->join();
    }
}

void Multiplexer::run()
{
    while (running_.load())
    {
        const auto result = backend_->dispatcher->HandleEvents(kIdleWait);

        if (!running_.load())
        {
            break;
        }

        // TODO: IODispatcher can easily support native timers for both QNX and Linux, so the TimerManager can be
        // removed from here.
#if defined(LINUX)
        // On QNX, MessagePassingServer drives the timers.
        TimerManager::get().iterate_loop();
#endif  // LINUX

        if (!result.has_value())
        {
            // A failed wait is not retried: a poll() error other than EINTR repeats on every call.
            LGSOMEIP_LOG_ERROR << "Multiplexer::run / HandleEvents failed / " << std::string{result.error().Message()};
            running_.store(false);
            break;
        }

        if (result.value() == io_dispatcher::UnblockReason::kTimeout)
        {
            std::function<void()> timeout_callback;
            {
                const std::lock_guard<std::mutex> lock{backend_->mutex};
                timeout_callback = backend_->timeout_callback;
            }
            if (timeout_callback != nullptr)
            {
                timeout_callback();
            }
        }
    }
}

void Multiplexer::set(int file_descriptor, std::function<void(bool)> callback)
{
    if (!callback)
    {
        LGSOMEIP_LOG_ERROR << "Multiplexer::set / empty callback for FD : " << file_descriptor;
        return;
    }

    // LG's readers detect end of stream on the read, so every readiness is reported as data.
    io_dispatcher::CallbackType dispatch{
        [callback = std::move(callback)](io_dispatcher::EventSourceHandle, io_dispatcher::EventTypes) {
            callback(false);
        }};

    const std::lock_guard<std::mutex> lock{backend_->mutex};
    const auto handle = backend_->dispatcher->Register(
        file_descriptor, io_dispatcher::EventTypes{}.SetReadEvent(true), std::move(dispatch));
    if (!handle.has_value())
    {
        LGSOMEIP_LOG_ERROR << "Multiplexer::set / Register failed for FD : " << file_descriptor << " / "
                           << std::string{handle.error().Message()};
        return;
    }

    // Assignment, not emplace: the QNX backend drops a closed fd on its own, so a reused fd number can register while
    // its stale handle is still stored here.
    backend_->handles[file_descriptor] = handle.value();
}

void Multiplexer::unset(int file_descriptor)
{
    io_dispatcher::EventSourceHandle handle{io_dispatcher::kInvalidEventSourceHandle};
    {
        const std::lock_guard<std::mutex> lock{backend_->mutex};
        const auto registered = backend_->handles.find(file_descriptor);
        if (registered == backend_->handles.end())
        {
            return;
        }

        handle = registered->second;
        backend_->handles.erase(registered);
    }

    // Outside the mutex: Unregister() waits for a running callback of this fd, and that callback may call set().
    backend_->dispatcher->Unregister(handle);
}

void Multiplexer::set_timeout(std::function<void()> callback)
{
    const std::lock_guard<std::mutex> lock{backend_->mutex};
    backend_->timeout_callback = std::move(callback);
}

}  // namespace osabstraction
}  // namespace lgsomeip
