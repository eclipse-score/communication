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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_DISPATCH_THREAD_RUNNER_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_DISPATCH_THREAD_RUNNER_H

#include "score/os/qnx/channel.h"
#include "score/os/qnx/dispatch.h"
#include "score/os/utils/signal_impl.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace score::message_passing
{

class QnxDispatchEngine;

}  // namespace score::message_passing

namespace score::message_passing::detail
{

/// \brief Class encapsulating resources and logic needed to run QNX diapatch thread
/// \details Separate instances of this class are used to run dispatch loops responsible for client and server
///          functionality of QnxDispachEngine, to avoid deadlocks for client-server system topology with cycles.
class DispatchThreadRunner
{
  public:
    DispatchThreadRunner(score::os::Channel& channel,
                         score::os::Dispatch& dispatch,
                         score::os::Signal& signal) noexcept;
    ~DispatchThreadRunner() noexcept;

    DispatchThreadRunner(const DispatchThreadRunner&) = delete;
    DispatchThreadRunner(DispatchThreadRunner&&) = delete;
    DispatchThreadRunner& operator=(const DispatchThreadRunner&) = delete;
    DispatchThreadRunner& operator=(DispatchThreadRunner&&) = delete;

    bool IsMyThread(const std::thread::id id) const noexcept
    {
        return id == thread_.get_id();
    }

    static constexpr std::int32_t kQuitPulseCode = _PULSE_CODE_MINAVAIL;

    template <typename Init>
    void Start(Init init) noexcept
    {
        // block concurrent creation attempts and postpone RunOnThread() till we assign thread_
        std::lock_guard lock(thread_mutex_);
        if (dispatch_pointer_ != nullptr)
        {
            // already started; duplicate initialization not to be done
            return;
        }
        StartPreInit();
        init();
        StartPostInit();
    }

    dispatch_t* GetDispatchPointer() const noexcept
    {
        return dispatch_pointer_;
    }
    std::int32_t GetSideChannelCoid() const noexcept
    {
        return side_channel_coid_;
    }
    std::mutex& GetMutex() noexcept
    {
        return thread_mutex_;
    }

  private:
    void StartPreInit() noexcept;
    void StartPostInit() noexcept;
    void RunOnThread() noexcept;

    static int QuitPulseCallback(message_context_t* ctp, int code, unsigned flags, void* handle) noexcept;

    void SendQuitPulse() noexcept;

    score::os::Channel& channel_;
    score::os::Dispatch& dispatch_;
    score::os::Signal& signal_;

    bool quit_flag_;

    // NOLINTNEXTLINE(score-banned-type) TODO: wait for new clarification of CB #26380215 from QNX
    std::thread thread_;
    std::mutex thread_mutex_;

    dispatch_t* dispatch_pointer_;
    dispatch_context_t* context_pointer_;
    std::int32_t side_channel_coid_;

    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used."
    // QnxDispatchEngine acts like a module scope
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend QnxDispatchEngine;
};

}  // namespace score::message_passing::detail

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_DISPATCH_THREAD_RUNNER_H
