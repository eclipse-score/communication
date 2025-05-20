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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHECK_POINT_CONTROL_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHECK_POINT_CONTROL_H

#include "score/os/utils/interprocess/interprocess_notification.h"

#include "score/mw/com/test/common_test_resources/timeout_supervisor.h"

#include <amp_stop_token.hpp>
#include <amp_string_view.hpp>

#include <sys/types.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace bmw::mw::com::test
{

    class CheckPointControl;

    bool VerifyCheckpoint(const std::string &tag,
                          bool notification_happened,
                          CheckPointControl &checkpoint_control,
                          std::uint8_t expected_check_point);
    int WaitAndVerifyCheckPoint(const std::string &tag,
                                CheckPointControl &check_point_control,
                                std::uint8_t check_point,
                                amp::stop_token token,
                                std::chrono::milliseconds wait_duration);

    class CheckPointControl
    {
    public:
        enum class ProceedInstruction : std::uint8_t
        {
            INVALID = 0,
            STILL_PROCESSING,
            PROCEED_NEXT_CHECKPOINT,
            FINISH_ACTIONS
        };
        static constexpr std::uint8_t kInvalidCheckpointNumber{0U};

        explicit CheckPointControl(const amp::string_view check_point_owner_name) noexcept;

        /// \brief Function called by the Controller to trigger child process (Consumer or Producer) to proceed with its
        ///        actions until it reaches its next checkpoint (or an error)
        void ProceedToNextCheckpoint() noexcept;

        /// \brief Function called by the Controller to trigger child process (Consumer or Producer) to finish with its
        ///        actions and return/exit (with EXIT_SUCCESS)
        void FinishActions() noexcept;

        /// \brief Function called by child process (Consumer or Producer) to notify its parent/controller, that it has
        ///        reached the given checkpoint.
        /// \param checkpoint_number number of checkpoint, which has been reached.
        void CheckPointReached(std::uint8_t checkpoint_number) noexcept;

        /// \brief Function called by child process (Consumer or Producer) to notify its parent/controller, that it has
        ///        encountered an error (hindering it to reach the next checkpoint)
        void ErrorOccurred() noexcept;

        /// \brief Called by Controller to track state of its controlled childs (provider/consumer)
        /// \tparam Rep see std::chrono::duration
        /// \tparam Period see std::chrono::duration
        /// \param duration how long shall be waited
        /// \param external_stop_token this is the stop token for our stop_source, which is connected with the
        /// signal-handler
        ///                            of the controller app. I.e. if it gets killed it will be propagated with this
        ///                            stop_token, which is supervised by the wait logic.
        /// \param supervisor We currently need an additional supervisor, which supervises the given duration as the
        ///                   internally used InterprocessNotification doesn't yet support time-restricted waiting! See
        ///                   comment inside impl.
        /// \return true in case checkpoint or error notification has been received (in time), false if nothing has been
        ///         received. I.e. if waiting for notification was aborted by either timeout-supervision or global test
        ///         stop-token stop-request.
        template <class Rep, class Period>
        bool WaitForCheckpointReachedOrError(const std::chrono::duration<Rep, Period> &duration,
                                             amp::stop_token external_stop_token,
                                             TimeoutSupervisor &supervisor) noexcept;

        bool WaitForProceedOrFinishTrigger(amp::stop_token stop_token) noexcept;

        [[nodiscard]] bool HasErrorOccurred() const noexcept { return error_occurred_; }

        [[nodiscard]] uint8_t GetReachedCheckPoint() const noexcept { return checkpoint_reached_; }

        [[nodiscard]] ProceedInstruction GetProceedInstruction() const noexcept { return proceed_instruction_.load(); }

        amp::string_view GetOwnerName() const noexcept { return owner_name_; }

        void ResetCheckpointReachedNotifications() noexcept { checkpoint_reached_notifier_.reset(); }

        void ResetProceedNotifications() noexcept
        {
            proceed_instruction_ = ProceedInstruction::STILL_PROCESSING;
            proceed_notifier_.reset();
        }

    private:
        amp::string_view owner_name_;
        bmw::os::InterprocessNotification proceed_notifier_{};
        bmw::os::InterprocessNotification checkpoint_reached_notifier_{};

        std::atomic_uint8_t checkpoint_reached_{kInvalidCheckpointNumber};
        std::atomic_bool error_occurred_{false};

        std::atomic<ProceedInstruction> proceed_instruction_{ProceedInstruction::STILL_PROCESSING};
    };

    template <class Rep, class Period>
    bool CheckPointControl::WaitForCheckpointReachedOrError(const std::chrono::duration<Rep, Period> &duration,
                                                            amp::stop_token external_stop_token,
                                                            TimeoutSupervisor &supervisor) noexcept
    {
        // If our InterprocessConditionVariable would already have support for wait_until, this whole function would
        // shrink down to this line:
        // return checkpoint_reached_notifier_.waitForWithAbort(duration, stop_token);

        amp::stop_source supervision_stop_source{};
        amp::stop_callback stop_callback{external_stop_token,
                                         [&supervision_stop_source]() noexcept
                                         { supervision_stop_source.request_stop(); }};

        const auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        auto supervision_timeout_callback = [&supervision_stop_source]() noexcept
        {
            supervision_stop_source.request_stop();
        };
        supervisor.StartSupervision(duration_in_milliseconds, std::move(supervision_timeout_callback));
        auto notification_received = checkpoint_reached_notifier_.waitWithAbort(supervision_stop_source.get_token());
        supervisor.StopSupervision();

        return notification_received;
    }

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHECK_POINT_CONTROL_H
