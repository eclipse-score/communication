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
#include "score/message_passing/qnx_dispatch/dispatch_thread_runner.h"

#include <iostream>

namespace score::message_passing::detail
{

namespace
{

template <typename T>
// Suppress "AUTOSAR C++14 A9-5-1" rule finding: "Unions shall not be used.".
// coverity[autosar_cpp14_a9_5_1_violation: FALSE]. False positive: no any union in the following line. (Ticket-219101)
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: namespace-scoped function, not a global (Ticket-234468)
auto ValueOrTerminate(const score::cpp::expected<T, score::os::Error> expected, const std::string_view error_text) -> T
{
    if (!expected.has_value())
    {
        std::cerr << "DispatchThreadRunner: " << error_text << ": "
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
        std::cerr << "DispatchThreadRunner: " << error_text << ": "
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

DispatchThreadRunner::DispatchThreadRunner(score::os::Channel& channel,
                                           score::os::Dispatch& dispatch,
                                           score::os::Signal& signal) noexcept
    : channel_{channel},
      dispatch_{dispatch},
      signal_{signal},
      quit_flag_{false},
      thread_{},
      thread_mutex_{},
      dispatch_pointer_{nullptr},
      context_pointer_{},
      side_channel_coid_{}
{
}

void DispatchThreadRunner::StartPreInit() noexcept
{
    dispatch_pointer_ =  // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
        ValueOrTerminate(dispatch_.dispatch_create_channel(-1, 0U), "Unable to allocate dispatch handle");
    side_channel_coid_ = ValueOrTerminate(dispatch_.message_connect(dispatch_pointer_, MSG_FLAG_SIDE_CHANNEL),
                                          "Unable to create side channel");
    IfUnexpectedTerminate(dispatch_.pulse_attach(dispatch_pointer_, 0, kQuitPulseCode, &QuitPulseCallback, this),
                          "Unable to attach quit pulse code");
}

void DispatchThreadRunner::StartPostInit() noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    context_pointer_ =
        ValueOrTerminate(dispatch_.dispatch_context_alloc(dispatch_pointer_), "Unable to allocate context pointer");

    // Normally, during the application lifecycle initialization, LifeCycleManager blocks the SIGTERM on the main
    // thread and creates a separate thread that catches all the SIGTERM signals coming to the process. The other
    // threads created after that will inherit the sigmask of the main thread with SIGTERM blocked.
    // However, LifeCycleManager starts using Logging before it blocks SIGTERM on the main thread. When this happens,
    // Logging will initialize Message Passing, which will create the Message Passing background thread with a sigmask
    // inherited without SIGTERM being blocked yet.
    // Thus, we need to mask SIGTERM for this thread specifically, to let the LifeCycleManager desicated SIGTERM
    // thread do its job.
    sigset_t new_set;
    sigset_t old_set;
    // the signal functions below, used with the parameters below, can only return EOK
    score::cpp::ignore = signal_.SigEmptySet(new_set);
    score::cpp::ignore = signal_.AddTerminationSignal(new_set);
    // NOLINTNEXTLINE(score-banned-function) by design of LifeCycleManager.
    score::cpp::ignore = signal_.PthreadSigMask(SIG_BLOCK, new_set, old_set);
    {
        thread_ = std::thread([this]() noexcept {
            {
                std::lock_guard release(thread_mutex_);  // guarantees that this->thread_ is already assigned
            }
            RunOnThread();
        });
    }
    // NOLINTNEXTLINE(score-banned-function) by design of LifeCycleManager.
    score::cpp::ignore = signal_.PthreadSigMask(SIG_SETMASK, old_set);
}

DispatchThreadRunner::~DispatchThreadRunner() noexcept
{
    if (dispatch_pointer_ == nullptr)
    {
        return;
    }

    SendQuitPulse();
    thread_.join();

    score::cpp::ignore = channel_.ConnectDetach(side_channel_coid_);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = dispatch_.dispatch_destroy(dispatch_pointer_);
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    dispatch_.dispatch_context_free(context_pointer_);
}

void DispatchThreadRunner::RunOnThread() noexcept
{
    while (!quit_flag_)
    {
        // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
        if (dispatch_.dispatch_block(context_pointer_))
        {
            // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
            score::cpp::ignore = dispatch_.dispatch_handler(context_pointer_);
        }
    }
}

// Note 'C++14 A8-4-10':
// Suppress AUTOSAR C++14 A8-4-10 rule findigs: "A parameter shall be passed by reference if it can't be NULL."
// Justification: raw pointers are used in method signatures to maintain compatibility with the QNX API,
// which provides parameters as raw pointers.

// coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
// coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
// coverity[autosar_cpp14_a0_1_3_violation] false-positive: used as a pulse callback
int DispatchThreadRunner::QuitPulseCallback(message_context_t* /*ctp*/,
                                            int /*code*/,
                                            unsigned /*flags*/,
                                            // coverity[autosar_cpp14_a8_4_10_violation]: see "Note 'C++14 A8-4-10'"
                                            void* handle) noexcept
{
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // QNX API
    // coverity[autosar_cpp14_m5_2_8_violation]
    auto& self = *static_cast<DispatchThreadRunner*>(handle);
    self.quit_flag_ = true;
    return 0;
}

void DispatchThreadRunner::SendQuitPulse() noexcept
{
    // NOLINTNEXTLINE(score-banned-function) implementing FFI wrapper
    score::cpp::ignore = channel_.MsgSendPulse(side_channel_coid_, -1, kQuitPulseCode, 0);
}

}  // namespace score::message_passing::detail
