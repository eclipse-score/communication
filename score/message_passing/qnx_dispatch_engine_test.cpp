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
#include <gtest/gtest.h>

#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include "score/concurrency/synchronized_queue.h"

#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/qnx/mock_channel.h"
#include "score/os/mocklib/qnx/mock_dispatch.h"
#include "score/os/mocklib/qnx/mock_iofunc.h"
#include "score/os/mocklib/qnx/mock_timer.h"
#include "score/os/mocklib/sys_uio_mock.h"
#include "score/os/mocklib/unistdmock.h"

#include <future>
#include <unordered_map>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

class ResourceManagerServerMock : public QnxDispatchEngine::ResourceManagerServer
{
  public:
    ResourceManagerServerMock(std::shared_ptr<QnxDispatchEngine> engine)
        : QnxDispatchEngine::ResourceManagerServer(std::move(engine))
    {
    }

    MOCK_METHOD(std::int32_t, ProcessConnect, (resmgr_context_t*, io_open_t*), (noexcept, override));
};

class ResourceManagerConnectionMock : public QnxDispatchEngine::ResourceManagerConnection
{
  public:
    MOCK_METHOD(bool, ProcessInput, (std::uint8_t code, score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD(void, ProcessDisconnect, (), (noexcept, override));
    MOCK_METHOD(bool, HasSomethingToRead, (), (noexcept, override));
    MOCK_METHOD(std::int32_t, ProcessReadRequest, (resmgr_context_t*), (noexcept, override));
};

/// \brief Provides QNX Resource Manager emulation layer for use with OSAL mocks
///
/// This helper class is used to emulate the QNX Resource Manager dispatch loop functionality and to provide the ability
/// to inject different events into the loop.
///
/// It consists of two sets of methods:
///
/// * One set is the OSAL function substitutes that are supposed to keep the needed consitent
///   state of the emulator between the mock calls. These functions have the same names as the respective OSAL methods
///   and are supposed to be employed as drop-in mock actions using gmock Invoke() functionality, like in:
///   ```
///       EXPECT_CALL(*dispatch_, dispatch_block)
///           .Times(AnyNumber())
///           .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::dispatch_block));
///   ```
///
/// * The other set, with the method names starting with `Helper` prefix, is supposed to analyze and modify the emulated
///   Resource Manager state for testing purposes.
///
/// The class also exposes the synchronizatio objects (promises) to help synchronize the events in dispatch loop thread
/// under testing with the events in the main thread of the test suite, and in particular, to make sure that some events
/// on the dispatch loop thread happened before the main thread proceeds with future actions.
///
/// The QNX Resource Manager emulation is not perfect and could be incorrect in some edge cases that are not supposed
/// to not appear in testing. In particular, one should be carefl with ending the lifetimes of server and conection
/// mocks before the injected events associated with them have been processed by the dispatch loop thread.
class ResourceMockHelper
{
  public:
    using pulse_handler_t = std::int32_t (*)(message_context_t* ctp,
                                             std::int32_t code,
                                             std::uint32_t flags,
                                             void* handle) noexcept;

    /// Registers the pulse handler callback to call in dispatch loop
    score::cpp::expected<std::int32_t, score::os::Error> pulse_attach(dispatch_t* const /*dpp*/,
                                                             const std::int32_t /*flags*/,
                                                             const std::int32_t code,
                                                             const pulse_handler_t func,
                                                             void* const handle) noexcept
    {
        pulse_handlers_.emplace(code, std::make_pair(func, handle));
        return code;
    }

    /// Registers the `connect_funcs` and `io_funcs` callbacks to call in dispatch loop
    score::cpp::expected<std::int32_t, score::os::Error> resmgr_attach(dispatch_t* const /*dpp*/,
                                                              resmgr_attr_t* const /*attr*/,
                                                              const char* const /*path*/,
                                                              const enum _file_type /*file_type*/,
                                                              const std::uint32_t /*flags*/,
                                                              const resmgr_connect_funcs_t* const connect_funcs,
                                                              const resmgr_io_funcs_t* const io_funcs,
                                                              RESMGR_HANDLE_T* const handle) noexcept
    {
        connect_funcs_ = connect_funcs;
        io_funcs_ = io_funcs;
        handle_ = handle;
        return kFakeResmgrServerId;
    }

    /// Blocks the dispatch loop till it has an event to process. Special treatment for `ErrnoPseudoMessage` event
    score::cpp::expected_blank<score::os::Error> dispatch_block(dispatch_context_t* const /*ctp*/) noexcept
    {
        const auto max_wait = std::chrono::milliseconds(1000);

        auto result = message_queue_.Pop(max_wait, score::cpp::stop_token{});
        if (!result.has_value())
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ETIMEDOUT));
        }
        if (std::holds_alternative<ErrnoPseudoMessage>(*result))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(std::get<ErrnoPseudoMessage>(*result).error));
        }
        current_message_ = std::move(*result);
        return {};
    }

    /// Processes the event extracted by `dispatch_block`
    score::cpp::expected_blank<std::int32_t> dispatch_handler(dispatch_context_t* const /*ctp*/) noexcept
    {
        if (std::holds_alternative<PulseMessage>(current_message_))
        {
            auto& pulse = std::get<PulseMessage>(current_message_);

            message_context_t context{};
            resmgr_iomsgs_t message{};
            context.msg = &message;
            message.pulse.value.sival_int = pulse.value;

            auto& handler = pulse_handlers_[pulse.code];
            score::cpp::ignore = handler.first(&context, pulse.code, 0, handler.second);
        }
        else if (std::holds_alternative<IoOpenMessage>(current_message_))
        {
            resmgr_context_t context{};
            io_open_t message{};

            const auto result = (*connect_funcs_->open)(&context, &message, handle_, nullptr);
            promises_.open.set_value(result);
        }
        else if (std::holds_alternative<IoWriteMessage>(current_message_))
        {
            auto& io_write = std::get<IoWriteMessage>(current_message_);
            resmgr_context_t context{};
            struct IoWriteData
            {
                io_write_t message;
                char payload[4];
            } data{{}, {1, 2, 3, 4}};
            io_write_t& message = data.message;
            message.i.xtype = io_write.xtype;
            message.i.nbytes = io_write.nbytes;
            context.offset = 0;  // explicitly
            context.info.msglen = context.offset + sizeof(io_write_t) + io_write.nbytes_max;

            const auto result = (*io_funcs_->write)(&context, &message, ocb_);
            promises_.write.set_value(result);
        }
        else if (std::holds_alternative<IoReadMessage>(current_message_))
        {
            auto& io_read = std::get<IoReadMessage>(current_message_);
            resmgr_context_t context{};
            io_read_t message{};
            message.i.xtype = io_read.xtype;
            message.i.nbytes = io_read.nbytes;

            const auto result = (*io_funcs_->read)(&context, &message, ocb_);
            promises_.read.set_value(result);
        }
        return {};
    }

    /// Helps maintain lock counter for testing purposes by increasing the counter
    score::cpp::expected_blank<std::int32_t> iofunc_attr_lock(iofunc_attr_t* const /*attr*/) noexcept
    {
        ++lock_count_;
        return {};
    }

    /// Helps maintain lock counter for testing purposes by decreasing the counter
    score::cpp::expected_blank<std::int32_t> iofunc_attr_unlock(iofunc_attr_t* const /*attr*/) noexcept
    {
        --lock_count_;
        return {};
    }

    /// Gives the ability to trigger the error handling branch in the `io_open` handler
    score::cpp::expected_blank<std::int32_t> iofunc_open(resmgr_context_t* const /*ctp*/,
                                                  io_open_t* const /*msg*/,
                                                  iofunc_attr_t* const /*attr*/,
                                                  iofunc_attr_t* const /*dattr*/,
                                                  struct _client_info* const /*info*/) noexcept
    {
        auto& io_open = std::get<IoOpenMessage>(current_message_);
        return io_open.iofunc_open_result;
    }

    /// Binds the resource manager connection object to the resource manager server that created the connection
    score::cpp::expected_blank<std::int32_t> iofunc_ocb_attach(resmgr_context_t* const /*ctp*/,
                                                        io_open_t* const /*msg*/,
                                                        iofunc_ocb_t* const ocb,
                                                        iofunc_attr_t* const attr,
                                                        const resmgr_io_funcs_t* const /*io_funcs*/) noexcept
    {
        ocb->attr = (extended_dev_attr_t*)attr;
        ocb_ = ocb;
        return {};
    }

    /// Returns the specified iofunc_write_verify result
    score::cpp::expected_blank<std::int32_t> iofunc_write_verify(resmgr_context_t* const /*ctp*/,
                                                          io_write_t* const /*msg*/,
                                                          iofunc_ocb_t* const /*ocb*/,
                                                          std::int32_t* const /*nonblock*/) noexcept
    {
        auto& io_write = std::get<IoWriteMessage>(current_message_);
        return io_write.iofunc_write_verify_result;
    }

    /// Returns the specified iofunc_read_verify result
    score::cpp::expected_blank<std::int32_t> iofunc_read_verify(resmgr_context_t* const /*ctp*/,
                                                         io_read_t* const /*msg*/,
                                                         iofunc_ocb_t* const /*ocb*/,
                                                         std::int32_t* const /*nonblock*/) noexcept
    {
        auto& io_read = std::get<IoReadMessage>(current_message_);
        return io_read.iofunc_read_verify_result;
    }

    /// Queues a pulse event to process in dispatch loop
    score::cpp::expected_blank<score::os::Error> MsgSendPulse(const std::int32_t coid,
                                                     const std::int32_t priority,
                                                     const std::int32_t code,
                                                     const std::int32_t value) noexcept
    {
        message_queue_.CreateSender().push(PulseMessage{code, value});
        return {};
    }

    /// Queues a special kind of event that causes `dispatch_block` to return failure
    void HelperInsertDispatchBlockError(std::int32_t error)
    {
        message_queue_.CreateSender().push(ErrnoPseudoMessage{error});
    }

    /// Queues an io_open event (specifying the return status of the respective `iofunc_open`)
    void HelperInsertIoOpen(score::cpp::expected_blank<std::int32_t> iofunc_open_result)
    {
        message_queue_.CreateSender().push(IoOpenMessage{iofunc_open_result});
    }

    /// Check if the server resource attribute structure is locked
    bool HelperIsLocked() const
    {
        return lock_count_ != 0;
    }

    /// Queues an io_write event
    void HelperInsertIoWrite(score::cpp::expected_blank<std::int32_t> iofunc_write_verify_result,
                             std::int32_t xtype = _IO_XTYPE_NONE,
                             std::size_t nbytes = 0UL,
                             std::size_t nbytes_max = 0UL)
    {
        message_queue_.CreateSender().push(IoWriteMessage{iofunc_write_verify_result, xtype, nbytes, nbytes_max});
    }

    /// Queues an io_write event
    void HelperInsertIoRead(score::cpp::expected_blank<std::int32_t> iofunc_read_verify_result,
                            std::int32_t xtype = _IO_XTYPE_NONE,
                            std::size_t nbytes = 0UL)
    {
        message_queue_.CreateSender().push(IoReadMessage{iofunc_read_verify_result, xtype, nbytes});
    }

    constexpr static std::int32_t kFakeResmgrServerId{1};

    struct Promises
    {
        std::promise<std::int32_t> open;   ///< Fulfilled when `io_open` event has been processed
        std::promise<std::int32_t> write;  ///< Fulfilled when `io_write` event has been processed
        std::promise<std::int32_t> read;   ///< Fulfilled when `io_read` event has been processed
    };

    Promises promises_{};

  private:
    struct PulseMessage
    {
        std::int32_t code;
        std::int32_t value;
    };

    struct ErrnoPseudoMessage
    {
        std::int32_t error;
    };

    struct IoOpenMessage
    {
        score::cpp::expected_blank<std::int32_t> iofunc_open_result;
    };

    struct IoWriteMessage
    {
        score::cpp::expected_blank<std::int32_t> iofunc_write_verify_result;
        std::int32_t xtype;
        std::size_t nbytes;
        std::size_t nbytes_max;
    };

    struct IoReadMessage
    {
        score::cpp::expected_blank<std::int32_t> iofunc_read_verify_result;
        std::int32_t xtype;
        std::size_t nbytes;
    };

    using QueueMessage =
        std::variant<std::monostate, ErrnoPseudoMessage, PulseMessage, IoOpenMessage, IoWriteMessage, IoReadMessage>;

    std::unordered_map<std::int32_t, std::pair<pulse_handler_t, void*>> pulse_handlers_{};
    const resmgr_connect_funcs_t* connect_funcs_{};
    const resmgr_io_funcs_t* io_funcs_{};
    RESMGR_HANDLE_T* handle_{};
    RESMGR_OCB_T* ocb_{};

    constexpr static std::size_t kMaxQueueLength{5};
    score::concurrency::SynchronizedQueue<QueueMessage> message_queue_{kMaxQueueLength};
    QueueMessage current_message_{};

    std::int32_t lock_count_{0};
};

class QnxDispatchEngineTestFixture : public ::testing::Test
{
  protected:
    template <typename Mock>
    static void SetupResource(score::cpp::pmr::unique_ptr<Mock>& mock_ptr)
    {
        mock_ptr = score::cpp::pmr::make_unique<Mock>(score::cpp::pmr::get_default_resource());
    }

    void SetUp() override
    {
        SetupResource(channel_);
        SetupResource(dispatch_);
        SetupResource(fcntl_);
        SetupResource(iofunc_);
        SetupResource(timer_);
        SetupResource(sysuio_);
        SetupResource(unistd_);
    }

    void TearDown() override {}

    QnxDispatchEngine::OsResources MoveMockOsResources() noexcept
    {
        return {std::move(channel_),
                std::move(dispatch_),
                std::move(fcntl_),
                std::move(iofunc_),
                std::move(timer_),
                std::move(sysuio_),
                std::move(unistd_)};
    }

    void ExpectEngineConstructed()
    {
        EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(1).WillOnce(Return(kFakeDispatchPtr));
        EXPECT_CALL(*dispatch_, pulse_attach)
            .Times(2)
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
        EXPECT_CALL(*dispatch_, message_connect).Times(1).WillOnce(Return(kFakeCoid));
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Return(kFakeResmgrEmptyId));
        EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(1).WillOnce(Return(kFakeContextPtr));
        EXPECT_CALL(*timer_, TimerCreate).Times(1).WillOnce(Return(kFakeTimerId));
        EXPECT_CALL(*iofunc_, iofunc_func_init).Times(1);
    }

    void ExpectEngineThreadRunning()
    {
        EXPECT_CALL(*dispatch_, dispatch_block)
            .Times(AnyNumber())
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::dispatch_block));
        EXPECT_CALL(*dispatch_, dispatch_handler)
            .Times(AnyNumber())
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::dispatch_handler));
    }

    void ExpectEngineDestructed()
    {
        EXPECT_CALL(*channel_, MsgSendPulse)
            .Times(1)
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::MsgSendPulse));

        EXPECT_CALL(*timer_, TimerDestroy).Times(1);
        EXPECT_CALL(*channel_, ConnectDetach).Times(1);
        EXPECT_CALL(*dispatch_, pulse_detach).Times(2);
        EXPECT_CALL(*dispatch_, dispatch_destroy).Times(1);
        EXPECT_CALL(*dispatch_, dispatch_context_free).Times(1);
    }

    void ExpectServerAttached()
    {
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::resmgr_attach));
        EXPECT_CALL(*iofunc_, iofunc_attr_init).Times(1);
    }

    void ExpectServerDetached()
    {
        EXPECT_CALL(*dispatch_,
                    resmgr_detach(_, kFakeResmgrServerId, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE)));
    }

    void ExpectConnectionOpen()
    {
        InSequence is;
        EXPECT_CALL(*iofunc_, iofunc_attr_lock)
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_attr_lock));
        EXPECT_CALL(*iofunc_, iofunc_open).Times(1).WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_open));
        EXPECT_CALL(*iofunc_, iofunc_attr_unlock)
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_attr_unlock));
    }

    void ExpectConnectionAccepted()
    {
        EXPECT_CALL(*iofunc_, iofunc_ocb_attach)
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_ocb_attach));
    }

    template <typename Mock>
    using mock_ptr = score::cpp::pmr::unique_ptr<StrictMock<Mock>>;

    mock_ptr<score::os::MockChannel> channel_;
    mock_ptr<score::os::MockDispatch> dispatch_;
    mock_ptr<score::os::FcntlMock> fcntl_;
    mock_ptr<score::os::MockIoFunc> iofunc_;
    mock_ptr<score::os::qnx::MockTimer> timer_;
    mock_ptr<score::os::SysUioMock> sysuio_;
    mock_ptr<score::os::UnistdMock> unistd_;

    ResourceMockHelper helper_;

    constexpr static dispatch_t* kFakeDispatchPtr{nullptr};
    constexpr static dispatch_context_t* kFakeContextPtr{nullptr};
    constexpr static std::int32_t kFakeCoid{0};
    constexpr static std::int32_t kFakeTimerId{0};
    constexpr static std::int32_t kFakeResmgrEmptyId{0};
    constexpr static std::int32_t kFakeResmgrServerId{ResourceMockHelper::kFakeResmgrServerId};
    const score::cpp::unexpected<score::os::Error> kFakeOsError{score::os::Error::createFromErrno(EINVAL)};
};

using QnxDispatchEngineDeathTest = QnxDispatchEngineTestFixture;

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_CreateChannel)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to allocate dispatch handle");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AttachTimerPulse)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to attach timer pulse code");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AttachEventPulse)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillOnce(Invoke(&helper_, &ResourceMockHelper::pulse_attach))
        .WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to attach event pulse code");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_ConnectSideChannel)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to create side channel");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_CreateTimer)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()), "Unable to create timer");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_SetUpResourceManager)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeTimerId));
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
        .Times(AnyNumber())
        .WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to set up resource manager operations");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AllocateContext)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeTimerId));
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
        .Times(AnyNumber())
        .WillOnce(Return(kFakeResmgrEmptyId));
    EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to allocate context pointer");
}

TEST_F(QnxDispatchEngineTestFixture, EngineCreationAndDestruction)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    // Check that dispatch_block error does not break dispatch loop
    helper_.HelperInsertDispatchBlockError(ENOMEM);
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_RegisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    // For non-death fork:
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.RegisterPosixEndpoint(posix_endpoint), "");
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_UnregisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    // For non-death fork:
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.UnregisterPosixEndpoint(posix_endpoint), "");
}

TEST_F(QnxDispatchEngineTestFixture, ServerStart_Unsuccessful)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _)).Times(1).WillOnce(Return(kFakeOsError));
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    EXPECT_FALSE(server.Start(path));
}

TEST_F(QnxDispatchEngineTestFixture, ServerStartStop)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    {
        InSequence is;
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Return(kFakeResmgrServerId));
        EXPECT_CALL(*iofunc_, iofunc_attr_init).Times(1);

        EXPECT_CALL(*dispatch_,
                    resmgr_detach(_, kFakeResmgrServerId, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE)));
    }
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    // shall be ignored by engine
    server.Stop();

    EXPECT_TRUE(server.Start(path));
    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::unexpected{ENOMEM});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), ENOMEM);
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccess)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    EXPECT_CALL(server, ProcessConnect).Times(1).WillOnce([this](auto&&...) {
        EXPECT_TRUE(helper_.HelperIsLocked());
        return EOK;
    });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EOK);
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccessConnectionAttached)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    EXPECT_CALL(*iofunc_, iofunc_ocb_attach).Times(1);

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            EXPECT_TRUE(helper_.HelperIsLocked());
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EOK);
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerWriteChecksFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    ExpectConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_write_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::iofunc_write_verify));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    helper_.promises_.open.get_future().get();

    // iofunc_write_verify unexpected
    helper_.HelperInsertIoWrite(score::cpp::unexpected{ENOMEM});
    EXPECT_EQ(helper_.promises_.write.get_future().get(), ENOMEM);
    helper_.promises_.write = std::promise<std::int32_t>();  // reset

    // unsupported write request type
    helper_.HelperInsertIoWrite(score::cpp::blank{}, _IO_XTYPE_READDIR);
    EXPECT_EQ(helper_.promises_.write.get_future().get(), ENOSYS);
    helper_.promises_.write = std::promise<std::int32_t>();  // reset

    // too small write request size
    helper_.HelperInsertIoWrite(score::cpp::blank{}, _IO_XTYPE_NONE, 0UL, 4UL);
    EXPECT_EQ(helper_.promises_.write.get_future().get(), EBADMSG);
    helper_.promises_.write = std::promise<std::int32_t>();  // reset

    // too large write request size
    helper_.HelperInsertIoWrite(score::cpp::blank{}, _IO_XTYPE_NONE, 8UL, 4UL);
    EXPECT_EQ(helper_.promises_.write.get_future().get(), EMSGSIZE);

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerWriteChecksSuccess)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    ExpectConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_write_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::iofunc_write_verify));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_CALL(connection, ProcessInput)
        .Times(1)
        .WillOnce([](const std::uint8_t code, const score::cpp::span<const std::uint8_t> message) noexcept {
            EXPECT_EQ(code, 1);
            EXPECT_EQ(message.size(), 3);
            return EOK;
        });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    helper_.promises_.open.get_future().get();

    helper_.HelperInsertIoWrite(score::cpp::blank{}, _IO_XTYPE_NONE, 4UL, 4UL);
    EXPECT_EQ(helper_.promises_.write.get_future().get(), EOK);

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerReadChecksFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    ExpectConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_read_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::iofunc_read_verify));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    helper_.promises_.open.get_future().get();

    // iofunc_reaf_verify unexpected
    helper_.HelperInsertIoRead(score::cpp::unexpected{ENOMEM});
    EXPECT_EQ(helper_.promises_.read.get_future().get(), ENOMEM);
    helper_.promises_.read = std::promise<std::int32_t>();  // reset

    // unsupported read request type
    helper_.HelperInsertIoRead(score::cpp::blank{}, _IO_XTYPE_READDIR);
    EXPECT_EQ(helper_.promises_.read.get_future().get(), ENOSYS);
    helper_.promises_.read = std::promise<std::int32_t>();  // reset

    // too small read request size
    helper_.HelperInsertIoRead(score::cpp::blank{}, _IO_XTYPE_NONE, 0UL);
    EXPECT_EQ(helper_.promises_.read.get_future().get(), _RESMGR_NPARTS(0));

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerReadChecksSuccess)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    ExpectConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_read_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::iofunc_read_verify));

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_CALL(connection, ProcessReadRequest).Times(1).WillOnce([](auto) noexcept {
        return _RESMGR_NPARTS(1);
    });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    helper_.promises_.open.get_future().get();

    helper_.HelperInsertIoRead(score::cpp::blank{}, _IO_XTYPE_NONE, 4UL);
    EXPECT_EQ(helper_.promises_.read.get_future().get(), _RESMGR_NPARTS(1));

    server.Stop();
}

}  // namespace
}  // namespace message_passing
}  // namespace score
