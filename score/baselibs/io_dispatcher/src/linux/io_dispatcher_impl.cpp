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

/********************************************************************************
 * Linux backend of the io_dispatcher (poll + self-pipe).
 ********************************************************************************/

#include "score/io_dispatcher/linux/io_dispatcher_impl.h"
#include "score/io_dispatcher/io_dispatcher_error.h"
#include "score/io_dispatcher/io_dispatcher_factory.h"
#include "score/mw/log/logging.h"
#include "score/os/errno.h"
#include "score/os/fcntl.h"
#include "score/os/sys_poll.h"
#include "score/os/unistd.h"
#include <measurement/perf.hpp>
#include <poll.h>
#include <array>
#include <climits>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace io_dispatcher
{
namespace
{

/**
 * @brief Maximum number of callbacks.
 * One value is reserved for the unblock / invalid handle.
 */
constexpr std::uint64_t kMaxNumCallbacks{8191U};

/**
 * @brief Bit offset of the entry index within an EventSourceHandle.
 */
constexpr std::uint32_t kEventSourceHandleIndexOffset{51U};

/**
 * @brief Mask selecting the entry-index bits (upper 13 bits).
 */
constexpr std::uint64_t kEventSourceHandleIndexMask{0xFFF8000000000000ULL};

/**
 * @brief Mask selecting the sequence-number bits (lower 48 bits); bits 48..50 carry the DispatcherTag.
 */
constexpr std::uint64_t kEventSourceHandleSeqNumMask{0x0000FFFFFFFFFFFFULL};

/**
 * @brief Reserved handle used to report an unblock event (all bits set == invalid).
 */
constexpr EventSourceHandle kUnblockEventSourceHandle{kInvalidEventSourceHandle};

/**
 * @brief Decoded representation of an EventSourceHandle.
 */
struct EventSourceHandleData
{
    std::uint16_t index;
    std::uint64_t sequence_num;
    DispatcherTag tag;
};

/**
 * @brief Decode an EventSourceHandle into its index, sequence number and tag.
 */
EventSourceHandleData UnpackEventSourceHandle(const EventSourceHandle handle) noexcept
{
    EventSourceHandleData data{};
    data.index = static_cast<std::uint16_t>((handle & kEventSourceHandleIndexMask) >> kEventSourceHandleIndexOffset);
    data.sequence_num = handle & kEventSourceHandleSeqNumMask;
    data.tag = DispatcherTagOf(handle);
    return data;
}

/**
 * @brief Encode an index, sequence number and tag into an EventSourceHandle.
 */
EventSourceHandle PackEventSourceHandle(const EventSourceHandleData data) noexcept
{
    return internal::WithDispatcherTag((static_cast<EventSourceHandle>(data.index) << kEventSourceHandleIndexOffset) |
                                           (data.sequence_num & kEventSourceHandleSeqNumMask),
                                       data.tag);
}

/**
 * @brief Set O_NONBLOCK on a file descriptor. Returns false on failure.
 */
bool MakeNonBlocking(const NativeHandle fd) noexcept
{
    auto& fcntl = score::os::Fcntl::instance();
    const auto flags = fcntl.fcntl(fd, score::os::Fcntl::Command::kFileGetStatusFlags);
    if (!flags.has_value())
    {
        return false;
    }
    const auto set = fcntl.fcntl(
        fd, score::os::Fcntl::Command::kFileSetStatusFlags, flags.value() | score::os::Fcntl::Open::kNonBlocking);
    return set.has_value();
}

void CloseFd(const NativeHandle fd) noexcept
{
    if (fd != kInvalidNativeHandle)
    {
        score::cpp::ignore = score::os::Unistd::instance().close(fd);
    }
}

std::int32_t ClampTimeout(const std::chrono::milliseconds timeout) noexcept
{
    const std::chrono::milliseconds::rep ms{timeout.count()};
    if (ms <= 0)
    {
        return 0;
    }
    if (ms > static_cast<std::chrono::milliseconds::rep>(INT_MAX))
    {
        return INT_MAX;
    }
    return static_cast<std::int32_t>(ms);
}

/**
 * @brief Moves the callback out of `entry`, leaving the slot empty.
 * @pre `entry.mutex_` is held; the caller destroys the result after releasing it.
 */
CallbackType TakeCallback(internal::EventSourceEntry& entry) noexcept
{
    CallbackType taken{std::move(entry.callback_)};
    entry.callback_ = CallbackType{};
    return taken;
}

}  // namespace

score::Result<std::unique_ptr<IoDispatcher>> IoDispatcherImpl::Create(const std::uint16_t max_num_event_sources,
                                                                      const DispatcherTag tag) noexcept
{
    if (tag > kMaxDispatcherTag)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "tag exceeds kMaxDispatcherTag");
    }

    if (static_cast<std::uint64_t>(max_num_event_sources) > kMaxNumCallbacks)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "max_num_event_sources exceeds maximum (8191)");
    }

    // Wrapped syscall requires a C-style array.
    // NOLINTNEXTLINE(modernize-avoid-c-arrays) POSIX pipe() contract
    std::int32_t pipe_fds[2] = {kInvalidNativeHandle, kInvalidNativeHandle};
    const auto pipe_result = score::os::Unistd::instance().pipe(pipe_fds);
    if (!pipe_result.has_value())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "pipe() failed while creating unblock channel");
    }

    if ((!MakeNonBlocking(pipe_fds[0])) || (!MakeNonBlocking(pipe_fds[1])))
    {
        CloseFd(pipe_fds[0]);
        CloseFd(pipe_fds[1]);
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "failed to set unblock pipe non-blocking");
    }

    try
    {
        return std::unique_ptr<IoDispatcher>{
            new IoDispatcherImpl(max_num_event_sources, tag, pipe_fds[0], pipe_fds[1])};
    }
    catch (const std::bad_alloc&)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "failed to allocate object");
    }
}

IoDispatcherImpl::IoDispatcherImpl(const std::uint16_t max_num_event_sources,
                                   const DispatcherTag tag,
                                   const NativeHandle unblock_read_fd,
                                   const NativeHandle unblock_write_fd) noexcept
    : IoDispatcher{},
      tag_{tag},
      callbacks_{static_cast<std::size_t>(max_num_event_sources)},
      unblock_read_fd_{unblock_read_fd},
      unblock_write_fd_{unblock_write_fd}
{
    // Construct all slots up front so indices are stable and references never move.
    for (std::size_t i{0U}; i < max_num_event_sources; ++i)
    {
        score::cpp::ignore = callbacks_.emplace_back();
    }
}

IoDispatcherImpl::~IoDispatcherImpl() noexcept
{
    CloseFd(unblock_read_fd_);
    CloseFd(unblock_write_fd_);
}

score::Result<std::size_t> IoDispatcherImpl::FindFreeSlot(const NativeHandle io_source) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::find_free_entry");
    PERF_ZONE_VALUE(callbacks_.size());
    std::size_t free_index{callbacks_.size()};
    bool found_free{false};

    for (std::size_t i{0U}; i < callbacks_.size(); ++i)
    {
        internal::EventSourceEntry& entry = callbacks_[i];
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        if (entry.valid_)
        {
            if (entry.io_source_ == io_source)
            {
                return score::MakeUnexpected(IoDispatcherErrc::kApiError, "a callback is already registered for fd");
            }
        }
        else if ((!found_free) && (!entry.in_callback_))
        {
            free_index = i;
            found_free = true;
        }
        else
        {
            // keep searching for duplicate io_source registrations
        }
    }

    if (!found_free)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "no free callback slot");
    }
    return free_index;
}

template <typename Accessor>
auto IoDispatcherImpl::AccessSources(Accessor&& accessor) noexcept -> decltype(accessor())
{
    // The counter is raised before the flag is read, and HandleEvents() sets the flag before it reads the counter.
    // One of the two therefore sees the other, so this call either wakes the poll() or is waited for.
    score::cpp::ignore = num_of_accesses_.fetch_add(1U);
    if (in_wait_.load())
    {
        Unblock();
    }

    std::size_t pending_before{0U};
    auto result = [this, &accessor, &pending_before]() {
        const std::lock_guard<std::mutex> wait_lock{wait_mutex_};
        auto accessed = accessor();
        pending_before = num_of_accesses_.fetch_sub(1U);
        return accessed;
    }();

    if (pending_before == 1U)
    {
        accesses_finished_.notify_one();
    }

    return result;
}

score::Result<EventSourceHandle> IoDispatcherImpl::Register(const NativeHandle io_source,
                                                            const EventTypes events,
                                                            CallbackType callback) noexcept
{
    // poll() ignores a negative descriptor, so a registration of one would never dispatch.
    if (io_source < 0)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kInvalidHandle, "io_source is negative");
    }

    if (callback.empty())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback is empty");
    }

    return AccessSources([this, io_source, events, &callback]() -> score::Result<EventSourceHandle> {
        const auto index_result = FindFreeSlot(io_source);
        if (!index_result.has_value())
        {
            return score::MakeUnexpected<EventSourceHandle>(index_result.error());
        }

        const std::size_t index{index_result.value()};
        internal::EventSourceEntry& entry = callbacks_[index];
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        entry.io_source_ = io_source;
        entry.registered_events_ = events;
        entry.callback_ = std::move(callback);
        // Wraps within the handle's sequence bits, so a handle always decodes to the value stored here.
        entry.sequence_num_ = (entry.sequence_num_ + 1U) & kEventSourceHandleSeqNumMask;
        entry.valid_ = true;
        return PackEventSourceHandle({static_cast<std::uint16_t>(index), entry.sequence_num_, tag_});
    });
}

score::Result<void> IoDispatcherImpl::SetMonitoredEvents(const EventSourceHandle event_source_handle,
                                                         const EventTypes events) noexcept
{
    const EventSourceHandleData data{UnpackEventSourceHandle(event_source_handle)};
    if ((data.tag != tag_) || (static_cast<std::size_t>(data.index) >= callbacks_.size()))
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "invalid callback handle");
    }

    internal::EventSourceEntry& entry = callbacks_[data.index];
    return AccessSources([&entry, &data, events]() -> score::Result<void> {
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        if ((!entry.valid_) || (entry.sequence_num_ != data.sequence_num))
        {
            return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback handle is not registered");
        }

        entry.registered_events_ = events;
        return {};
    });
}

score::Result<void> IoDispatcherImpl::SetCallbackTarget(const EventSourceHandle event_source_handle,
                                                        CallbackType callback) noexcept
{
    const EventSourceHandleData data{UnpackEventSourceHandle(event_source_handle)};
    if ((data.tag != tag_) || (static_cast<std::size_t>(data.index) >= callbacks_.size()))
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "invalid callback handle");
    }

    internal::EventSourceEntry& entry = callbacks_[data.index];
    const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
    if ((!entry.valid_) || (entry.sequence_num_ != data.sequence_num))
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback handle is not registered");
    }
    if (entry.in_callback_)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback is currently executing");
    }
    entry.callback_ = std::move(callback);
    return {};
}

void IoDispatcherImpl::Unregister(const EventSourceHandle event_source_handle) noexcept
{
    const EventSourceHandleData data{UnpackEventSourceHandle(event_source_handle)};
    if ((data.tag != tag_) || (static_cast<std::size_t>(data.index) >= callbacks_.size()))
    {
        return;
    }

    internal::EventSourceEntry& entry = callbacks_[data.index];
    bool callback_running_elsewhere{false};
    // Declared outside the entry lock, so the callback is destroyed only after the lock is released: the destructors
    // of its captures run code the dispatcher does not control.
    CallbackType released{};
    // poll() holds a reference to every file in its set until it returns, so a socket closed during that wait stays
    // bound. The access runs only while the dispatch thread is outside poll(), so the closing caller is safe once
    // this returns.
    const bool unregistered = AccessSources([&entry, &data, &callback_running_elsewhere, &released]() {
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        if ((!entry.valid_) || (entry.sequence_num_ != data.sequence_num))
        {
            return false;
        }

        entry.valid_ = false;
        entry.registered_events_ = EventTypes{};
        // A callback unregistering its own source cannot wait for itself.
        callback_running_elsewhere = entry.in_callback_ && (entry.callback_thread_ != std::this_thread::get_id());
        // A running callback is released by HandleOneEvent() once it returns.
        if (!entry.in_callback_)
        {
            released = TakeCallback(entry);
        }

        return true;
    });
    if (!unregistered)
    {
        return;
    }

    // The dispatcher checks `valid_` and sets `in_callback_` under one lock, so no new invocation starts from here on.
    while (callback_running_elsewhere)
    {
        std::this_thread::yield();
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        callback_running_elsewhere = entry.in_callback_;
    }
}

score::Result<UnblockReason> IoDispatcherImpl::HandleEvents(const std::chrono::milliseconds timeout) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::handle_events");

    std::unique_lock<std::mutex> wait_lock{wait_mutex_};
    // Set before the counter is read, so an access that raises the counter after this wakes the poll() below.
    in_wait_.store(true);
    accesses_finished_.wait(wait_lock, [this]() {
        return num_of_accesses_.load() == 0U;
    });

    // Build the poll set: slot 0 is the unblock pipe, the rest are valid callbacks.
    std::vector<struct pollfd> poll_fds{};
    std::vector<EventSourceHandle> handles{};
    poll_fds.reserve(callbacks_.size() + 1U);
    handles.reserve(callbacks_.size() + 1U);

    poll_fds.push_back(pollfd{unblock_read_fd_, static_cast<short>(POLLIN), static_cast<short>(0)});
    handles.push_back(kUnblockEventSourceHandle);

    {
        PERF_ZONE_SCOPED("io_dispatcher::build_poll_set");
        PERF_ZONE_VALUE(callbacks_.size());
        for (std::size_t i{0U}; i < callbacks_.size(); ++i)
        {
            internal::EventSourceEntry& entry = callbacks_[i];
            const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
            // A source with no events is never dispatched, so it is left out: poll() would still report a hang-up
            // or an error on it at once, every time.
            if (entry.valid_ && entry.registered_events_.HasAnyEvent())
            {
                short events{0};
                if (entry.registered_events_.HasReadEvent())
                {
                    events = static_cast<short>(events | POLLIN);
                }
                if (entry.registered_events_.HasWriteEvent())
                {
                    events = static_cast<short>(events | POLLOUT);
                }
                poll_fds.push_back(pollfd{entry.io_source_, events, static_cast<short>(0)});
                handles.push_back(PackEventSourceHandle({static_cast<std::uint16_t>(i), entry.sequence_num_, tag_}));
            }
        }
    }
    PERF_ZONE_VALUE(poll_fds.size());

    const auto poll_result = [this, &poll_fds, timeout] {
        PERF_ZONE_SCOPED("io_dispatcher::wait");
        return score::os::SysPoll::instance().poll(
            poll_fds.data(), static_cast<nfds_t>(poll_fds.size()), ClampTimeout(timeout));
    }();
    in_wait_.store(false);
    wait_lock.unlock();

    if (!poll_result.has_value())
    {
        if (poll_result.error() == score::os::Error::Code::kOperationWasInterruptedBySignal)
        {
            return UnblockReason::kSignal;
        }
        score::mw::log::LogFatal() << "io_dispatcher: fatal error reported by poll()";
        return score::MakeUnexpected(IoDispatcherErrc::kFatal, "poll() failed");
    }

    if (poll_result.value() == 0)
    {
        return UnblockReason::kTimeout;
    }

    for (std::size_t k{0U}; k < poll_fds.size(); ++k)
    {
        if (poll_fds[k].revents != 0)
        {
            if (handles[k] == kUnblockEventSourceHandle)
            {
                HandleUnblock();
            }
            else
            {
                const short revents{poll_fds[k].revents};
                const bool err_or_hup{(revents & (POLLERR | POLLHUP)) != 0};
                EventTypes occurred{};
                occurred.SetReadEvent(((revents & POLLIN) != 0) || err_or_hup);
                occurred.SetWriteEvent(((revents & POLLOUT) != 0) || err_or_hup);
                HandleOneEvent(handles[k], occurred);
            }
        }
    }

    return UnblockReason::kEventsHandledOrUnblock;
}

void IoDispatcherImpl::HandleOneEvent(const EventSourceHandle event_source_handle, const EventTypes events) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::dispatch");
    const EventSourceHandleData data{UnpackEventSourceHandle(event_source_handle)};
    internal::EventSourceEntry& entry = callbacks_[data.index];

    bool call_callback{false};
    EventTypes filtered{};
    {
        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        if (entry.valid_ && (entry.sequence_num_ == data.sequence_num))
        {
            filtered.SetReadEvent(events.HasReadEvent() && entry.registered_events_.HasReadEvent());
            filtered.SetWriteEvent(events.HasWriteEvent() && entry.registered_events_.HasWriteEvent());
            if (filtered.HasAnyEvent())
            {
                call_callback = true;
                entry.in_callback_ = true;
                entry.callback_thread_ = std::this_thread::get_id();
            }
        }
    }

    // The entry mutex is released while the callback runs so the callback may re-enter the
    // dispatcher API (Register/Unregister/SetMonitoredEvents) on this handle.
    if (call_callback)
    {
        entry.callback_(event_source_handle, filtered);

        // The slot cannot be reused while `in_callback_` is set, so `valid_` still describes this registration.
        CallbackType released{};
        {
            const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
            if (!entry.valid_)
            {
                released = TakeCallback(entry);
            }
        }
        // Destroyed outside the entry lock and before `in_callback_` clears, so an Unregister() waiting for this
        // callback returns only after its captures are gone.
        released = CallbackType{};

        const std::lock_guard<std::mutex> entry_lock{entry.mutex_};
        entry.in_callback_ = false;
    }
}

void IoDispatcherImpl::HandleUnblock() noexcept
{
    std::array<std::uint8_t, 256U> buffer{};
    auto& unistd = score::os::Unistd::instance();
    while (true)
    {
        const auto read_result = unistd.read(unblock_read_fd_, buffer.data(), buffer.size());
        if (!read_result.has_value())
        {
            break;  // EAGAIN (drained) or EINTR
        }
        if (static_cast<std::size_t>(read_result.value()) < buffer.size())
        {
            break;  // fewer bytes than the buffer => pipe drained
        }
    }
}

void IoDispatcherImpl::Unblock() noexcept
{
    const std::uint8_t byte{1U};
    // If the pipe is full (EAGAIN) a wake-up is already pending, so ignore the result.
    score::cpp::ignore = score::os::Unistd::instance().write(unblock_write_fd_, &byte, sizeof(byte));
}

score::Result<std::unique_ptr<IoDispatcher>> MakeIoDispatcher(const std::uint16_t max_num_event_sources,
                                                              const DispatcherTag tag) noexcept
{
    return IoDispatcherImpl::Create(max_num_event_sources, tag);
}

std::uint16_t MaxNumEventSources() noexcept
{
    return static_cast<std::uint16_t>(kMaxNumCallbacks);
}

}  // namespace io_dispatcher
