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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RESMGR_RECEIVER_TRAITS_H
#define SCORE_MW_COM_MESSAGE_PASSING_RESMGR_RECEIVER_TRAITS_H

#include "score/mw/com/message_passing/message.h"

#include "score/memory/pmr_ring_buffer.h"

#include "score/os/qnx/channel.h"
#include "score/os/qnx/dispatch.h"
#include "score/os/qnx/iofunc.h"
#include "score/os/unistd.h"

#include <score/expected.hpp>
#include <score/utility.hpp>
#include <score/vector.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace score
{
namespace mw
{
namespace com
{
namespace message_passing
{

class ResmgrReceiverTraits
{
  private:
    struct ResmgrReceiverState;

  public:
    // Suppress "AUTOSAR C++14 A0-1-1" rule findings. This rule states: "A project shall not contain instances of
    // non-volatile variables being given values that are not subsequently used." False positive, variable is used.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr std::size_t kConcurrency{2U};
    using file_descriptor_type = ResmgrReceiverState*;
    static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR{nullptr};

    struct OsResources
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Dispatch> dispatch{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Channel> channel{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::IoFunc> iofunc{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
    };

    static OsResources GetDefaultOSResources(score::cpp::pmr::memory_resource* memory_resource) noexcept
    {
        return {score::os::Dispatch::Default(memory_resource),
                score::os::Channel::Default(memory_resource),
                score::os::IoFunc::Default(memory_resource),
                score::os::Unistd::Default(memory_resource)};
    }

    using FileDescriptorResourcesType = OsResources;

    static score::cpp::expected<file_descriptor_type, score::os::Error> open_receiver(
        const std::string_view identifier,
        const score::cpp::pmr::vector<uid_t>& allowed_uids,
        const std::int32_t max_number_message_in_queue,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static void close_receiver(const file_descriptor_type file_descriptor,
                               const std::string_view /*identifier*/,
                               const FileDescriptorResourcesType& os_resources) noexcept;

    static void stop_receive(const file_descriptor_type file_descriptor,
                             const FileDescriptorResourcesType& os_resources) noexcept;

    template <typename ShortMessageProcessor, typename MediumMessageProcessor>
    // Suppress "AUTOSAR C++14 A8-4-10", The rule states: "A parameter shall be passed by reference if it can’t be
    // NULL". Used type from system API.
    // Suppress "AUTOSAR C++14 A15-5-3", The rule states: "The std::terminate() function shall not be called
    // implicitly." The argument for the thread parameter only comes from a ChannelTraits::kConcurrency bounded loop in
    // Receiver<ChannelTraits>::StartListening(), hence no exception will be thrown.
    // coverity[autosar_cpp14_a8_4_10_violation]
    // coverity[autosar_cpp14_a15_5_3_violation]
    static score::cpp::expected<bool, score::os::Error> receive_next(const file_descriptor_type file_descriptor,
                                                            const std::size_t thread,
                                                            ShortMessageProcessor fShort,
                                                            MediumMessageProcessor fMedium,
                                                            const FileDescriptorResourcesType&) noexcept
    {
        ResmgrReceiverState& receiver_state = *file_descriptor;
        // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
        // QNX union.
        // The argument for the thread parameter only comes from a ChannelTraits::kConcurrency bounded loop in
        // Receiver<ChannelTraits>::StartListening().
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): See above
        // coverity[autosar_cpp14_a9_5_1_violation]
        dispatch_context_t* const context_pointer = receiver_state.context_pointers_[thread];
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index): See above

        // pre-initialize our context_data
        ResmgrContextData context_data{false, receiver_state};
        context_pointer->resmgr_context.extra->data = &context_data;

        auto& dispatch = context_data.receiver_state.os_resources_.dispatch;

        // tell the framework to wait for the message
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        const auto block_result = dispatch->dispatch_block(context_pointer);
        if (!block_result.has_value())  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
        {
            // shall not be a critical error; skip the dispatch_handler() but allow the next iteration
            return true;
        }

        // tell the framework to process the incoming message (and maybe to call one of our callbacks)
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        const auto handler_result = dispatch->dispatch_handler(context_pointer);

        if (!handler_result.has_value())  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
        {
            // shall not be a critical error, but there were no valid message to handle
            return true;
        }

        if (context_data.to_terminate)  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
        {
            // we were asked to stop; do it in this thread
            return false;
        }

        {
            // Suppress "AUTOSAR C++14 M0-1-3" and "AUTOSAR C++14 M0-1-9" rule violations. The rule states
            // "A project shall not contain unused variables." and "There shall be no dead code.", respectively.
            // This lock guard ensures the mutex is locked for the duration of this scope and is not dead code.
            // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
            // coverity[autosar_cpp14_m0_1_3_violation : FALSE]
            std::lock_guard<std::mutex> guard(receiver_state.message_queue_mutex_);
            if (receiver_state.message_queue_.empty())  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
            {
                // nothing to process yet
                return true;
            }
            if (receiver_state.message_queue_owned_)  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
            {
                // will be processed in another thread
                return true;
            }
            // Only one thread, SWS_CM_00182 (SCR-5898338) explicitly fulfilled for Resmgr implementation
            receiver_state.message_queue_owned_ = true;
        }

        while (true)
        {
            MessageData message_data{};
            {
                // coverity[autosar_cpp14_m0_1_9_violation : FALSE]
                // coverity[autosar_cpp14_m0_1_3_violation : FALSE]
                std::lock_guard<std::mutex> guard(receiver_state.message_queue_mutex_);
                if (receiver_state.message_queue_.empty())  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
                {
                    // nothing to process already
                    receiver_state.message_queue_owned_ = false;
                    return true;
                }
                message_data = receiver_state.message_queue_.front();
                receiver_state.message_queue_.pop_front();
            }

            if (message_data.type_ == MessageType::kShortMessage)  // LCOV_EXCL_BR_LINE False positive: Ticket-181446
            {
                // The unions are strictly prohibited by AUTOSAR Rule A9-5-1. But, in this case what we need to do
                // is serialize two type alternatives in most efficient way possible. The union fits this purpose
                // perfectly as long as we use Tagged union in proper manner.
                // Also the union is implementation detail of outer class, so cannot be potentially misused externally.
                // The Tagged union is implemented in accordence with CppCoreGuidelines to minimize the risk.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Serialization optimization necessity
                fShort(message_data.payload_.short_);
            }
            else
            {
                // The unions are strictly prohibited by AUTOSAR Rule A9-5-1. But, in this case what we need to do
                // is serialize two type alternatives in most efficient way possible. The union fits this purpose
                // perfectly as long as we use Tagged union in proper manner.
                // Also the union is implementation detail of outer class, so cannot be potentially misused externally.
                // The Tagged union is implemented in accordence with CppCoreGuidelines to minimize the risk.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Serialization optimization necessity
                fMedium(message_data.payload_.medium_);
            }
        }
    }

  private:
    // coverity[autosar_cpp14_a4_5_1_violation]
    static constexpr std::uint16_t kPrivateMessageTypeFirst{_IO_MAX + 1};
    // Suppress "AUTOSAR C++14 A0-1-1" rule findings. This rule states: "A project shall not contain instances of
    // non-volatile variables being given values that are not subsequently used." False positive, variable is used in
    // resmgr_receiver_traits.cpp file.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr std::uint16_t kPrivateMessageTypeLast{kPrivateMessageTypeFirst};
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr std::uint16_t kPrivateMessageStop{kPrivateMessageTypeFirst};

    enum class MessageType : std::size_t
    {
        kNone,
        kShortMessage,
        kMediumMessage
    };

    // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
    // Exception tagged union.
    // coverity[autosar_cpp14_a9_5_1_violation]
    union MessagePayload
    {
        // coverity[autosar_cpp14_m11_0_1_violation]
        ShortMessage short_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        MediumMessage medium_;
    };

    static_assert(std::is_trivially_copyable<MessagePayload>::value, "MessagePayload union must be TriviallyCopyable");

    // The unions are strictly prohibited by AUTOSAR Rule A9-5-1. But, in this case what we need to do
    // is serialize two type alternatives in most efficient way possible. The union fits this purpose
    // perfectly as long as we use Tagged union in proper manner.
    // Also the union is implementation detail of outer class, so cannot be potentially misused externally.
    // The Tagged union is implemented in accordence with CppCoreGuidelines to minimize the risk.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Serialization optimization necessity
    struct MessageData
    {
        // coverity[autosar_cpp14_m11_0_1_violation]
        MessageType type_{MessageType::kNone};
        // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
        // Exception tagged union.
        // coverity[autosar_cpp14_a9_5_1_violation]
        // coverity[autosar_cpp14_m11_0_1_violation]
        MessagePayload payload_{};
    };

    static_assert(std::is_trivially_copyable<MessageData>::value, "MessageData must be TriviallyCopyable");

    // common attributes for all services of the process
    // AUTOSAR Rule A11-0-1 prohibits non-POD structs as well as Rule A11-0-2 prohibits special member
    // functions (such as constructor) for structs. But, in this case struct is implementation detail
    // of outer class and this violation have no side effect. Instead, a semantic struct is much better
    // choice to implement Setup info as data object, as long as there are only member access operations.
    // WARNING: ResmgrSetup is struct but it is neither PODType nor TrivialType nor TriviallyCopyable!
    // Note that it is still StandardLayoutType.
    // NOLINTBEGIN(score-struct-usage-compliance): Constructor implements data initialization logic
    // coverity[autosar_cpp14_a11_0_2_violation]
    struct ResmgrSetup
    // NOLINTEND(score-struct-usage-compliance): Constructor implements data initialization logic
    {
        // coverity[autosar_cpp14_m11_0_1_violation]
        resmgr_attr_t resmgr_attr_{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        resmgr_connect_funcs_t connect_funcs_{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        resmgr_io_funcs_t io_funcs_{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        extended_dev_attr_t extended_attr_{};
        // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
        // QNX union.
        // coverity[autosar_cpp14_a9_5_1_violation]
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::int32_t (*open_default)(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T* handle, void* extra){
            nullptr};

        explicit ResmgrSetup(const FileDescriptorResourcesType& os_resources) noexcept;

        static ResmgrSetup& instance(const FileDescriptorResourcesType& os_resources) noexcept;
    };

    static_assert(std::is_standard_layout<ResmgrSetup>::value, "ResmgrSetup type must be StandardLayoutType");

    // AUTOSAR Rule A11-0-1 prohibits non-POD structs as well as Rule A11-0-2 prohibits special member
    // functions (such as constructor) for structs. But, in this case struct is implementation detail
    // of outer class and this violation have no side effect. Instead, a semantic struct is much better
    // choice to implement State as data object, as long as there are only member access operations.
    // WARNING: ResmgrReceiverState is struct but it is neither PODType nor StandardLayoutType
    // nor TrivialType nor TriviallyCopyable!
    // NOLINTBEGIN(score-struct-usage-compliance): Constructor is used for struct members initialization
    // coverity[autosar_cpp14_a11_0_2_violation]
    struct ResmgrReceiverState
    // NOLINTEND(score-struct-usage-compliance): Constructor implements data initialization logic
    {
        // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
        // QNX union.
        // coverity[autosar_cpp14_m11_0_1_violation]
        // coverity[autosar_cpp14_a9_5_1_violation]
        std::array<dispatch_context_t*, kConcurrency> context_pointers_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const std::int32_t side_channel_coid_;

        // coverity[autosar_cpp14_m11_0_1_violation]
        std::mutex message_queue_mutex_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::memory::PmrRingBuffer<MessageData> message_queue_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        bool message_queue_owned_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const score::cpp::pmr::vector<uid_t>& allowed_uids_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        const FileDescriptorResourcesType& os_resources_;

        // LCOV_EXCL_START False positive: Ticket-184253
        ResmgrReceiverState(const std::size_t max_message_queue_size,
                            const std::int32_t side_channel_coid,
                            const score::cpp::pmr::vector<uid_t>& allowed_uids,
                            const FileDescriptorResourcesType& os_resources)
            : context_pointers_(),
              side_channel_coid_(side_channel_coid),
              // LCOV_EXCL_START (Tool incorrectly marks this line as not covered. However, the lines before and after
              // are covered so this is clearly an error by the tool. Suppression can be removed when bug is fixed in
              // Ticket-184253).
              message_queue_(max_message_queue_size, allowed_uids.get_allocator()),
              // LCOV_EXCL_STOP
              message_queue_owned_(false),
              allowed_uids_(allowed_uids),
              os_resources_(os_resources)
        {
        }
        // LCOV_EXCL_STOP
    };

    struct ResmgrContextData
    {
        // coverity[autosar_cpp14_m11_0_1_violation]
        bool to_terminate;
        // coverity[autosar_cpp14_m11_0_1_violation]
        ResmgrReceiverState& receiver_state;
    };

    // coverity[autosar_cpp14_a8_4_10_violation]
    static ResmgrContextData& GetContextData(const resmgr_context_t* const ctp)
    {
        // Suppress "AUTOSAR C++14 M5-2-8", The rule states: "An object with integer type or pointer to void type
        // shall not be converted to an object with pointer type."
        // QNX datatype.
        // coverity[autosar_cpp14_m5_2_8_violation]
        return *static_cast<ResmgrContextData*>(ctp->extra->data);
    }

    static score::cpp::expected<dispatch_t*, score::os::Error> CreateAndAttachChannel(
        const std::string_view identifier,
        ResmgrSetup& setup,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static score::cpp::expected<std::int32_t, score::os::Error> CreateTerminationMessageSideChannel(
        dispatch_t* const dispatch_pointer,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static void Stop(const std::int32_t side_channel_coid, const FileDescriptorResourcesType& os_resources);

    static bool IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept;

    static std::int32_t io_open(resmgr_context_t* const ctp,
                                // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
                                // QNX union.
                                // coverity[autosar_cpp14_a9_5_1_violation]
                                io_open_t* const msg,
                                RESMGR_HANDLE_T* const handle,
                                void* const extra) noexcept;

    static std::int32_t CheckWritePreconditions(resmgr_context_t* const ctp,
                                                // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not
                                                // be used." This is a QNX union.
                                                // coverity[autosar_cpp14_a9_5_1_violation]
                                                io_write_t* const msg,
                                                RESMGR_OCB_T* const ocb) noexcept;

    static std::int32_t GetMessageData(resmgr_context_t* const ctp,
                                       // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
                                       // QNX union.
                                       // coverity[autosar_cpp14_a9_5_1_violation]
                                       io_write_t* const msg,
                                       const std::size_t nbytes,
                                       MessageData& message_data) noexcept;

    // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
    // QNX union.
    // coverity[autosar_cpp14_a9_5_1_violation]
    static std::int32_t io_write(resmgr_context_t* const ctp, io_write_t* const msg, RESMGR_OCB_T* const ocb) noexcept;

    static std::int32_t private_message_handler(message_context_t* const ctp,
                                                const std::int32_t /*code*/,
                                                const std::uint32_t /*flags*/,
                                                void* const /*handle*/) noexcept;
};

}  // namespace message_passing
}  // namespace com
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RESMGR_RECEIVER_TRAITS_H
