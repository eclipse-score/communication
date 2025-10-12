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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H

#include <score/callback.hpp>
#include <score/memory.hpp>
#include <score/span.hpp>
#include <score/vector.hpp>

#include "score/message_passing/i_shared_resource_engine.h"
#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"
#include "score/message_passing/timed_command_queue.h"
#include "score/os/fcntl.h"
#include "score/os/qnx/channel.h"
#include "score/os/qnx/dispatch.h"
#include "score/os/qnx/iofunc.h"
#include "score/os/qnx/timer_impl.h"
#include "score/os/sys_uio.h"
#include "score/os/unistd.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace score::message_passing
{

/// \brief Class encapsulating resources needed for QNX Dispatch Client/Server implementation
/// \details The class provides access to the OSAL resource objects, memory resource, background thread with poll loop,
///          and timer queue. It also provides an implementation of a simple message exchange transport protocol
///          over a connected socket.
///          The class is supposed to be shared via std::shared_ptr between its consumers (client and server factories,
///          server objects, client and server connections).
///          One or more instances of this class, with separate background threads and potentially separate memory
///          resources, can co-exist in the same process, if needed.
class QnxDispatchEngine final : public ISharedResourceEngine
{
  public:
    struct OsResources
    {
        score::cpp::pmr::unique_ptr<score::os::Channel> channel{};
        score::cpp::pmr::unique_ptr<score::os::Dispatch> dispatch{};
        score::cpp::pmr::unique_ptr<score::os::Fcntl> fcntl{};
        score::cpp::pmr::unique_ptr<score::os::IoFunc> iofunc{};
        score::cpp::pmr::unique_ptr<score::os::qnx::Timer> timer{};
        score::cpp::pmr::unique_ptr<score::os::SysUio> uio{};
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
    };

    using QnxResourcePath = detail::QnxResourcePath;

    // we don't need "extended" attributes for us, but the OSAL is written in such a way...
    // NOLINTNEXTLINE(score-struct-usage-compliance): controlled downcasting from a reference to plain C API datatype
    class ResourceManagerServer : private extended_dev_attr_t
    {
      public:
        explicit ResourceManagerServer(std::shared_ptr<QnxDispatchEngine> engine) noexcept
            : extended_dev_attr_t{}, engine_{std::move(engine)}, resmgr_id_{-1}
        {
        }

        virtual ~ResourceManagerServer() = default;

        ResourceManagerServer(const ResourceManagerServer&) = delete;
        ResourceManagerServer(ResourceManagerServer&&) = delete;
        ResourceManagerServer& operator=(const ResourceManagerServer&) = delete;
        ResourceManagerServer& operator=(ResourceManagerServer&&) = delete;

        score::cpp::expected_blank<score::os::Error> Start(const QnxResourcePath& path) noexcept
        {
            return engine_->StartServer(*this, path);
        }

        void Stop() noexcept
        {
            engine_->StopServer(*this);
        }

      protected:
        // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used."
        // QnxDispatchEngine acts like a module scope
        // coverity[autosar_cpp14_a11_3_1_violation]
        friend QnxDispatchEngine;

        // TODO: isolate resmgr stuff
        virtual std::int32_t ProcessConnect(resmgr_context_t* const ctp, io_open_t* const msg) noexcept = 0;

        std::shared_ptr<QnxDispatchEngine> engine_;
        std::int32_t resmgr_id_;
    };

    // NOLINTNEXTLINE(score-struct-usage-compliance): controlled downcasting from a reference to plain C API datatype
    class ResourceManagerConnection : private iofunc_ocb_t
    {
      public:
        ResourceManagerConnection() noexcept : iofunc_ocb_t{}, notify_{}
        {
            // Suppress "AUTOSAR C++14 M5-0-15" rule finding: "Array indexing shall be the only form of pointer
            // arithmetic".
            // Suppress "AUTOSAR C++14 A4-10-1" rule finding: "Only nullptr literal shall be used as the
            // null-pointer-constant.
            // OS API
            // coverity[autosar_cpp14_m5_0_15_violation]
            // coverity[autosar_cpp14_a4_10_1_violation]
            IOFUNC_NOTIFY_INIT(notify_.data());
        }

        virtual ~ResourceManagerConnection() = default;

        ResourceManagerConnection(const ResourceManagerConnection&) = delete;
        ResourceManagerConnection(ResourceManagerConnection&&) = delete;
        ResourceManagerConnection& operator=(const ResourceManagerConnection&) = delete;
        ResourceManagerConnection& operator=(ResourceManagerConnection&&) = delete;

        ResourceManagerServer& GetServer()
        {
            return QnxDispatchEngine::OcbToServer(this);
        }

      protected:
        std::array<iofunc_notify_t, 3> notify_;

      private:
        // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used."
        // Only QnxDispatchEngine is allowed to trigger these callbacks
        // coverity[autosar_cpp14_a11_3_1_violation]
        friend QnxDispatchEngine;

        virtual bool ProcessInput(const std::uint8_t code, const score::cpp::span<const std::uint8_t> message) noexcept = 0;
        virtual void ProcessDisconnect() noexcept = 0;
        virtual bool HasSomethingToRead() noexcept = 0;

        // TODO: isolate resmgr stuff
        virtual std::int32_t ProcessReadRequest(resmgr_context_t* const ctp) noexcept = 0;
    };

    QnxDispatchEngine(score::cpp::pmr::memory_resource* memory_resource, OsResources os_resources) noexcept;
    explicit QnxDispatchEngine(score::cpp::pmr::memory_resource* memory_resource) noexcept
        : QnxDispatchEngine(memory_resource, GetDefaultOsResources(memory_resource))
    {
    }
    ~QnxDispatchEngine() noexcept override;

    QnxDispatchEngine(const QnxDispatchEngine&) = delete;
    QnxDispatchEngine(QnxDispatchEngine&&) = delete;
    QnxDispatchEngine& operator=(const QnxDispatchEngine&) = delete;
    QnxDispatchEngine& operator=(QnxDispatchEngine&&) = delete;

    static OsResources GetDefaultOsResources(score::cpp::pmr::memory_resource* const memory_resource) noexcept
    {
        return {score::os::Channel::Default(memory_resource),
                score::os::Dispatch::Default(memory_resource),
                score::os::Fcntl::Default(memory_resource),
                score::os::IoFunc::Default(memory_resource),
                score::cpp::pmr::make_unique<score::os::qnx::TimerImpl>(memory_resource),
                score::os::SysUio::Default(memory_resource),
                score::os::Unistd::Default(memory_resource)};
    }

    score::cpp::pmr::memory_resource* GetMemoryResource() noexcept override
    {
        return memory_resource_;
    }
    const OsResources& GetOsResources() noexcept
    {
        return os_resources_;
    }

    using FinalizeOwnerCallback = score::cpp::callback<void() /* noexcept */>;

    score::cpp::expected<std::int32_t, score::os::Error> TryOpenClientConnection(std::string_view identifier) noexcept override;

    void CloseClientConnection(std::int32_t client_fd) noexcept override;

    void RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;
    void UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;

    void EnqueueCommand(CommandQueueEntry& entry,
                        const TimePoint until,
                        CommandCallback callback,
                        const void* const owner) noexcept override;

    // this call is blocking
    void CleanUpOwner(const void* const owner) noexcept override;

    score::cpp::expected_blank<score::os::Error> SendProtocolMessage(
        const std::int32_t fd,
        std::uint8_t code,
        const score::cpp::span<const std::uint8_t> message) noexcept override;
    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ReceiveProtocolMessage(
        const std::int32_t fd,
        std::uint8_t& code) noexcept override;

    static score::cpp::expected_blank<std::int32_t> AttachConnection(resmgr_context_t* const ctp,
                                                              io_open_t* const msg,
                                                              ResourceManagerServer& server,
                                                              ResourceManagerConnection& connection) noexcept;

    bool IsOnCallbackThread() const noexcept override
    {
        return std::this_thread::get_id() == thread_.get_id();
    }

  private:
    enum class PulseEvent : std::uint8_t
    {
        QUIT,
        TIMER
    };

    void SendPulseEvent(const PulseEvent pulse_event) noexcept;
    void ProcessPulseEvent(const std::uint8_t pulse_event) noexcept;
    void ProcessCleanup(const void* const owner) noexcept;
    void RunOnThread() noexcept;

    static int EndpointFdSelectCallback(select_context_t* ctp, int fd, unsigned flags, void* handle) noexcept;
    static int TimerPulseCallback(message_context_t* ctp, int code, unsigned flags, void* handle) noexcept;
    static int EventPulseCallback(message_context_t* ctp, int code, unsigned flags, void* handle) noexcept;

    void UnselectEndpoint(PosixEndpointEntry& endpoint) noexcept;
    void ProcessTimerQueue() noexcept;

    void SetupResourceManagerCallbacks() noexcept;
    score::cpp::expected_blank<score::os::Error> StartServer(ResourceManagerServer& server,
                                                    const QnxResourcePath& path) noexcept;
    void StopServer(ResourceManagerServer& server) noexcept;

    static ResourceManagerConnection& OcbToConnection(RESMGR_OCB_T* const ocb)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) by API design
        return static_cast<ResourceManagerConnection&>(*ocb);
    }

    static ResourceManagerServer& ResmgrHandleToServer(RESMGR_HANDLE_T* const handle)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) by API design
        return static_cast<ResourceManagerServer&>(*handle);
    }

    static ResourceManagerServer& OcbToServer(RESMGR_OCB_T* const ocb)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) by API design
        return *static_cast<ResourceManagerServer*>(ocb->attr);
    }

    static std::int32_t io_open(resmgr_context_t* const ctp,
                                io_open_t* const msg,
                                RESMGR_HANDLE_T* const handle,
                                void* const extra) noexcept;

    static std::int32_t io_close_ocb(resmgr_context_t* const ctp,
                                     void* const reserved,
                                     RESMGR_OCB_T* const ocb) noexcept;

    static std::int32_t io_write(resmgr_context_t* const ctp, io_write_t* const msg, RESMGR_OCB_T* const ocb) noexcept;
    static std::int32_t io_read(resmgr_context_t* const ctp, io_read_t* const msg, RESMGR_OCB_T* const ocb) noexcept;
    static std::int32_t io_notify(resmgr_context_t* const ctp,
                                  io_notify_t* const msg,
                                  RESMGR_OCB_T* const ocb) noexcept;

    score::cpp::pmr::memory_resource* const memory_resource_;
    OsResources os_resources_;

    bool quit_flag_;
    // NOLINTNEXTLINE(score-banned-type) TODO: wait for new clarification of CB #26380215 from QNX
    std::thread thread_;
    std::mutex thread_mutex_;
    std::condition_variable thread_condition_;

    detail::TimedCommandQueue timer_queue_;
    score::containers::intrusive_list<PosixEndpointEntry> posix_endpoint_list_;
    score::cpp::pmr::vector<std::uint8_t> posix_receive_buffer_;

    dispatch_t* dispatch_pointer_;
    dispatch_context_t* context_pointer_;
    std::int32_t side_channel_coid_;
    timer_t timer_id_;
    std::mutex attach_mutex_;

    resmgr_connect_funcs_t connect_funcs_;
    resmgr_io_funcs_t io_funcs_;
};

}  // namespace score::message_passing

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H
