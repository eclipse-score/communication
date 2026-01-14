/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include "score/message_passing/non_allocating_future/non_allocating_future.h"
#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

#include <iostream>

namespace score::message_passing
{

namespace
{

// false-positive: vars are being used in pulse _attach _detach calls
// coverity[autosar_cpp14_a0_1_1_violation]
constexpr std::int32_t kTimerPulseCode = _PULSE_CODE_MINAVAIL;
// coverity[autosar_cpp14_a0_1_1_violation]
constexpr std::int32_t kEventPulseCode = _PULSE_CODE_MINAVAIL + 1;

template <typename T>
// Suppress "AUTOSAR C++14 A9-5-1" rule finding: "Unions shall not be used.".
// coverity[autosar_cpp14_a9_5_1_violation: FALSE]. False positive: no any union in the following line. (Ticket-219101)
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: namespace-scoped function, not a global (Ticket-234468)
auto ValueOrTerminate(const score::cpp::expected<T, score::os::Error> expected, const std::string_view error_text) -> T
{
    if (!expected.has_value())
    {
        std::cerr << "QnxDispatchEngine: " << error_text << ": "
                  << expected.error().ToString()
                  // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call
                  // the function or it shall be preceded by &". Passing std::endl to std::cout object with the stream
                  // operator follows the idiomatic way that both features in conjunction were designed in the C++
                  // standard.
                  // coverity[autosar_cpp14_m8_4_4_violation : FALSE] Ticket-219101
                  << std::endl;
        std::terminate();
    }
    return expected.value();
}

template <typename T>
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: namespace-scoped function, not a global (Ticket-234468)
void IfUnexpectedTerminate(const score::cpp::expected<T, score::os::Error> expected, const std::string_view error_text)
{
    if (!expected.has_value())
    {
        std::cerr << "QnxDispatchEngine: " << error_text << ": "
                  << expected.error().ToString()
                  // Suppress AUTOSAR C++14 M8-4-4, rule finding: "A function identifier shall either be used to call
                  // the function or it shall be preceded by &". Passing std::endl to std::cout object with the stream
                  // operator follows the idiomatic way that both features in conjunction were designed in the C++
                  // standard.
                  // coverity[autosar_cpp14_m8_4_4_violation : FALSE] Ticket-219101
                  << std::endl;
        std::terminate();
    }
}

}  // namespace

QnxDispatchEngine::QnxDispatchEngine(score::cpp::pmr::memory_resource* memory_resource, OsResources os_resources) noexcept
    : ISharedResourceEngine{},
      memory_resource_{memory_resource},
      os_resources_{std::move(os_resources)},
      quit_flag_{false},
      thread_{},
      thread_mutex_{},
      thread_condition_{},
      timer_queue_{},
      posix_endpoint_list_{},
      posix_receive_buffer_{memory_resource},
      dispatch_pointer_{},
      context_pointer_{},
      side_channel_coid_{},
      timer_id_{},
      attach_mutex_{},
      connect_funcs_{},
      io_funcs_{}
{
    dispatch_pointer_ =  // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
        ValueOrTerminate(os_resources_.dispatch->dispatch_create_channel(-1, 0U), "Unable to allocate dispatch handle");
    IfUnexpectedTerminate(
        os_resources_.dispatch->pulse_attach(dispatch_pointer_, 0, kTimerPulseCode, &TimerPulseCallback, this),
        "Unable to attach timer pulse code");
    IfUnexpectedTerminate(
        os_resources_.dispatch->pulse_attach(dispatch_pointer_, 0, kEventPulseCode, &EventPulseCallback, this),
        "Unable to attach event pulse code");

    /* resmgr_attach */
    side_channel_coid_ =
        ValueOrTerminate(os_resources_.dispatch->message_connect(dispatch_pointer_, MSG_FLAG_SIDE_CHANNEL),
                         "Unable to create side channel");

    struct sigevent event{};
    // Suppress "AUTOSAR C++14 M5-0-21" rule finding: "Bitwise operators shall only be applied to operands of unsigned
    // underlying type.". 'SIGEV_PULSE' is a macro of QNX API, which defines bitwise operation of two flags. This macro
    // cannot be modified.
    // coverity[autosar_cpp14_m5_0_21_violation]
    event.sigev_notify = static_cast<decltype(event.sigev_notify)>(SIGEV_PULSE);
    event.sigev_coid = side_channel_coid_;
    event.sigev_priority = SIGEV_PULSE_PRIO_INHERIT;
    static_assert(kTimerPulseCode <= std::numeric_limits<decltype(event.sigev_code)>::max());
    event.sigev_code = static_cast<decltype(event.sigev_code)>(kTimerPulseCode);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    event.sigev_value.sival_int = 0;
    timer_id_ = ValueOrTerminate(os_resources_.timer->TimerCreate(CLOCK_MONOTONIC, &event), "Unable to create timer");

    // it's actually the default settings for resmgr buffers; we are just making them explicit here
    // Ourselves, we are not limited by these values, as we use resmgr_msgget() and we don't use ctp.iov
    resmgr_attr_t resmgr_attr{};
    resmgr_attr.nparts_max = 1U;
    resmgr_attr.msg_max_size = 2088U;
    IfUnexpectedTerminate(os_resources_.dispatch->resmgr_attach(
                              dispatch_pointer_, &resmgr_attr, nullptr, _FTYPE_ANY, 0U, nullptr, nullptr, nullptr),
                          "Unable to set up resource manager operations");

    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    context_pointer_ = ValueOrTerminate(os_resources_.dispatch->dispatch_context_alloc(dispatch_pointer_),
                                        "Unable to allocate context pointer");

    SetupResourceManagerCallbacks();

    // Suppress "AUTOSAR C++14 A8-5-3" rule finding: "A variable of type auto shall not be initialized using {} or
    // ={} braced initialization.". (Ticket-219101)
    // coverity[autosar_cpp14_a8_5_3_violation: FALSE] False positive: the variable is not declared with 'auto'.
    std::lock_guard acquire{thread_mutex_};  // postpone thread start till we assign thread_
    thread_ = std::thread([this]() noexcept {
        {
            // Suppress "AUTOSAR C++14 A8-5-3" rule finding: "A variable of type auto shall not be initialized using {}
            // or ={} braced initialization.". (Ticket-219101)
            // coverity[autosar_cpp14_a8_5_3_violation: FALSE] False positive: the variable is not declared with 'auto'.
            std::lock_guard release{thread_mutex_};
        }
        RunOnThread();
    });
}

// Note 'C++14 A8-4-10':
// Suppress AUTOSAR C++14 A8-4-10 rule findigs: "A parameter shall be passed by reference if it can't be NULL."
// Justification: raw pointers are used in method signatures to maintain compatibility with the QNX API,
// which provides parameters as raw pointers.

int QnxDispatchEngine::EndpointFdSelectCallback(select_context_t* /*ctp*/,
                                                int /*fd*/,
                                                unsigned /*flags*/,
                                                // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                                void* handle) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // QNX API
    // coverity[autosar_cpp14_m5_2_8_violation]
    PosixEndpointEntry& endpoint = *static_cast<PosixEndpointEntry*>(handle);
    endpoint.input();
    return 0;
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
// coverity[autosar_cpp14_a0_1_3_violation] false-positive: used as a pulse callback
int QnxDispatchEngine::TimerPulseCallback(message_context_t* /*ctp*/,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                          void* handle) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // QNX API
    // coverity[autosar_cpp14_m5_2_8_violation]
    static_cast<QnxDispatchEngine*>(handle)->ProcessTimerQueue();
    return 0;
}

// coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
// coverity[autosar_cpp14_a0_1_3_violation] false-positive: used as a pulse callback
int QnxDispatchEngine::EventPulseCallback(message_context_t* ctp,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                          void* handle) noexcept
{
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) C API
    // Suppress "AUTOSAR C++14 A7-2-1" rule: "An expression with enum underlying type shall only have values
    // corresponding to the enumerators of the enumeration.".
    // Passing the enum value unchanged through the OS API
    // coverity[autosar_cpp14_a7_2_1_violation]
    const auto pulse_event = static_cast<std::int32_t>(ctp->msg->pulse.value.sival_int);
    // NOLINTEND(cppcoreguidelines-pro-type-union-access) C API
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // QNX API
    // coverity[autosar_cpp14_m5_2_8_violation]
    static_cast<QnxDispatchEngine*>(handle)->ProcessPulseEvent(pulse_event);
    return 0;
}

QnxDispatchEngine::~QnxDispatchEngine() noexcept
{
    SendPulseEvent(PulseEvent::QUIT);
    thread_.join();
    score::cpp::ignore = os_resources_.timer->TimerDestroy(timer_id_);
    score::cpp::ignore = os_resources_.channel->ConnectDetach(side_channel_coid_);
    score::cpp::ignore = os_resources_.dispatch->pulse_detach(dispatch_pointer_, kEventPulseCode, 0);
    score::cpp::ignore = os_resources_.dispatch->pulse_detach(dispatch_pointer_, kTimerPulseCode, 0);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = os_resources_.dispatch->dispatch_destroy(dispatch_pointer_);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    os_resources_.dispatch->dispatch_context_free(context_pointer_);
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
score::cpp::expected<std::int32_t, score::os::Error> QnxDispatchEngine::TryOpenClientConnection(
    std::string_view identifier) noexcept
{
    const detail::QnxResourcePath path{identifier};
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    return os_resources_.fcntl->open(path.c_str(),
                                     score::os::Fcntl::Open::kReadWrite | score::os::Fcntl::Open::kCloseOnExec);
}

void QnxDispatchEngine::CloseClientConnection(std::int32_t client_fd) noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = os_resources_.unistd->close(client_fd);
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
void QnxDispatchEngine::RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    const auto needed_buffer_size = static_cast<std::size_t>(endpoint.max_receive_size) + 1U;
    if (posix_receive_buffer_.size() < needed_buffer_size)
    {
        posix_receive_buffer_.resize(needed_buffer_size);
    }

    score::cpp::ignore = os_resources_.dispatch->select_attach(
        dispatch_pointer_,
        nullptr,
        endpoint.fd,
        static_cast<std::uint32_t>(SELECT_FLAG_READ) | static_cast<std::uint32_t>(SELECT_FLAG_REARM),
        &EndpointFdSelectCallback,
        &endpoint);
    posix_endpoint_list_.push_back(endpoint);
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
void QnxDispatchEngine::UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    score::cpp::ignore = posix_endpoint_list_.erase(posix_endpoint_list_.iterator_to(endpoint));
    UnselectEndpoint(endpoint);
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
void QnxDispatchEngine::UnselectEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    score::cpp::ignore = os_resources_.dispatch->select_detach(dispatch_pointer_, endpoint.fd);
    if (!endpoint.disconnect.empty())
    {
        endpoint.disconnect();
    }
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
void QnxDispatchEngine::EnqueueCommand(CommandQueueEntry& entry,
                                       const TimePoint until,
                                       CommandCallback callback,
                                       const void* const owner) noexcept
{
    timer_queue_.RegisterTimedEntry(entry, until, std::move(callback), owner);
    SendPulseEvent(PulseEvent::TIMER);
}

void QnxDispatchEngine::CleanUpOwner(const void* const owner) noexcept
{
    if (owner == nullptr)
    {
        return;
    }
    if (IsOnCallbackThread())
    {
        ProcessCleanup(owner);
    }
    else
    {
        // Suppress "AUTOSAR C++14 A8-5-3" rule finding: "A variable of type auto shall not be initialized using {} or
        // ={} braced initialization.". (Ticket-219101)
        // coverity[autosar_cpp14_a8_5_3_violation: FALSE] False positive: the variable is not declared with 'auto'
        detail::NonAllocatingFuture future{thread_mutex_, thread_condition_};
        detail::TimedCommandQueue::Entry cleanup_command;
        timer_queue_.RegisterImmediateEntry(
            cleanup_command,
            [this, owner, &future](auto) noexcept {
                ProcessCleanup(owner);
                future.MarkReady();
            },
            owner);
        SendPulseEvent(PulseEvent::TIMER);
        future.Wait();
    }
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::SendProtocolMessage(
    const std::int32_t fd,
    std::uint8_t code,
    const score::cpp::span<const std::uint8_t> message) noexcept
{
    // Suppress AUTOSAR C++14 Rule A0-1-1 rule finding: "A project shall not contain instances
    // of non-volatile variables being given values that are not subsequently used."
    // False positive: the variable is used immediately after its declaration.
    // coverity[autosar_cpp14_a0_1_1_violation: FALSE] Ticket-219101
    constexpr auto kVectorCount = 2UL;
    std::array<iov_t, kVectorCount> io{};
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) C API
    io[0].iov_base = &code;
    io[0].iov_len = sizeof(code);
    // Suppress "AUTOSAR C++14 A5-2-3" rule finding: A cast shall not remove any const or volatile
    // qualification from the type of a pointer or reference.
    // Rationale: OS API
    // coverity[autosar_cpp14_a5_2_3_violation]
    io[1].iov_base = const_cast<std::uint8_t*>(message.data());
    io[1].iov_len = static_cast<std::size_t>(message.size());
    // NOLINTEND(cppcoreguidelines-pro-type-union-access) C API
    auto result_expected = os_resources_.uio->writev(fd, io.data(), static_cast<std::int32_t>(io.size()));
    if (result_expected.has_value())
    {
        return {};
    }
    return score::cpp::make_unexpected(result_expected.error());
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> QnxDispatchEngine::ReceiveProtocolMessage(
    const std::int32_t fd,
    std::uint8_t& code) noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    auto size_expected = os_resources_.unistd->read(fd, posix_receive_buffer_.data(), posix_receive_buffer_.size());
    if (!size_expected.has_value())
    {
        return score::cpp::make_unexpected(size_expected.error());
    }
    ssize_t size = size_expected.value();
    if (size < 1)
    {
        // other side disconnected
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    }
    code = posix_receive_buffer_[0];
    const auto span_size = static_cast<score::cpp::span<const std::uint8_t>::size_type>(size) - 1U;
    return score::cpp::span<const std::uint8_t>{&posix_receive_buffer_[1], span_size};
}

void QnxDispatchEngine::SendPulseEvent(const PulseEvent pulse_event) noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = os_resources_.channel->MsgSendPulse(
        side_channel_coid_, -1, kEventPulseCode, static_cast<std::int32_t>(pulse_event));
}

void QnxDispatchEngine::ProcessPulseEvent(const std::int32_t pulse_event) noexcept
{
    if (pulse_event == score::cpp::to_underlying(PulseEvent::TIMER))
    {
        ProcessTimerQueue();
    }
    else
    {
        quit_flag_ = true;
    }
}

void QnxDispatchEngine::ProcessCleanup(const void* const owner) noexcept
{
    posix_endpoint_list_.remove_and_dispose_if(
        [owner](auto& endpoint) noexcept {
            return endpoint.owner == owner;
        },
        [this](auto* endpoint) noexcept {
            UnselectEndpoint(*endpoint);
        });

    timer_queue_.CleanUpOwner(owner);
}

void QnxDispatchEngine::RunOnThread() noexcept
{
    while (!quit_flag_)
    {
        // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
        if (os_resources_.dispatch->dispatch_block(context_pointer_))
        {
            // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
            score::cpp::ignore = os_resources_.dispatch->dispatch_handler(context_pointer_);
        }
    }
}

void QnxDispatchEngine::ProcessTimerQueue() noexcept
{
    const auto now = Clock::now();
    const auto then = timer_queue_.ProcessQueue(now);
    if (then == TimePoint{})
    {
        // no future event to wait for (yet); no need to re-arm timer
        return;
    }
    const auto distance = std::chrono::duration_cast<std::chrono::nanoseconds>(then - Clock::now()).count() + 1;
    struct _itimer itimer{};
    itimer.nsec = static_cast<std::uint64_t>(distance);
    itimer.interval_nsec = 0U;
    score::cpp::ignore = os_resources_.timer->TimerSettime(timer_id_, 0, &itimer, nullptr);
}

void QnxDispatchEngine::SetupResourceManagerCallbacks() noexcept
{
    // pre-configure resmgr callback data
    os_resources_.iofunc->iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs_, _RESMGR_IO_NFUNCS, &io_funcs_);
    // Suppress "AUTOSAR C++14 M5-2-6", The rule states: "A cast shall not convert a pointer to a function
    // to any other pointer type, including a pointer to function type used."
    // QNX API
    // coverity[autosar_cpp14_m5_2_6_violation]
    connect_funcs_.open = &io_open;
    // coverity[autosar_cpp14_m5_2_6_violation]
    io_funcs_.notify = &io_notify;
    // coverity[autosar_cpp14_m5_2_6_violation]
    io_funcs_.write = &io_write;
    // coverity[autosar_cpp14_m5_2_6_violation]
    io_funcs_.read = &io_read;
    // coverity[autosar_cpp14_m5_2_6_violation]
    io_funcs_.close_ocb = &io_close_ocb;
}

// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::StartServer(ResourceManagerServer& server,
                                                                   const QnxResourcePath& path) noexcept
{
    // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer

    // Suppress "AUTOSAR C++14 A8-5-3" rule finding: "A variable of type auto shall not be initialized using {} or ={}
    // braced initialization.".
    // coverity[autosar_cpp14_a8_5_3_violation: FALSE] False positive: the variable is not declared with 'auto'.
    std::lock_guard guard{attach_mutex_};

    const auto id_expected = os_resources_.dispatch->resmgr_attach(dispatch_pointer_,
                                                                   nullptr,
                                                                   path.c_str(),
                                                                   _FTYPE_ANY,
                                                                   static_cast<std::uint32_t>(_RESMGR_FLAG_SELF),
                                                                   &connect_funcs_,
                                                                   &io_funcs_,
                                                                   &server);
    if (!id_expected.has_value())
    {
        return score::cpp::make_unexpected(id_expected.error());
    }
    server.resmgr_id_ = id_expected.value();

    // pre-configure resmgr access rights data; attr member is from the extended_dev_attr_t base class

    // Suppress "AUTOSAR C++14 M5-0-21" rule finding: "Bitwise operators shall only be applied
    // to operands of unsigned underlying type.".
    // All permission macros (S_IRUSR, S_IWUSR, etc.) are defined as unsigned constants.
    // The bitwise OR operation is performed exclusively on unsigned values,
    // and the result is assigned to an auto-deduced unsigned type
    // coverity[autosar_cpp14_m5_0_21_violation]
    constexpr auto perm_0666 = static_cast<std::uint32_t>(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    constexpr mode_t attrMode{static_cast<std::uint32_t>(S_IFNAM) | perm_0666};
    os_resources_.iofunc->iofunc_attr_init(&server.attr, attrMode, nullptr, nullptr);

    return {};
}

void QnxDispatchEngine::StopServer(ResourceManagerServer& server) noexcept
{
    if (server.resmgr_id_ != -1)
    {
        // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer

        // Suppress "AUTOSAR C++14 A8-5-3" rule finding: "A variable of type auto shall not be initialized using {} or
        // ={} braced initialization.".
        // coverity[autosar_cpp14_a8_5_3_violation: FALSE] False positive: the variable is not declared with 'auto'.
        std::lock_guard guard{attach_mutex_};
        const auto detach_expected = os_resources_.dispatch->resmgr_detach(
            dispatch_pointer_, server.resmgr_id_, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE));
        server.resmgr_id_ = -1;
    }
}

// Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
// QNX union.
std::int32_t QnxDispatchEngine::io_open(resmgr_context_t* const ctp,
                                        // coverity[autosar_cpp14_a9_5_1_violation]
                                        io_open_t* const msg,
                                        // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                        RESMGR_HANDLE_T* const handle,
                                        void* const /*extra*/) noexcept
{
    ResourceManagerServer& server = ResmgrHandleToServer(handle);
    QnxDispatchEngine& self = *server.engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    // the attr locks are currently not needed, but we should not forget about them in multithreaded implementation
    score::cpp::ignore = iofunc->iofunc_attr_lock(&server.attr);
    const score::cpp::expected_blank<std::int32_t> status = iofunc->iofunc_open(ctp, msg, &server.attr, nullptr, nullptr);
    if (!status.has_value())
    {
        score::cpp::ignore = iofunc->iofunc_attr_unlock(&server.attr);
        return status.error();
    }

    const auto result = server.ProcessConnect(ctp, msg);
    score::cpp::ignore = iofunc->iofunc_attr_unlock(&server.attr);
    return result;
}

// Suppress "AUTOSAR C++14 A9-5-1" rule finding: "Unions shall not be used.".
score::cpp::expected_blank<std::int32_t> QnxDispatchEngine::AttachConnection(
    resmgr_context_t* const ctp,
    // coverity[autosar_cpp14_a9_5_1_violation] 'io_open_t' type is defined by the QNX API.
    io_open_t* const msg,
    ResourceManagerServer& server,
    ResourceManagerConnection& connection) noexcept
{
    QnxDispatchEngine& self = *server.engine_;
    auto& os_resources = self.GetOsResources();
    auto& iofunc = os_resources.iofunc;
    return iofunc->iofunc_ocb_attach(ctp, msg, &connection, &server.attr, nullptr);
}

// coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
std::int32_t QnxDispatchEngine::io_write(resmgr_context_t* const ctp,
                                         // coverity[autosar_cpp14_a9_5_1_violation]
                                         // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                         io_write_t* const msg,
                                         RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    // check if the write operation is allowed
    auto result = iofunc->iofunc_write_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    // check if we are requested to do just a plain write
    if ((msg->i.xtype & static_cast<std::uint32_t>(_IO_XTYPE_MASK)) != static_cast<std::uint32_t>(_IO_XTYPE_NONE))
    {
        return ENOSYS;
    }

    // get the number of bytes we were asked to write, check that there are enough bytes in the message

    // Suppress following rule findings:
    // - "AUTOSAR C++14 A4-7-1": "An integer expression shall not lead to data loss.".
    // - "AUTOSAR C++14 A5-2-2": "Traditional C-style casts shall not be used.".
    // - "AUTOSAR C++14 A5-16-1": "The ternary conditional operator shall not be used as a sub-expression.".
    // - "AUTOSAR C++14 M5-0-4": "An implicit integral conversion shall not change the signedness of the underlying
    // type.".
    // - "AUTOSAR C++14 M5-0-21": "Bitwise operators shall only be applied to operands of unsigned underlying type.".
    // The findings relate to _IO_WRITE_GET_NBYTES macro which is a part of QNX API and cannot be modified.
    // coverity[autosar_cpp14_a4_7_1_violation]
    // coverity[autosar_cpp14_a5_2_2_violation]
    // coverity[autosar_cpp14_a5_16_1_violation]
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    const std::size_t nbytes = _IO_WRITE_GET_NBYTES(msg);
    if (nbytes < 1U)
    {
        return EBADMSG;
    }

    // check that the message doesn't ask us to access beyond the valid part of the message
    // this may happen either if we have not received the whole message (ctp->info.msglen < ctp->info.srcmsglen)
    // or if the sent message is malformed (nbytes field contains incorrect information)
    const auto nbytes_max = static_cast<std::int64_t>(ctp->info.msglen) - static_cast<std::int64_t>(ctp->offset) -
                            static_cast<std::int64_t>(sizeof(io_write_t));
    if (nbytes > static_cast<std::size_t>(nbytes_max))
    {
        return EMSGSIZE;
    }

    // the message payload starts right after the io_write_t message header

    // Suppress "AUTOSAR C++14 M5-0-15" rule findings: "Array indexing shall be the only form of pointer arithmetic.".
    // Rationale: Pointer arithmetic used to access payload after io_write_t header, layout is guaranteed by QNX API.

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic) C API
    // coverity[autosar_cpp14_m5_0_15_violation] see Rationale above
    // coverity[autosar_cpp14_m5_2_8_violation] handling raw data from C-style API payload
    const auto buffer = const_cast<const std::uint8_t*>(static_cast<std::uint8_t*>(static_cast<void*>(&msg[1])));
    const std::uint8_t code = buffer[0];
    // coverity[autosar_cpp14_m5_0_15_violation] see Rationale above
    const score::cpp::span<const std::uint8_t> message = {&buffer[1],
                                                   static_cast<score::cpp::span<std::uint8_t>::size_type>(nbytes - 1U)};
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic) C API

    // TODO: close connection on false (once this functionality is demanded)
    score::cpp::ignore = OcbToConnection(ocb).ProcessInput(code, message);

    _IO_SET_WRITE_NBYTES(ctp, static_cast<std::int64_t>(nbytes));
    return EOK;
}

// coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
std::int32_t QnxDispatchEngine::io_read(resmgr_context_t* const ctp,
                                        // coverity[autosar_cpp14_a9_5_1_violation]
                                        // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                        io_read_t* const msg,
                                        RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    auto result = iofunc->iofunc_read_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    if ((msg->i.xtype & static_cast<std::uint32_t>(_IO_XTYPE_MASK)) != static_cast<std::uint32_t>(_IO_XTYPE_NONE))
    {
        return ENOSYS;
    }

    // Suppress following rule findings:
    // - "AUTOSAR C++14 A4-7-1": "An integer expression shall not lead to data loss.".
    // - "AUTOSAR C++14 A5-2-2": "Traditional C-style casts shall not be used.".
    // - "AUTOSAR C++14 A5-16-1": "The ternary conditional operator shall not be used as a sub-expression.".
    // - "AUTOSAR C++14 M5-0-4": "An implicit integral conversion shall not change the signedness of the underlying
    //    type.".
    // - "AUTOSAR C++14 M5-0-21": "Bitwise operators shall only be applied to operands of unsigned underlying type.".
    // The findings relate to _IO_READ_GET_NBYTES macro which is a part of QNX API and cannot be modified.
    // coverity[autosar_cpp14_a4_7_1_violation]
    // coverity[autosar_cpp14_a5_2_2_violation]
    // coverity[autosar_cpp14_a5_16_1_violation]
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    const size_t nbytes = _IO_READ_GET_NBYTES(msg);
    if (nbytes == 0U)
    {
        _IO_SET_READ_NBYTES(ctp, 0);
        return _RESMGR_NPARTS(0);
    }

    ResourceManagerConnection& connection = OcbToConnection(ocb);
    return connection.ProcessReadRequest(ctp);
}

std::int32_t QnxDispatchEngine::io_notify(resmgr_context_t* const ctp,
                                          // coverity[autosar_cpp14_a9_5_1_violation]
                                          io_notify_t* const msg,
                                          // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                          RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    ResourceManagerConnection& connection = OcbToConnection(ocb);

    // 'trig' will tell iofunc_notify() which conditions are currently satisfied.
    auto trig = static_cast<std::uint32_t>(_NOTIFY_COND_OUTPUT); /* clients can always give us data */
    if (connection.HasSomethingToRead())
    {
        // NOLINTNEXTLINE(hicpp-signed-bitwise): QNX API
        trig |= static_cast<std::uint32_t>(_NOTIFY_COND_INPUT); /* we have some data available */
    }
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    return iofunc->iofunc_notify(
        ctp, msg, connection.notify_.data(), static_cast<std::int32_t>(trig), nullptr, nullptr);
}

std::int32_t QnxDispatchEngine::io_close_ocb(resmgr_context_t* const ctp,
                                             void* const /*reserved*/,
                                             // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                             RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    ResourceManagerConnection& connection = OcbToConnection(ocb);

    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_.data(), INT_MAX, IOFUNC_NOTIFY_INPUT);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_.data(), INT_MAX, IOFUNC_NOTIFY_OUTPUT);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_.data(), INT_MAX, IOFUNC_NOTIFY_OBAND);

    iofunc->iofunc_notify_remove(ctp, &connection.notify_[0]);

    // the attr locks are currently not needed, but we should not forget about them in multithreaded implementation
    score::cpp::ignore = iofunc->iofunc_attr_lock(&ocb->attr->attr);
    score::cpp::ignore = iofunc->iofunc_ocb_detach(ctp, ocb);
    score::cpp::ignore = iofunc->iofunc_attr_unlock(&ocb->attr->attr);

    connection.ProcessDisconnect();
    return EOK;
}

}  // namespace score::message_passing
