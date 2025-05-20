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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include "score/os/errno.h"

// #include <amp_assert.hpp>
#include <amp_optional.hpp>
#include <amp_stop_token.hpp>
#include <amp_string_view.hpp>

#include <chrono>
#include <functional>
#include <vector>

namespace bmw::mw::com::test
{

    /// \brief Helper class for cleaning up objects that must be destroyed when the test ends
    class ObjectCleanupGuard
    {
    public:
        void AddConsumerCheckpointControlGuard(
            SharedMemoryObjectCreator<CheckPointControl> &consumer_checkpoint_control_guard) noexcept;
        void AddProviderCheckpointControlGuard(
            SharedMemoryObjectCreator<CheckPointControl> &provider_checkpoint_control_guard) noexcept;
        void AddForkConsumerGuard(ChildProcessGuard &fork_consumer_pid_guard) noexcept;
        void AddForkProviderGuard(ChildProcessGuard &fork_provider_pid_guard) noexcept;
        bool CleanUp() noexcept;

    private:
        std::vector<std::reference_wrapper<SharedMemoryObjectCreator<CheckPointControl>>>
            consumer_checkpoint_control_guard_{};
        std::vector<std::reference_wrapper<SharedMemoryObjectCreator<CheckPointControl>>>
            provider_checkpoint_control_guard_{};
        std::vector<std::reference_wrapper<ChildProcessGuard>> fork_provider_pid_guard_{};
        std::vector<std::reference_wrapper<ChildProcessGuard>> fork_consumer_pid_guard_{};
    };

    void assertion_stdout_handler(const amp::handler_parameters &param) noexcept;

    /// \brief Helper function used in childs (consumer/provider) to receive and evaluate notifications from
    ///        parent/controller and to decide, whether the next checkpoint shall be reached or the consumer has to
    ///        finish/terminate.
    ///        Both: an explicit notification to terminate and an aborted (via stop-token) wait shall lead to
    ///        finish/terminate.
    /// \param check_point_control CheckPointControl instance used in communication with controller.
    /// \param test_stop_token test-wide/process-wide stop-token used for wait on notification.
    /// \return returns ProceedInstruction indicating whether child should proceed to the next checkpoint or
    /// finish/terminate.
    CheckPointControl::ProceedInstruction WaitForChildProceed(CheckPointControl &check_point_control,
                                                              amp::stop_token test_stop_token);

    os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateSharedCheckPointControl(
        amp::string_view message_prefix,
        amp::string_view shared_memory_file_path,
        amp::string_view check_point_owner_name) noexcept;

    os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateOrOpenSharedCheckPointControl(
        amp::string_view message_prefix,
        amp::string_view shared_memory_file_path,
        amp::string_view check_point_owner_name) noexcept;

    os::Result<SharedMemoryObjectCreator<CheckPointControl>> OpenSharedCheckPointControl(
        amp::string_view message_prefix,
        amp::string_view shared_memory_file_path) noexcept;

    amp::optional<ChildProcessGuard> ForkProcessAndRunInChildProcess(amp::string_view parent_message_prefix,
                                                                     amp::string_view child_message_prefix,
                                                                     std::function<void()> child_callable) noexcept;

    bool WaitForChildProcessToTerminate(amp::string_view message_prefix,
                                        bmw::mw::com::test::ChildProcessGuard &child_process_guard,
                                        std::chrono::milliseconds max_wait_time) noexcept;

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H
