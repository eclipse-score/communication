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
 * QNX backend of the io_dispatcher (channel + pulses + ionotify).
 *
 * Everything apart from ionotify() and TimerTimeout() goes through the score os wrappers (Channel / Neutrino).
 ********************************************************************************/

#include "score/io_dispatcher/qnx/io_dispatcher_impl.h"
#include "score/io_dispatcher/io_dispatcher_error.h"
#include "score/io_dispatcher/io_dispatcher_factory.h"
#include "score/mw/log/logging.h"
#include "score/os/qnx/channel.h"
#include "score/os/qnx/neutrino.h"
#include <measurement/perf.hpp>
#include <sys/iomgr.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <time.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>

namespace io_dispatcher
{
namespace
{

enum class HandleType : std::uint64_t
{
    kNative = 1UL << 63,
    kPulse = 1UL << 62,
};

/**
 * @brief Builds the handle of a source: `handle` in the low 32 bits, the dispatcher's tag, the type flag on top.
 */
EventSourceHandle ToEventSourceHandle(std::uint32_t handle, HandleType type, const DispatcherTag tag) noexcept
{
    auto handle_u64{static_cast<std::uint64_t>(handle)};
    auto type_u64{static_cast<std::uint64_t>(type)};
    return internal::WithDispatcherTag(handle_u64 | type_u64, tag);
}

bool IsIoSource(const EventSourceHandle event_source_handle) noexcept
{
    const auto type_u64{static_cast<std::uint64_t>(HandleType::kNative)};
    return static_cast<bool>(event_source_handle & type_u64);
}

bool IsPulseSource(const EventSourceHandle event_source_handle) noexcept
{
    const auto type_u64{static_cast<std::uint64_t>(HandleType::kPulse)};
    return static_cast<bool>(event_source_handle & type_u64);
}

/**
 * @brief Reserved pulse code used by Unblock().
 */
constexpr std::int8_t kPulseUnblockCode{static_cast<std::int8_t>(_PULSE_CODE_MINAVAIL)};
/**
 * @brief Pulse code an fd-readiness notification arrives with.
 *
 * @pre The event is registered with this code and marked updateable. That pair is what asks the descriptor's
 * manager to report the satisfied condition in the pulse; without it the event arrives exactly as registered.
 */
constexpr std::int8_t kPulseIoNotificationCode{static_cast<std::int8_t>(SI_NOTIFY)};

/**
 * @brief The notification conditions this backend ever arms for.
 */
constexpr std::int32_t kArmableConditions{_NOTIFY_COND_INPUT | _NOTIFY_COND_OUTPUT};

/**
 * @brief Split of the `_NOTIFY_DATA_MASK` bits of a notification pulse: the descriptor below, the generation above.
 * @note A pulse queued before Unregister() stays in the channel, and its descriptor number can be reused at once.
 * The generation tells that pulse from one of the new registration. It counts registrations per descriptor number,
 * so a match needs 256 registrations of one number while the pulse waits for the next receive.
 */
constexpr std::uint32_t kPulseDescriptorBits{20U};
constexpr std::uint32_t kPulseDescriptorMask{(1U << kPulseDescriptorBits) - 1U};
constexpr std::uint32_t kPulseGenerationMask{static_cast<std::uint32_t>(_NOTIFY_DATA_MASK) >> kPulseDescriptorBits};

/**
 * @brief Translate EventTypes read/write into ionotify _NOTIFY_COND_* flags.
 */
std::int32_t ToIoNotifyFlags(const EventTypes events) noexcept
{
    std::int32_t flags{0};
    if (events.HasReadEvent())
    {
        flags |= _NOTIFY_COND_INPUT;
    }
    if (events.HasWriteEvent())
    {
        flags |= _NOTIFY_COND_OUTPUT;
    }
    return flags;
}

/**
 * @brief Translate ionotify _NOTIFY_COND_* flags into EventTypes.
 */
EventTypes ToEventTypes(const std::int32_t flags) noexcept
{
    return EventTypes{}
        .SetReadEvent((flags & _NOTIFY_COND_INPUT) != 0)
        .SetWriteEvent((flags & _NOTIFY_COND_OUTPUT) != 0);
}

std::chrono::nanoseconds ClampTimeout(const std::chrono::milliseconds timeout) noexcept
{
    if (timeout.count() <= 0)
    {
        return std::chrono::nanoseconds{0};
    }

    constexpr std::chrono::nanoseconds::rep kNanosecondsPerMillisecond{
        std::chrono::nanoseconds{std::chrono::milliseconds{1}}.count()};
    constexpr std::chrono::milliseconds kLongestConvertibleTimeout{std::chrono::nanoseconds::max().count() /
                                                                   kNanosecondsPerMillisecond};
    if (timeout > kLongestConvertibleTimeout)
    {
        return std::chrono::nanoseconds::max();
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
}

/**
 * @brief Bound the calling thread's next MsgReceivePulse() to @a timeout.
 *
 * @note Calls the kernel directly. Neutrino::TimerTimeout() in this score_baselibs takes only a SigEvent, which it
 * refuses when null, and a receive timeout needs a null event.
 */
score::Result<void> ArmReceiveTimeout(const std::chrono::milliseconds timeout) noexcept
{
    const auto timeout_ns{static_cast<std::uint64_t>(ClampTimeout(timeout).count())};
    if (::TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, nullptr, &timeout_ns, nullptr) == -1)
    {
        score::mw::log::LogFatal() << "io_dispatcher: setting receive timer failed";
        return score::MakeUnexpected(IoDispatcherErrc::kFatal, "TimerTimeout() failed");
    }
    return {};
}

bool IsContainerFull(const internal::EventContainer& container) noexcept
{
    return container.size() >= container.capacity();
}

/**
 * @brief Set referenced flag to true on construction, false on destruction.
 */
class BoolGuard
{
  public:
    explicit BoolGuard(std::atomic_bool& flag) noexcept : flag_{flag}
    {
        flag_ = true;
    }

    ~BoolGuard() noexcept
    {
        flag_ = false;
    }

  private:
    std::atomic_bool& flag_;
};

std::shared_ptr<internal::CallbackSlot> MakeCallbackSlot(CallbackType callback)
{
    auto slot{std::make_shared<internal::CallbackSlot>()};
    slot->callback = std::move(callback);
    return slot;
}

}  // namespace

EventSourceHandle IoDispatcherImpl::NativeHandleOf(const std::uint32_t source) const noexcept
{
    return ToEventSourceHandle(source, HandleType::kNative, tag_);
}

EventSourceHandle IoDispatcherImpl::PulseHandleOf(const std::uint32_t code) const noexcept
{
    return ToEventSourceHandle(code, HandleType::kPulse, tag_);
}

score::Result<std::unique_ptr<IoDispatcher>> IoDispatcherImpl::Create(const std::uint16_t max_num_event_sources,
                                                                      const DispatcherTag tag) noexcept
{
    if (tag > kMaxDispatcherTag)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "tag exceeds kMaxDispatcherTag");
    }

    auto& neutrino = score::os::qnx::Neutrino::instance();
    const auto channel = neutrino.ChannelCreate(score::os::qnx::Neutrino::ChannelFlag::kPrivate);
    if (!channel.has_value())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "ChannelCreate() failed");
    }

    const auto connection =
        score::os::Channel::instance().ConnectAttach(0U, 0, channel.value(), _NTO_SIDE_CHANNEL, _NTO_COF_CLOEXEC);
    if (!connection.has_value())
    {
        score::cpp::ignore = neutrino.ChannelDestroy(channel.value());
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "ConnectAttach() failed");
    }

    try
    {
        return std::unique_ptr<IoDispatcher>(
            new IoDispatcherImpl(max_num_event_sources, tag, channel.value(), connection.value()));
    }
    catch (const std::bad_alloc&)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "failed to allocate object");
    }
}

IoDispatcherImpl::IoDispatcherImpl(const std::uint16_t max_num_event_sources,
                                   const DispatcherTag tag,
                                   const std::int32_t channel_id,
                                   const std::int32_t connection_id) noexcept
    : IoDispatcher{},
      max_num_event_sources_{max_num_event_sources},
      tag_{tag},
      channel_id_{channel_id},
      connection_id_{connection_id},
      sources_{},
      io_sources_to_rearm_{},
      _internal_received_events_{},
      dispatch_batch_{}
{
    sources_.reserve(max_num_event_sources);
    io_sources_to_rearm_.reserve(max_num_event_sources);
    _internal_received_events_.reserve(max_num_event_sources);
    dispatch_batch_.reserve(max_num_event_sources);
}

IoDispatcherImpl::~IoDispatcherImpl() noexcept
{
    for (auto& [_, event_source_entry] : sources_)
    {
        UnregisterNotificationEvent(event_source_entry);
    }
    if (connection_id_ >= 0)
    {
        score::cpp::ignore = score::os::Channel::instance().ConnectDetach(connection_id_);
    }
    if (channel_id_ >= 0)
    {
        score::cpp::ignore = score::os::qnx::Neutrino::instance().ChannelDestroy(channel_id_);
    }
}

score::Result<void> IoDispatcherImpl::RegisterNotificationEvent(internal::EventSourceEntry& event_source_entry) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::register_event");

    // Allow only one registration.
    if (event_source_entry.notification_event.has_value())
    {
        return {};
    }

    // Place `sigevent` in the optional.
    event_source_entry.notification_event = sigevent{};

    // The pulse carries the file descriptor, which `Register()` has checked to fit in kPulseDescriptorMask, and the
    // generation of this registration above it.
    const std::uint32_t pulse_value{static_cast<std::uint32_t>(event_source_entry.source) |
                                    (event_source_entry.generation << kPulseDescriptorBits)};
    SIGEV_PULSE_INT_INIT(&event_source_entry.notification_event.value(),
                         connection_id_,
                         SIGEV_PULSE_PRIO_INHERIT,
                         kPulseIoNotificationCode,
                         static_cast<std::int32_t>(pulse_value));
    // Make event updateable - satisfied conditions will be written into the sival of an event.
    SIGEV_MAKE_UPDATEABLE(&event_source_entry.notification_event.value());

    // Register the event.
    const auto register_result{score::os::Channel::instance().MsgRegisterEvent(
        &event_source_entry.notification_event.value(), event_source_entry.source)};
    if (!register_result.has_value())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kResource, "MsgRegisterEvent() failed");
    }
    return {};
}

void IoDispatcherImpl::UnregisterNotificationEvent(internal::EventSourceEntry& event_source_entry) noexcept
{
    if (event_source_entry.notification_event.has_value())
    {
        score::cpp::ignore =
            score::os::Channel::instance().MsgUnregisterEvent(&event_source_entry.notification_event.value());
        event_source_entry.notification_event = score::cpp::nullopt;
    }
}

score::Result<void> IoDispatcherImpl::AccessSources(
    score::cpp::callback<score::Result<void>(void), 64U> accessor) noexcept
{
    // The counter is raised before the flag is read, and the reactor sets the flag before it reads the counter.
    // One of the two therefore sees the other, so this call either sends the pulse or is waited for.
    score::cpp::ignore = num_of_accesses_.fetch_add(1U);

    if (receiving_events_)
    {
        Unblock();
    }

    score::Result<void> result{};
    std::size_t prev_num_of_accesses{0U};
    {
        const std::lock_guard<std::mutex> lock{dispatcher_mutex_};

        result = accessor();

        prev_num_of_accesses = num_of_accesses_.fetch_sub(1U, std::memory_order_acq_rel);
    }

    if (prev_num_of_accesses == 1U)
    {
        accesses_finished_.notify_one();
    }
    return result;
}

score::Result<EventSourceHandle> IoDispatcherImpl::Register(const NativeHandle io_source,
                                                            const EventTypes events,
                                                            CallbackType callback) noexcept
{
    // Disallow `io_source` exceeding the descriptor bits of the pulse. A negative one sets bits above them.
    if ((static_cast<std::uint32_t>(io_source) & ~kPulseDescriptorMask) != 0U)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kInvalidHandle, "io_source must fit in kPulseDescriptorMask");
    }

    if (callback.empty())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback is empty");
    }

    EventSourceHandle event_source_handle{kInvalidEventSourceHandle};
    auto access_sources_result{AccessSources([&]() -> score::Result<void> {
        // Disallow number of registrations exceeding number declared at construction.
        if (sources_.size() >= max_num_event_sources_)
        {
            return score::MakeUnexpected(IoDispatcherErrc::kResource, "register capacity reached");
        }

        // Disallow reregistration of the same IO source.
        event_source_handle = NativeHandleOf(static_cast<std::uint32_t>(io_source));
        if (sources_.find(event_source_handle) != sources_.end())
        {
            return score::MakeUnexpected(IoDispatcherErrc::kApiError, "io_source is already registered");
        }

        // Make an entry.
        internal::EventSourceEntry event_source_entry{};
        event_source_entry.source = io_source;
        event_source_entry.events = events;
        event_source_entry.callback = MakeCallbackSlot(std::move(callback));
        std::uint32_t& generation{generations_[static_cast<std::uint32_t>(io_source)]};
        generation = (generation + 1U) & kPulseGenerationMask;
        event_source_entry.generation = generation;

        // Register notification event and add entry to re-arm.
        auto register_event_result{RegisterNotificationEvent(event_source_entry)};
        if (!register_event_result.has_value())
        {
            return score::MakeUnexpected<void>(register_event_result.error());
        }
        io_sources_to_rearm_.insert(event_source_handle);

        // Push to map.
        sources_.insert(std::make_pair(event_source_handle, std::move(event_source_entry)));

        return {};
    })};

    if (!access_sources_result.has_value())
    {
        return score::MakeUnexpected<EventSourceHandle>(access_sources_result.error());
    }

    return event_source_handle;
}

score::Result<EventSourceHandle> IoDispatcherImpl::RegisterPulse(const EventTypes events,
                                                                 CallbackType callback) noexcept
{
    // Disallow non-pulse events.
    if (!events.HasPulseEvent())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "provided event type is not for pulses");
    }

    // Disallow reserved pulse codes.
    const PulseCode pulse_code{events.GetPulseCode()};
    if (pulse_code == kPulseUnblockCode || pulse_code == kPulseIoNotificationCode)
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "provided pulse code is reserved");
    }

    if (callback.empty())
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError, "callback is empty");
    }

    EventSourceHandle event_source_handle{kInvalidEventSourceHandle};
    auto access_sources_result{AccessSources([&]() -> score::Result<void> {
        // Disallow number of registrations exceeding number declared at construction.
        if (sources_.size() >= max_num_event_sources_)
        {
            return score::MakeUnexpected(IoDispatcherErrc::kResource, "register capacity reached");
        }

        // Reuse pulse code as a handle.
        event_source_handle = PulseHandleOf(static_cast<std::uint32_t>(pulse_code));
        // Disallow reregistration of the same pulse code.
        if (sources_.find(event_source_handle) != sources_.end())
        {
            return score::MakeUnexpected(IoDispatcherErrc::kApiError, "pulse code is already registered");
        }

        // Make an entry.
        internal::EventSourceEntry event_source_entry{};
        event_source_entry.source = pulse_code;
        event_source_entry.events = events;
        event_source_entry.callback = MakeCallbackSlot(std::move(callback));

        // Push to map.
        sources_.insert(std::make_pair(event_source_handle, std::move(event_source_entry)));

        return {};
    })};

    if (!access_sources_result.has_value())
    {
        return score::MakeUnexpected<EventSourceHandle>(access_sources_result.error());
    }

    return event_source_handle;
}

score::Result<void> IoDispatcherImpl::SetMonitoredEvents(const EventSourceHandle event_source_handle,
                                                         const EventTypes events) noexcept
{
    if (IsPulseSource(event_source_handle))
    {
        return score::MakeUnexpected(IoDispatcherErrc::kApiError);
    }

    return AccessSources([&]() -> score::Result<void> {
        // Check source is registered.
        auto it{sources_.find(event_source_handle)};
        if (it == sources_.end())
        {
            return score::MakeUnexpected(IoDispatcherErrc::kApiError, "invalid callback handle");
        }
        auto& event_source_entry{it->second};

        // Set events.
        event_source_entry.events = events;

        // Re-arm IO source.
        io_sources_to_rearm_.insert(event_source_handle);

        return {};
    });
}

score::Result<void> IoDispatcherImpl::SetCallbackTarget(const EventSourceHandle event_source_handle,
                                                        CallbackType callback) noexcept
{
    std::shared_ptr<internal::CallbackSlot> previous_slot{};
    auto access_sources_result{AccessSources([&]() -> score::Result<void> {
        // Check source is registered.
        auto it{sources_.find(event_source_handle)};
        if (it == sources_.end())
        {
            return score::MakeUnexpected(IoDispatcherErrc::kApiError, "invalid callback handle");
        }
        auto& event_source_entry{it->second};

        previous_slot = std::move(event_source_entry.callback);
        event_source_entry.callback = MakeCallbackSlot(std::move(callback));
        return {};
    })};

    RetireSlot(previous_slot);
    return access_sources_result;
}

std::shared_ptr<internal::CallbackSlot> IoDispatcherImpl::UnregisterInner(
    const EventSourceHandle event_source_handle) noexcept
{
    // Get entry iterator.
    auto it{sources_.find(event_source_handle)};

    // Do nothing if `event_source_handle` leads to nothing.
    if (it == sources_.end())
    {
        return nullptr;
    }
    auto& event_source_entry = it->second;

    // Cancel what this source still has armed.
    if (IsIoSource(event_source_handle))
    {
        score::cpp::ignore = ionotify(event_source_entry.source, _NOTIFY_ACTION_POLL, kArmableConditions, nullptr);
    }

    // Unregister notification event and erase.
    UnregisterNotificationEvent(event_source_entry);
    auto slot{std::move(event_source_entry.callback)};
    sources_.erase(it);
    io_sources_to_rearm_.erase(event_source_handle);
    return slot;
}

void IoDispatcherImpl::RetireSlot(const std::shared_ptr<internal::CallbackSlot>& slot) noexcept
{
    if (slot == nullptr)
    {
        return;
    }

    // One read-modify-write on each side orders this against the reactor's claim in `RunCallbacks()`.
    // Whichever comes second sees the other: the reactor skips a retired slot, or this call sees it running.
    const auto previous_flags{slot->flags.fetch_or(internal::CallbackSlot::kRetired, std::memory_order_acq_rel)};
    if ((previous_flags & internal::CallbackSlot::kRunning) != 0U)
    {
        // Callbacks run one at a time on the reactor thread, so a running slot seen from there is the caller's own.
        // It cannot wait for itself; the reactor drops the slot once it returns.
        if (reactor_thread_.load(std::memory_order_relaxed) == std::this_thread::get_id())
        {
            return;
        }
        while ((slot->flags.load(std::memory_order_acquire) & internal::CallbackSlot::kRunning) != 0U)
        {
            std::this_thread::yield();
        }
    }

    // The reactor never invokes a retired slot, so the captures go here rather than with the last batch reference.
    slot->callback = CallbackType{};
}

void IoDispatcherImpl::Unregister(const EventSourceHandle event_source_handle) noexcept
{
    std::shared_ptr<internal::CallbackSlot> slot{};
    score::cpp::ignore = AccessSources([&]() -> score::Result<void> {
        slot = UnregisterInner(event_source_handle);
        return {};
    });
    RetireSlot(slot);
}

score::Result<void> IoDispatcherImpl::RearmIoSources(internal::EventContainer& received_events) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::rearm_io_sources");
    PERF_ZONE_VALUE(io_sources_to_rearm_.size());

    // Pop IO sources requiring arm from the set.
    while (!io_sources_to_rearm_.empty())
    {
        // Check container capacity.
        // Do not proceed and do not pop from container if full.
        if (IsContainerFull(received_events))
        {
            break;
        }

        // Get and erase first element from the set.
        const auto event_source_handle{*io_sources_to_rearm_.begin()};
        io_sources_to_rearm_.erase(io_sources_to_rearm_.begin());

        // Find the entry, check if still available.
        auto it{sources_.find(event_source_handle)};
        if (it == sources_.end())
        {
            continue;
        }
        auto& event_source_entry{it->second};

        // Convert registered events into `ionotify` flags.
        const auto flags{ToIoNotifyFlags(event_source_entry.events)};

        // Execute `ionotify`.
        // `_NOTIFY_ACTION_POLLARM` returns the conditions already met, or arms the notification event when none are.
        const auto source{event_source_entry.source};
        auto& notification_event{event_source_entry.notification_event.value()};
        const std::int32_t ionotify_result{ionotify(source, _NOTIFY_ACTION_POLLARM, flags, &notification_event)};
        // Negative (-1) result indicates error.
        if (ionotify_result < 0)
        {
            // Bad file descriptor - unregister and proceed.
            if (score::os::geterrno() == EBADF)
            {
                score::cpp::ignore = UnregisterInner(event_source_handle);
                continue;
            }

            // Reinsert handle as it is not armed.
            io_sources_to_rearm_.insert(event_source_handle);

            score::mw::log::LogFatal() << "io_dispatcher: ionotify() failed while arming fd readiness";
            return score::MakeUnexpected(IoDispatcherErrc::kFatal, "ionotify() failed");
        }
        // Positive indicates active conditions, as requested by flags.
        else if (ionotify_result > 0)
        {
            // Push received event.
            internal::ReceivedEvent received_event{event_source_handle, ToEventTypes(ionotify_result)};

            received_events.push_back(received_event);
        }
        // Zero indicates conditions are not met, so the call armed.
        else
        {
            event_source_entry.armed = true;
        }
    }

    return {};
}

score::Result<EventTypes> IoDispatcherImpl::PollActiveEvents(
    const internal::EventSourceEntry& event_source_entry) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::poll_active");

    // `_NOTIFY_ACTION_POLL` reports the conditions that are true now without arming.
    // It is where a pulse's conditions come from when the manager reported none in the pulse itself.
    const auto armed_flags{ToIoNotifyFlags(event_source_entry.events)};
    const auto ionotify_result{ionotify(event_source_entry.source, _NOTIFY_ACTION_POLL, armed_flags, nullptr)};
    if (ionotify_result < 0)
    {
        // Do not unregister here, let `RearmIoSources` do it - just return empty.
        if (score::os::geterrno() == EBADF)
        {
            return EventTypes{};
        }

        score::mw::log::LogFatal() << "io_dispatcher: ionotify() failed while re-polling fd readiness";
        return score::MakeUnexpected(IoDispatcherErrc::kFatal, "ionotify() failed");
    }

    return ToEventTypes(ionotify_result);
}

score::Result<std::pair<IoDispatcherImpl::PulseResult, score::cpp::optional<internal::ReceivedEvent>>>
IoDispatcherImpl::ReceivePulse(const std::chrono::milliseconds timeout) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::receive_pulse");

    // Set the timeout.
    const auto set_timeout_result{ArmReceiveTimeout(timeout)};
    if (!set_timeout_result.has_value())
    {
        return score::MakeUnexpected<std::pair<PulseResult, score::cpp::optional<internal::ReceivedEvent>>>(
            set_timeout_result.error());
    }

    // Receive the pulse.
    struct _pulse pulse;
    const auto received{score::os::Channel::instance().MsgReceivePulse(channel_id_, &pulse, sizeof(pulse), nullptr)};
    if (!received.has_value())
    {
        using ErrorCode = score::os::Error::Code;
        if (received.error() == ErrorCode::kOperationWasInterruptedBySignal)
        {
            return std::make_pair(PulseResult::kSignal, score::cpp::nullopt);
        }
        else if (received.error() == ErrorCode::kKernelTimeout)
        {
            return std::make_pair(PulseResult::kTimeout, score::cpp::nullopt);
        }
        else
        {
            score::mw::log::LogFatal() << "io_dispatcher: fatal error reported by MsgReceivePulse()";
            return score::MakeUnexpected<std::pair<PulseResult, score::cpp::optional<internal::ReceivedEvent>>>(
                IoDispatcherErrc::kFatal);
        }
    }

    // IO notification received.
    if (pulse.code == kPulseIoNotificationCode)
    {
        const auto conditions{pulse.value.sival_int & kArmableConditions};
        const auto data{static_cast<std::uint32_t>(pulse.value.sival_int) &
                        static_cast<std::uint32_t>(_NOTIFY_DATA_MASK)};
        const auto fd{data & kPulseDescriptorMask};
        const auto generation{data >> kPulseDescriptorBits};
        const auto event_source_handle{NativeHandleOf(fd)};

        // Detect if the source is armed by `ionotify`.
        // This is required to avoid duplicate received events (same event observed by `ionotify` and pulse).
        // A generation other than the registration's marks a pulse that a closed socket queued on this number.
        const auto it{sources_.find(event_source_handle)};
        if (it == sources_.end() || !it->second.armed || it->second.generation != generation)
        {
            return std::make_pair(PulseResult::kDuplicateEvent, score::cpp::nullopt);
        }
        it->second.armed = false;

        // A pulse that names its condition is complete on arrival.
        // One that does not costs a round trip to ask.
        EventTypes events;
        if (conditions != 0U)
        {
            events = ToEventTypes(conditions);
        }
        else
        {
            auto poll_active_events_result{PollActiveEvents(it->second)};
            if (!poll_active_events_result.has_value())
            {
                return score::MakeUnexpected<
                    std::pair<IoDispatcherImpl::PulseResult, score::cpp::optional<internal::ReceivedEvent>>>(
                    poll_active_events_result.error());
            }
            events = *poll_active_events_result;
        }

        internal::ReceivedEvent received_event{event_source_handle, events};

        return std::make_pair(PulseResult::kEvent, received_event);
    }
    // Unblock notification received.
    else if (pulse.code == kPulseUnblockCode)
    {
        return std::make_pair(PulseResult::kUnblock, score::cpp::nullopt);
    }
    // Generic pulse received.
    else
    {
        const auto event_source_handle{PulseHandleOf(static_cast<std::uint32_t>(pulse.code))};

        internal::ReceivedEvent received_event{event_source_handle,
                                               EventTypes{}.SetPulseEvent(true).SetPulseCode(pulse.code)};

        return std::make_pair(PulseResult::kEvent, received_event);
    }
}

score::Result<UnblockReason> IoDispatcherImpl::HandlePulses(const std::chrono::milliseconds timeout,
                                                            internal::EventContainer& received_events) noexcept
{
    // Function is supposed to work like a `poll`.
    // - Drain non-blocking pulses, or
    // - Wait for events for specified timeout.
    PERF_ZONE_SCOPED("io_dispatcher::handle_pulses");

    // Drain non-blocking pulses.
    // Non-blocking part of execution is performed fully even when unblock is received.
    bool unblock_received{false};
    {
        PERF_ZONE_SCOPED("io_dispatcher::drain");

        // Loop until container is full or blocking call encountered.
        while (!IsContainerFull(received_events))
        {
            const std::chrono::milliseconds kZeroTimeout{0};
            const auto receive_pulse_result{ReceivePulse(kZeroTimeout)};
            if (!receive_pulse_result.has_value())
            {
                return score::MakeUnexpected<UnblockReason>(receive_pulse_result.error());
            }

            auto [pulse_result, received_event] = *receive_pulse_result;
            if (pulse_result == PulseResult::kTimeout)
            {
                // Hitting timeout on zero means operation is blocking.
                // No new non-blocking pulses to receive.
                break;
            }
            else if (pulse_result == PulseResult::kUnblock)
            {
                unblock_received = true;
            }
            else if (pulse_result == PulseResult::kEvent)
            {
                // Unchecked dereference of optional value.
                // `ReceivePulse` always provides value for `kEvent`.
                received_events.push_back(*received_event);
            }
            // Else: ignore signal interrupt and duplicate event here.
        }
    }

    // Wait for events for specified timeout.
    // Having already enqueued events or unblock pulse means non-blocking variant was executed.
    // Do not recheck for capacity - assume vector cannot be both empty and full.
    if (received_events.empty() && !unblock_received)
    {
        PERF_ZONE_SCOPED("io_dispatcher::wait");

        const auto receive_pulse_result{ReceivePulse(timeout)};
        if (!receive_pulse_result.has_value())
        {
            return score::MakeUnexpected<UnblockReason>(receive_pulse_result.error());
        }

        auto [pulse_result, received_event] = *receive_pulse_result;
        if (pulse_result == PulseResult::kTimeout)
        {
            return UnblockReason::kTimeout;
        }
        else if (pulse_result == PulseResult::kSignal)
        {
            return UnblockReason::kSignal;
        }
        else if (pulse_result == PulseResult::kEvent)
        {
            // Unchecked dereference of optional value.
            // `ReceivePulse` always provides value for `kEvent`.
            received_events.push_back(*received_event);
        }
        // Else: unblock, no events to dispatch.
    }

    return UnblockReason::kEventsHandledOrUnblock;
}

score::Result<UnblockReason> IoDispatcherImpl::ReceiveEvents(const std::chrono::milliseconds timeout,
                                                             internal::EventContainer& received_events,
                                                             std::unique_lock<std::mutex>& lock) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::receive_events");

    const BoolGuard receiving_events_guard{receiving_events_};

    // An accessor that raises the counter after this loop sees the flag set and sends `Unblock()`.
    while (num_of_accesses_.load() != 0)
    {
        accesses_finished_.wait(lock);
    }

    // Arm IO sources using `ionotify`.
    // Collect events available on `ionotify`.
    auto rearm_io_sources_result{RearmIoSources(received_events)};
    if (!rearm_io_sources_result.has_value())
    {
        return score::MakeUnexpected<UnblockReason>(rearm_io_sources_result.error());
    }

    // Handle incoming pulses.
    // Collect events announced via a pulse.
    return HandlePulses(timeout, received_events);
}

void IoDispatcherImpl::CollectCallback(const internal::ReceivedEvent& received_event) noexcept
{
    auto event_source_handle{received_event.handle};

    EventTypes events_to_deliver{received_event.events};

    const auto it{sources_.find(event_source_handle)};
    if (it == sources_.end())
    {
        return;
    }
    auto& event_source_entry{it->second};

    if (IsIoSource(received_event.handle))
    {
        events_to_deliver.SetReadEvent(events_to_deliver.HasReadEvent() && event_source_entry.events.HasReadEvent());
        events_to_deliver.SetWriteEvent(events_to_deliver.HasWriteEvent() && event_source_entry.events.HasWriteEvent());

        // Mark IO source to re-arm.
        io_sources_to_rearm_.insert(event_source_handle);
    }
    if (events_to_deliver.HasAnyEvent())
    {
        dispatch_batch_.push_back(
            internal::DispatchItem{event_source_entry.callback, event_source_handle, events_to_deliver});
    }
}

void IoDispatcherImpl::CollectCallbacks(const internal::EventContainer& received_events) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::collect_callbacks");

    for (const auto& received_event : received_events)
    {
        CollectCallback(received_event);
    }
}

void IoDispatcherImpl::RunCallbacks() noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::dispatch_events");

    for (auto& item : dispatch_batch_)
    {
        auto& slot{*item.slot};
        const auto previous_flags{slot.flags.fetch_or(internal::CallbackSlot::kRunning, std::memory_order_acq_rel)};
        if ((previous_flags & internal::CallbackSlot::kRetired) == 0U)
        {
            PERF_ZONE_SCOPED("io_dispatcher::dispatch_event");
            slot.callback(item.handle, item.events);
        }
        score::cpp::ignore = slot.flags.fetch_and(static_cast<std::uint8_t>(~internal::CallbackSlot::kRunning),
                                                  std::memory_order_release);
        item.slot.reset();
    }
    dispatch_batch_.clear();
}

score::Result<UnblockReason> IoDispatcherImpl::HandleEvents(const std::chrono::milliseconds timeout) noexcept
{
    PERF_ZONE_SCOPED("io_dispatcher::handle_events");

    score::Result<UnblockReason> receive_events_result{};
    {
        std::unique_lock<std::mutex> lock{dispatcher_mutex_};

        // Collected under the lock, so a callback unregistered before this point is never copied into the batch.
        receive_events_result = ReceiveEvents(timeout, _internal_received_events_, lock);
        if (receive_events_result.has_value())
        {
            CollectCallbacks(_internal_received_events_);
        }

        // Clear list of received events.
        _internal_received_events_.clear();
    }

    // Run the collected callbacks with the source table released.
    reactor_thread_.store(std::this_thread::get_id(), std::memory_order_relaxed);
    RunCallbacks();

    return receive_events_result;
}

void IoDispatcherImpl::Unblock() noexcept
{
    const std::int32_t kCallingThreadPriority{-1};
    score::cpp::ignore = score::os::Channel::instance().MsgSendPulse(
        connection_id_, kCallingThreadPriority, static_cast<std::int32_t>(kPulseUnblockCode), -1);
}

score::Result<std::unique_ptr<IoDispatcher>> MakeIoDispatcher(const std::uint16_t max_num_event_sources,
                                                              const DispatcherTag tag) noexcept
{
    return IoDispatcherImpl::Create(max_num_event_sources, tag);
}

std::uint16_t MaxNumEventSources() noexcept
{
    // The QNX backend keeps its sources in a hash map, so the parameter's type is the only bound.
    return std::numeric_limits<std::uint16_t>::max();
}

}  // namespace io_dispatcher
