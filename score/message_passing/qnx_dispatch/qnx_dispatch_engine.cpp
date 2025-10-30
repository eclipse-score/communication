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

constexpr std::int32_t kTimerPulseCode = _PULSE_CODE_MINAVAIL;
constexpr std::int32_t kEventPulseCode = _PULSE_CODE_MINAVAIL + 1;

template <typename T>
T ValueOrTerminate(const score::cpp::expected<T, score::os::Error> expected, const std::string_view error_text)
{
    if (!expected.has_value())
    {
        std::cerr << "QnxDispatchEngine: " << error_text << ": " << expected.error().ToString() << std::endl;
        std::terminate();
    }
    return expected.value();
}

template <typename T>
void IfUnexpectedTerminate(const score::cpp::expected<T, score::os::Error> expected, const std::string_view error_text)
{
    if (!expected.has_value())
    {
        std::cerr << "QnxDispatchEngine: " << error_text << ": " << expected.error().ToString() << std::endl;
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
        ValueOrTerminate(os_resources_.dispatch->dispatch_create_channel(-1, 0), "Unable to allocate dispatch handle");
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
    event.sigev_notify = SIGEV_PULSE;
    event.sigev_coid = side_channel_coid_;
    event.sigev_priority = SIGEV_PULSE_PRIO_INHERIT;
    event.sigev_code = kTimerPulseCode;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) C API
    event.sigev_value.sival_int = 0;
    timer_id_ = ValueOrTerminate(os_resources_.timer->TimerCreate(CLOCK_MONOTONIC, &event), "Unable to create timer");

    // it's actually the default settings for resmgr buffers; we are just making them explicit here
    // Ourselves, we are not limited by these values, as we use resmgr_msgget() and we don't use ctp.iov
    resmgr_attr_t resmgr_attr_;
    resmgr_attr_.nparts_max = 1U;
    resmgr_attr_.msg_max_size = 2088U;
    IfUnexpectedTerminate(os_resources_.dispatch->resmgr_attach(
                              dispatch_pointer_, &resmgr_attr_, nullptr, _FTYPE_ANY, 0, nullptr, nullptr, nullptr),
                          "Unable to set up resource manager operations");

    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    context_pointer_ = ValueOrTerminate(os_resources_.dispatch->dispatch_context_alloc(dispatch_pointer_),
                                        "Unable to allocate context pointer");

    SetupResourceManagerCallbacks();

    std::lock_guard acquire{thread_mutex_};  // postpone thread start till we assign thread_
    thread_ = std::thread([this]() noexcept {
        {
            std::lock_guard release{thread_mutex_};
        }
        RunOnThread();
    });
}

int QnxDispatchEngine::EndpointFdSelectCallback(select_context_t* /*ctp*/,
                                                int /*fd*/,
                                                unsigned /*flags*/,
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

int QnxDispatchEngine::TimerPulseCallback(message_context_t* /*ctp*/,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          void* handle) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // QNX API
    // coverity[autosar_cpp14_m5_2_8_violation]
    static_cast<QnxDispatchEngine*>(handle)->ProcessTimerQueue();
    return 0;
}

int QnxDispatchEngine::EventPulseCallback(message_context_t* ctp,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          void* handle) noexcept
{
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) C API
    // Suppress "AUTOSAR C++14 A7-2-1" rule: "An expression with enum underlying type shall only have values
    // corresponding to the enumerators of the enumeration.".
    // Passing the enum value unchanged through the OS API
    // coverity[autosar_cpp14_a7_2_1_violation]
    const auto pulse_event = static_cast<std::uint8_t>(ctp->msg->pulse.value.sival_int);
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

void QnxDispatchEngine::RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    if (posix_receive_buffer_.size() < endpoint.max_receive_size)
    {
        posix_receive_buffer_.resize(endpoint.max_receive_size);
    }

    score::cpp::ignore = os_resources_.dispatch->select_attach(dispatch_pointer_,
                                                        nullptr,
                                                        endpoint.fd,
                                                        SELECT_FLAG_READ | SELECT_FLAG_REARM,
                                                        &EndpointFdSelectCallback,
                                                        &endpoint);
    posix_endpoint_list_.push_back(endpoint);
}

void QnxDispatchEngine::UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    score::cpp::ignore = posix_endpoint_list_.erase(posix_endpoint_list_.iterator_to(endpoint));
    UnselectEndpoint(endpoint);
}

void QnxDispatchEngine::UnselectEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    score::cpp::ignore = os_resources_.dispatch->select_detach(dispatch_pointer_, endpoint.fd);
    if (!endpoint.disconnect.empty())
    {
        endpoint.disconnect();
    }
}

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

score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::SendProtocolMessage(
    const std::int32_t fd,
    std::uint8_t code,
    const score::cpp::span<const std::uint8_t> message) noexcept
{
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

    auto result_expected = os_resources_.uio->writev(fd, io.data(), io.size());
    if (result_expected.has_value())
    {
        return {};
    }
    return score::cpp::make_unexpected(result_expected.error());
}

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
    return score::cpp::span<const std::uint8_t>{&posix_receive_buffer_[1],
                                         static_cast<score::cpp::span<const std::uint8_t>::size_type>(size - 1)};
}

void QnxDispatchEngine::SendPulseEvent(const PulseEvent pulse_event) noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = os_resources_.channel->MsgSendPulse(
        side_channel_coid_, -1, kEventPulseCode, static_cast<std::int32_t>(pulse_event));
}

void QnxDispatchEngine::ProcessPulseEvent(const std::uint8_t pulse_event) noexcept
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
    itimer.nsec = distance;
    itimer.interval_nsec = 0;
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

score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::StartServer(ResourceManagerServer& server,
                                                                   const QnxResourcePath& path) noexcept
{
    // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer
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
    constexpr mode_t attrMode{S_IFNAM | 0666};
    os_resources_.iofunc->iofunc_attr_init(&server.attr, attrMode, nullptr, nullptr);

    return {};
}

void QnxDispatchEngine::StopServer(ResourceManagerServer& server) noexcept
{
    if (server.resmgr_id_ != -1)
    {
        // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer
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

score::cpp::expected_blank<std::int32_t> QnxDispatchEngine::AttachConnection(resmgr_context_t* const ctp,
                                                                      io_open_t* const msg,
                                                                      ResourceManagerServer& server,
                                                                      ResourceManagerConnection& connection) noexcept
{
    QnxDispatchEngine& self = *server.engine_;
    auto& os_resources = self.GetOsResources();
    auto& iofunc = os_resources.iofunc;
    return iofunc->iofunc_ocb_attach(ctp, msg, &connection, &server.attr, nullptr);
}

std::int32_t QnxDispatchEngine::io_write(resmgr_context_t* const ctp,
                                         // coverity[autosar_cpp14_a9_5_1_violation]
                                         io_write_t* const msg,
                                         RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;
    auto& dispatch = self.GetOsResources().dispatch;

    // check if the write operation is allowed
    auto result = iofunc->iofunc_write_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    // check if we are requested to do just a plain write
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return ENOSYS;
    }

    // get the number of bytes we were asked to write, check that there are enough bytes in the message
    const std::size_t nbytes = _IO_WRITE_GET_NBYTES(msg);  // LCOV_EXCL_BR_LINE library macro with benign conditional
    const std::size_t nbytes_max = static_cast<std::size_t>(ctp->info.srcmsglen) - ctp->offset - sizeof(io_write_t);
    if (nbytes > nbytes_max)
    {
        return EBADMSG;
    }

    // take from engine and control nbytes
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) C API; don't pessimize
    std::array<std::uint8_t, 2048> buffer;

    auto msgget_expected = dispatch->resmgr_msgget(ctp, buffer.data(), nbytes, sizeof(msg->i));
    if (!msgget_expected.has_value())
    {
        return msgget_expected.error().GetOsDependentErrorCode();
    }

    const std::uint8_t code = buffer[0];
    const score::cpp::span<const std::uint8_t> message = {&buffer[1],
                                                   static_cast<score::cpp::span<std::uint8_t>::size_type>(nbytes - 1)};

    // TODO: close connection on false (once this functionality is demanded)
    score::cpp::ignore = OcbToConnection(ocb).ProcessInput(code, message);

    _IO_SET_WRITE_NBYTES(ctp, static_cast<std::int64_t>(nbytes));
    return EOK;
}

std::int32_t QnxDispatchEngine::io_read(resmgr_context_t* const ctp,
                                        // coverity[autosar_cpp14_a9_5_1_violation]
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

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return ENOSYS;
    }

    size_t nbytes = _IO_READ_GET_NBYTES(msg);
    if (nbytes == 0)
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
                                          RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    ResourceManagerConnection& connection = OcbToConnection(ocb);

    // 'trig' will tell iofunc_notify() which conditions are currently satisfied.
    std::int32_t trig = _NOTIFY_COND_OUTPUT; /* clients can always give us data */
    if (connection.HasSomethingToRead())
    {
        trig = _NOTIFY_COND_INPUT; /* we have some data available */
    }
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    return iofunc->iofunc_notify(ctp, msg, connection.notify_.data(), trig, nullptr, nullptr);
}

std::int32_t QnxDispatchEngine::io_close_ocb(resmgr_context_t* const ctp,
                                             void* const /*reserved*/,
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
