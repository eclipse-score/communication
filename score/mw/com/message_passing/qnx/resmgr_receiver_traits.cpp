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
#include "score/mw/com/message_passing/qnx/resmgr_receiver_traits.h"

#include "score/os/unistd.h"
#include "score/mw/com/message_passing/qnx/resmgr_traits_common.h"

#include <score/assert.hpp>
#include <algorithm>

namespace score
{
namespace mw
{
namespace com
{
namespace message_passing
{

namespace
{

constexpr void* kNoReply{nullptr};
constexpr std::size_t kNoSize{0U};

}  // namespace

score::cpp::expected<dispatch_t*, score::os::Error> ResmgrReceiverTraits::CreateAndAttachChannel(
    const std::string_view identifier,
    ResmgrSetup& setup,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    const auto dpp_expected =
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        os_resources.dispatch->dispatch_create_channel(-1, static_cast<std::uint32_t>(DISPATCH_FLAG_NOLOCK));
    if (!dpp_expected.has_value())
    {
        return score::cpp::make_unexpected(dpp_expected.error());
    }
    dispatch_t* const dispatch_pointer = dpp_expected.value();

    const QnxResourcePath path{identifier};

    const auto id_expected = os_resources.dispatch->resmgr_attach(dispatch_pointer,
                                                                  &setup.resmgr_attr_,
                                                                  path.c_str(),
                                                                  _FTYPE_ANY,
                                                                  static_cast<std::uint32_t>(_RESMGR_FLAG_SELF),
                                                                  &setup.connect_funcs_,
                                                                  &setup.io_funcs_,
                                                                  &setup.extended_attr_);
    if (!id_expected.has_value())
    {
        return score::cpp::make_unexpected(id_expected.error());
    }

    return dpp_expected;
}

score::cpp::expected<std::int32_t, score::os::Error> ResmgrReceiverTraits::CreateTerminationMessageSideChannel(
    dispatch_t* const dispatch_pointer,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    // attach a private message handler to process service termination messages
    constexpr _message_attr* noAttr{nullptr};
    constexpr void* noHandle{nullptr};
    const auto attach_expected =
        os_resources.dispatch->message_attach(dispatch_pointer,
                                              noAttr,
                                              static_cast<std::int32_t>(kPrivateMessageTypeFirst),
                                              static_cast<std::int32_t>(kPrivateMessageTypeLast),
                                              &private_message_handler,
                                              noHandle);
    if (!attach_expected.has_value())
    {
        return score::cpp::make_unexpected(attach_expected.error());
    }

    // create a client connection to this channel
    const auto coid_expected = os_resources.dispatch->message_connect(dispatch_pointer, MSG_FLAG_SIDE_CHANNEL);
    return coid_expected;
}

score::cpp::expected<ResmgrReceiverTraits::file_descriptor_type, score::os::Error> ResmgrReceiverTraits::open_receiver(
    const std::string_view identifier,
    const score::cpp::pmr::vector<uid_t>& allowed_uids,
    const std::int32_t max_number_message_in_queue,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");

    ResmgrSetup& setup = ResmgrSetup::instance(os_resources);

    const auto dpp_expected = CreateAndAttachChannel(identifier, setup, os_resources);
    if (!dpp_expected.has_value())
    {
        return score::cpp::make_unexpected(dpp_expected.error());
    }
    dispatch_t* const dispatch_pointer = dpp_expected.value();

    const auto coid_expected = CreateTerminationMessageSideChannel(dispatch_pointer, os_resources);
    if (!coid_expected.has_value())
    {
        return score::cpp::make_unexpected(coid_expected.error());
    }
    const std::int32_t side_channel_coid = coid_expected.value();
    using Alloc = score::cpp::pmr::polymorphic_allocator<ResmgrReceiverState>;
    Alloc allocator{allowed_uids.get_allocator()};
    std::allocator_traits<Alloc>::pointer state_pointer = std::allocator_traits<Alloc>::allocate(allocator, 1U);
    std::allocator_traits<Alloc>::construct(
        allocator, state_pointer, max_number_message_in_queue, side_channel_coid, allowed_uids, os_resources);

    // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
    // QNX union.
    // coverity[autosar_cpp14_a9_5_1_violation]
    for (auto*& state_context_pointer : state_pointer->context_pointers_)
    {
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        const auto ctp_expected = os_resources.dispatch->dispatch_context_alloc(dispatch_pointer);
        if (!ctp_expected.has_value())
        {
            return score::cpp::make_unexpected(ctp_expected.error());
        }
        // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
        // QNX union.
        // coverity[autosar_cpp14_a9_5_1_violation]
        dispatch_context_t* const context_pointer = ctp_expected.value();
        state_context_pointer = context_pointer;
    }
    return state_pointer;
}

void ResmgrReceiverTraits::close_receiver(const ResmgrReceiverTraits::file_descriptor_type file_descriptor,
                                          const std::string_view /*identifier*/,
                                          const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");

    if (file_descriptor == ResmgrReceiverTraits::INVALID_FILE_DESCRIPTOR)
    {
        return;
    }

    // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
    // QNX union.
    // coverity[autosar_cpp14_a9_5_1_violation]
    dispatch_context_t* const first_context_pointer = file_descriptor->context_pointers_[0];
    const std::int32_t side_channel_coid = file_descriptor->side_channel_coid_;

    dispatch_t* const dispatch_pointer = first_context_pointer->resmgr_context.dpp;
    const std::int32_t id = first_context_pointer->resmgr_context.id;

    score::cpp::ignore = os_resources.channel->ConnectDetach(side_channel_coid);
    score::cpp::ignore =
        os_resources.dispatch->resmgr_detach(dispatch_pointer, id, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE));
    // This function in a banned list, however according to the requirement
    // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
    // NOLINTNEXTLINE(score-banned-function): See above
    score::cpp::ignore = os_resources.dispatch->dispatch_destroy(dispatch_pointer);
    // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be used."
    // QNX union.
    // coverity[autosar_cpp14_a9_5_1_violation]
    for (auto* context_pointer : file_descriptor->context_pointers_)
    {
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        os_resources.dispatch->dispatch_context_free(context_pointer);
    }
    using Alloc = score::cpp::pmr::polymorphic_allocator<ResmgrReceiverState>;
    Alloc allocator{file_descriptor->allowed_uids_.get_allocator()};
    std::allocator_traits<Alloc>::destroy(allocator, file_descriptor);
    std::allocator_traits<Alloc>::deallocate(allocator, file_descriptor, 1U);
}

// Suppress "AUTOSAR C++14 A8-4-10", The rule states: "A parameter shall be passed by reference if it can’t be NULL".
// Used type from system API.
// coverity[autosar_cpp14_a8_4_10_violation]
void ResmgrReceiverTraits::stop_receive(const ResmgrReceiverTraits::file_descriptor_type file_descriptor,
                                        const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");

    const std::int32_t side_channel_coid = file_descriptor->side_channel_coid_;
    Stop(side_channel_coid, os_resources);
}

ResmgrReceiverTraits::ResmgrSetup::ResmgrSetup(const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    resmgr_attr_.nparts_max = 1U;
    resmgr_attr_.msg_max_size = 1024U;

    // pre-configure resmgr callback data
    os_resources.iofunc->iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs_, _RESMGR_IO_NFUNCS, &io_funcs_);
    open_default = connect_funcs_.open;
    // Suppress "AUTOSAR C++14 M5-2-6", The rule states: "A cast shall not convert a pointer to a function
    // to any other pointer type, including a pointer to function type used."
    // QNX API
    // coverity[autosar_cpp14_m5_2_6_violation]
    connect_funcs_.open = &io_open;
    // Suppress "AUTOSAR C++14 M5-2-6", The rule states: "A cast shall not convert a pointer to a function
    // to any other pointer type, including a pointer to function type used."
    // QNX API
    // coverity[autosar_cpp14_m5_2_6_violation]
    io_funcs_.write = &io_write;

    // Suppress "AUTOSAR C++14 M5-0-4", the rule states "An implicit integral conversion shall not change the signedness
    // of the underlying type." This is caused due to macros, no harm from implicit integral conversion here.
    // Suppress "AUTOSAR C++14 M5-0-21" rule findings. This rule declares: "Bitwise operators shall only be
    // applied to operands of unsigned underlying type." Macro does not affect the sign of the result.
    // Suppress "AUTOSAR C++14 M2-13-2" rule findings. This rule declares: "Octal constants (other than zero) and octal
    // escape sequences (other than “\0” ) shall not be used." 0666 is a Unix/POSIX idiom (rw-rw-rw-) and is required.
    // coverity[autosar_cpp14_m5_0_21_violation]
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_m2_13_2_violation]
    constexpr mode_t attrMode{S_IFNAM | 0666};
    constexpr iofunc_attr_t* noAttr{nullptr};
    constexpr _client_info* noClientInfo{nullptr};

    // pre-configure resmgr access rights data
    os_resources.iofunc->iofunc_attr_init(&extended_attr_.attr, attrMode, noAttr, noClientInfo);
}

ResmgrReceiverTraits::ResmgrSetup& ResmgrReceiverTraits::ResmgrSetup::instance(
    const FileDescriptorResourcesType& os_resources) noexcept
{
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.". Due to how the library interface is defined, we cannot avoid singletons in the
    // implementation. We use Meyers singleton as the least problematic solution for multithreaded applications.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static ResmgrSetup setup(os_resources);  // LCOV_EXCL_BR_LINE false positive
    return setup;
}

void ResmgrReceiverTraits::Stop(const std::int32_t side_channel_coid, const FileDescriptorResourcesType& os_resources)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");

    // This function in a banned list, however according to the requirement
    // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
    // NOLINTNEXTLINE(score-banned-function): See above
    score::cpp::ignore = os_resources.channel->MsgSend(
        side_channel_coid, &kPrivateMessageStop, sizeof(kPrivateMessageStop), kNoReply, kNoSize);
}

// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t ResmgrReceiverTraits::io_open(resmgr_context_t* const ctp,
                                           // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be
                                           // used." This is a QNX union.
                                           // coverity[autosar_cpp14_a9_5_1_violation]
                                           io_open_t* const msg,
                                           RESMGR_HANDLE_T* const handle,
                                           void* const extra) noexcept
{
    const auto& context_data = GetContextData(ctp);
    const ResmgrReceiverState& receiver_state = context_data.receiver_state;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(receiver_state.os_resources_), "OS resources are not valid!");
    const auto& allowed_uids = receiver_state.allowed_uids_;

    if (!allowed_uids.empty())
    {
        auto& channel = receiver_state.os_resources_.channel;
        _client_info cinfo{};
        const auto result = channel->ConnectClientInfo(ctp->info.scoid, &cinfo, 0);
        if (!result.has_value())
        {
            return EINVAL;
        }
        const uid_t their_uid = cinfo.cred.euid;
        if (std::find(allowed_uids.begin(), allowed_uids.end(), their_uid) == allowed_uids.end())
        {
            return EACCES;
        }
    }

    ResmgrSetup& setup = ResmgrSetup::instance(receiver_state.os_resources_);
    return setup.open_default(ctp, msg, handle, extra);
}

// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t ResmgrReceiverTraits::CheckWritePreconditions(resmgr_context_t* const ctp,
                                                           // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions
                                                           // shall not be used." QNX union.
                                                           // coverity[autosar_cpp14_a9_5_1_violation]
                                                           // coverity[autosar_cpp14_a8_4_10_violation]
                                                           io_write_t* const msg,
                                                           RESMGR_OCB_T* const ocb) noexcept
{
    const auto& context_data = GetContextData(ctp);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(context_data.receiver_state.os_resources_), "OS resources are not valid!");
    const auto& iofunc = context_data.receiver_state.os_resources_.iofunc;

    // check if the write operation is allowed
    auto result = iofunc->iofunc_write_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    // check if we are requested to do just a plain write
    // Suppress "AUTOSAR C++14 A4-5-1" rule findings. This rule states: "Expressions with type enum or enum class shall
    // not be used as operands to built-in and overloaded operators other than the subscript operator [ ], the
    // assignment operator =, the equality operators == and ! =, the unary & operator, and the relational operators <,
    // <=, >, >=." It is required to perform the operation in order to check if we are requested to do just a plain
    // write.
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to data
    // loss.". Conversion is safe because msg->i.xtype is guaranteed to be within the range of _io_msg_xtypes by design.
    // coverity[autosar_cpp14_a4_5_1_violation]
    // coverity[autosar_cpp14_a4_7_1_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return ENOSYS;
    }
    // get the number of bytes we were asked to write, check that there are enough bytes in the message
    // Suppress "AUTOSAR C++14 A5-16-1" rule finding. this rule states: "The ternary conditional operator shall
    // not be used as a sub-expression.". This is considered safe as it is part of a library macro _IO_WRITE_GET_NBYTES.
    // Suppress AUTOSAR C++14 A5-2-2 rule findings. This rule stated: "Traditional C-style casts
    // shall not be used." This is a false positive. There is no c style casts.
    // coverity[autosar_cpp14_a5_2_2_violation: FALSE]
    // coverity[autosar_cpp14_a5_16_1_violation]
    // coverity[autosar_cpp14_a4_7_1_violation]
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    const std::size_t nbytes = _IO_WRITE_GET_NBYTES(msg);  // LCOV_EXCL_BR_LINE library macro with benign conditonal
    // coverity[autosar_cpp14_a4_7_1_violation]
    const std::size_t nbytes_max = static_cast<std::size_t>(ctp->info.srcmsglen) - ctp->offset - sizeof(io_write_t);
    if (nbytes > nbytes_max)
    {
        return EBADMSG;
    }
    return EOK;
}

// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t ResmgrReceiverTraits::GetMessageData(resmgr_context_t* const ctp,
                                                  // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not
                                                  // be used." QNX union.
                                                  // coverity[autosar_cpp14_a9_5_1_violation]
                                                  io_write_t* const msg,
                                                  const std::size_t nbytes,
                                                  MessageData& message_data) noexcept
{
    const auto& context_data = GetContextData(ctp);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(context_data.receiver_state.os_resources_), "OS resources are not valid!");
    const auto& dispatch = context_data.receiver_state.os_resources_.dispatch;

    if ((nbytes != sizeof(ShortMessage)) && (nbytes != sizeof(MediumMessage)))
    {
        return EBADMSG;
    }

    // get the message payload
    if (!dispatch->resmgr_msgget(ctp, &message_data.payload_, nbytes, sizeof(msg->i)).has_value())
    {
        return EBADMSG;
    }

    // check that the sender is what it claims to be
    if (nbytes == sizeof(ShortMessage))
    {
        const pid_t their_pid = ctp->info.pid;
        // The unions are strictly prohibited by AUTOSAR Rule A9-5-1. But, in this case what we need to do
        // is serialize two type alternatives in most efficient way possible. The union fits this purpose
        // perfectly as long as we use Tagged union in proper manner.
        // Also the union is implementation detail of outer class, so cannot be potentially misused externally.
        // The Tagged union is implemented in accordence with CppCoreGuidelines to minimize the risk.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Serialization optimization necessity
        if (their_pid != message_data.payload_.short_.pid)
        {
            return EBADMSG;
        }
        message_data.type_ = MessageType::kShortMessage;
    }
    else
    {
        const pid_t their_pid = ctp->info.pid;
        // The unions are strictly prohibited by AUTOSAR Rule A9-5-1. But, in this case what we need to do
        // is serialize two type alternatives in most efficient way possible. The union fits this purpose
        // perfectly as long as we use Tagged union in proper manner.
        // Also the union is implementation detail of outer class, so cannot be potentially misused externally.
        // The Tagged union is implemented in accordence with CppCoreGuidelines to minimize the risk.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Serialization optimization necessity
        if (their_pid != message_data.payload_.medium_.pid)
        {
            return EBADMSG;
        }
        message_data.type_ = MessageType::kMediumMessage;
    }
    return EOK;
}

// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t ResmgrReceiverTraits::io_write(resmgr_context_t* const ctp,
                                            // Suppress "AUTOSAR C++14 A9-5-1", The rule states: "Unions shall not be
                                            // used." QNX union.
                                            // coverity[autosar_cpp14_a9_5_1_violation]
                                            // coverity[autosar_cpp14_a8_4_10_violation]
                                            io_write_t* const msg,
                                            RESMGR_OCB_T* const ocb) noexcept
{
    std::int32_t result = CheckWritePreconditions(ctp, msg, ocb);
    if (result != EOK)
    {
        return result;
    }

    // coverity[autosar_cpp14_a4_7_1_violation]
    // coverity[autosar_cpp14_a5_16_1_violation]
    // coverity[autosar_cpp14_a5_2_2_violation : FALSE]
    // coverity[autosar_cpp14_m5_0_4_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    const std::size_t nbytes = _IO_WRITE_GET_NBYTES(msg);  // LCOV_EXCL_BR_LINE library macro with benign conditonal

    MessageData message_data;
    result = GetMessageData(ctp, msg, nbytes, message_data);
    if (result != EOK)
    {
        return result;
    }

    {
        // try to fit the payload to the message queue
        auto& context_data = GetContextData(ctp);
        ResmgrReceiverState& receiver_state = context_data.receiver_state;
        std::lock_guard<std::mutex> guard(receiver_state.message_queue_mutex_);
        if (receiver_state.message_queue_.full())
        {
            // buffer full; reject the message
            return ENOMEM;
        }
        receiver_state.message_queue_.emplace_back(message_data);
    }

    // mark that we have consumed all the bytes
    _IO_SET_WRITE_NBYTES(ctp, static_cast<std::int64_t>(nbytes));

    return EOK;
}

// coverity[autosar_cpp14_a8_4_10_violation]
std::int32_t ResmgrReceiverTraits::private_message_handler(message_context_t* const ctp,
                                                           const std::int32_t /*code*/,
                                                           const std::uint32_t /*flags*/,
                                                           void* const /*handle*/) noexcept
{
    auto& context_data = GetContextData(ctp);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(context_data.receiver_state.os_resources_), "OS resources are not valid!");
    const auto& os_resources = context_data.receiver_state.os_resources_;

    // we only accept private requests from ourselves
    const pid_t their_pid = ctp->info.pid;
    const pid_t our_pid = os_resources.unistd->getpid();
    if (their_pid != our_pid)
    {
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        score::cpp::ignore = os_resources.channel->MsgError(ctp->rcvid, EACCES);
        return EOK;
    }

    context_data.to_terminate = true;
    // This function in a banned list, however according to the requirement
    // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
    // NOLINTNEXTLINE(score-banned-function): See above
    score::cpp::ignore = os_resources.channel->MsgReply(ctp->rcvid, EOK, kNoReply, kNoSize);
    return EOK;
}

bool ResmgrReceiverTraits::IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept
{
    return (((os_resources.channel != nullptr) && (os_resources.dispatch != nullptr)) &&
            ((os_resources.unistd != nullptr) && (os_resources.iofunc != nullptr)));
}

}  // namespace message_passing
}  // namespace com
}  // namespace mw
}  // namespace score
