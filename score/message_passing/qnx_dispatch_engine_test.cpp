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

#include "score/message_passing/resource_manager_fixture_base.h"

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

constexpr std::uint16_t kIomgrStickySelect = _IOMGR_PRIVATE_BASE;
constexpr std::uint16_t kIomgrInvalidCode = _IOMGR_PRIVATE_BASE + 1U;

constexpr std::int32_t kTestCoid = 4;

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

class QnxDispatchEngineTestFixture : public ResourceManagerFixtureBase
{
  public:
    void SetUp() override
    {
        ResourceManagerFixtureBase::SetUp();
    }

    void TearDown() override
    {
        if (server_)
        {
            ExpectServerDetached();
            server_->Stop();
        }
        ResourceManagerFixtureBase::TearDown();
    }

    void WithEngineRunningAndServerAttached(LoggingCallback logger = {})
    {
        WithEngineRunning(std::move(logger));
        ExpectServerAttached();
        server_.emplace(engine_);
        QnxDispatchEngine::QnxResourcePath path{"fake_path"};
        EXPECT_TRUE(server_->Start(path));
    }

    void WithEngineRunningAndServerAttachedAndConnectionAccepted(LoggingCallback logger = {})
    {
        WithEngineRunningAndServerAttached(std::move(logger));
        ExpectConnectionOpen();
        ExpectConnectionAccepted();
        EXPECT_CALL(*server_, ProcessConnect)
            .Times(1)
            .WillOnce([this](resmgr_context_t* const ctp, io_open_t* const msg) {
                EXPECT_TRUE(helper_.HelperIsLocked());
                score::cpp::ignore = engine_->AttachConnection(ctp, msg, *server_, connection_);
                return EOK;
            });

        helper_.HelperInsertIoOpen(score::cpp::blank{});
        EXPECT_EQ(helper_.promises_.open.get_future().get(), EOK);
        helper_.promises_.open = std::promise<std::int32_t>();  // reset
        EXPECT_FALSE(helper_.HelperIsLocked());
    }

    std::optional<StrictMock<ResourceManagerServerMock>> server_;
    StrictMock<ResourceManagerConnectionMock> connection_;
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
        .WillOnce(Invoke(&helper_, &ResourceManagerMockHelper::pulse_attach))
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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::pulse_attach));
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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::pulse_attach));
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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::pulse_attach));
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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::pulse_attach));
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
    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    // Check that dispatch_block error does not break dispatch loop
    helper_.HelperInsertDispatchBlockError(ENOMEM);

    ExpectEngineDestructed();
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_RegisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.RegisterPosixEndpoint(posix_endpoint), "");

    // For non-death fork:
    ExpectEngineDestructed();
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_UnregisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.UnregisterPosixEndpoint(posix_endpoint), "");

    // For non-death fork:
    ExpectEngineDestructed();
}

TEST_F(QnxDispatchEngineTestFixture, ServerStart_Unsuccessful)
{
    WithEngineRunning();

    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _)).Times(1).WillOnce(Return(kFakeOsError));

    StrictMock<ResourceManagerServerMock> server{engine_};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    EXPECT_FALSE(server.Start(path));
}

TEST_F(QnxDispatchEngineTestFixture, ServerStartStop)
{
    WithEngineRunning();

    StrictMock<ResourceManagerServerMock> server{engine_};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    // shall be ignored by engine
    server.Stop();

    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(kFakeResmgrServerId));
    EXPECT_CALL(*iofunc_, iofunc_attr_init).Times(1);
    EXPECT_TRUE(server.Start(path));

    EXPECT_CALL(*dispatch_, resmgr_detach(_, kFakeResmgrServerId, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE)));
    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckFailure)
{
    WithEngineRunningAndServerAttached();

    ExpectConnectionOpen();
    helper_.HelperInsertIoOpen(score::cpp::unexpected{ENOMEM});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), ENOMEM);
    EXPECT_FALSE(helper_.HelperIsLocked());
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccess)
{
    WithEngineRunningAndServerAttached();

    ExpectConnectionOpen();
    EXPECT_CALL(*server_, ProcessConnect).Times(1).WillOnce([this](auto&&...) {
        EXPECT_TRUE(helper_.HelperIsLocked());
        return EOK;
    });

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EOK);
    EXPECT_FALSE(helper_.HelperIsLocked());
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccessConnectionAttached)
{
    WithEngineRunningAndServerAttached();

    ExpectConnectionOpen();
    EXPECT_CALL(*iofunc_, iofunc_ocb_attach).Times(1);

    StrictMock<ResourceManagerConnectionMock> connection;

    EXPECT_CALL(*server_, ProcessConnect)
        .Times(1)
        .WillOnce([this, &connection](resmgr_context_t* const ctp, io_open_t* const msg) {
            EXPECT_TRUE(helper_.HelperIsLocked());
            score::cpp::ignore = engine_->AttachConnection(ctp, msg, *server_, connection);
            return EOK;
        });

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    EXPECT_EQ(helper_.promises_.open.get_future().get(), EOK);
    EXPECT_FALSE(helper_.HelperIsLocked());
}

TEST_F(QnxDispatchEngineTestFixture, ServerWriteChecksFailure)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_write_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_write_verify));

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
}

TEST_F(QnxDispatchEngineTestFixture, ServerWriteChecksSuccess)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_write_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_write_verify));

    EXPECT_CALL(connection_, ProcessInput)
        .Times(1)
        .WillOnce([](const std::uint8_t code, const score::cpp::span<const std::uint8_t> message) noexcept {
            EXPECT_EQ(code, 1);
            EXPECT_EQ(message.size(), 3);
            return EOK;
        });

    helper_.HelperInsertIoWrite(score::cpp::blank{}, _IO_XTYPE_NONE, 4UL, 4UL);
    EXPECT_EQ(helper_.promises_.write.get_future().get(), EOK);
}

TEST_F(QnxDispatchEngineTestFixture, ServerReadChecksFailure)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_read_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_read_verify));

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
}

TEST_F(QnxDispatchEngineTestFixture, ServerReadChecksSuccess)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_read_verify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_read_verify));

    EXPECT_CALL(connection_, ProcessReadRequest).Times(1).WillOnce([](auto) noexcept {
        return _RESMGR_NPARTS(1);
    });

    helper_.HelperInsertIoRead(score::cpp::blank{}, _IO_XTYPE_NONE, 4UL);
    EXPECT_EQ(helper_.promises_.read.get_future().get(), _RESMGR_NPARTS(1));
}

TEST_F(QnxDispatchEngineTestFixture, ServerIoMsgSuccessScenarios)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();
    {
        struct select_msg_t
        {
            _io_msg hdr;
            sigevent select_event;
        };

        InSequence is;
        EXPECT_CALL(*dispatch_, resmgr_msgget).WillOnce(Return(sizeof(select_msg_t)));
        EXPECT_CALL(connection_, HasSomethingToRead).WillOnce(Return(false));

        EXPECT_CALL(*dispatch_, resmgr_msgget).WillOnce(Return(sizeof(select_msg_t)));
        EXPECT_CALL(connection_, HasSomethingToRead).WillOnce(Return(true));
        EXPECT_CALL(*channel_, MsgDeliverEvent).Times(1);
    }
    helper_.HelperInsertIoMsg(kIomgrStickySelect);
    EXPECT_EQ(helper_.promises_.msg.get_future().get(), EOK);
    helper_.promises_.msg = std::promise<std::int32_t>();  // reset

    helper_.HelperInsertIoMsg(kIomgrStickySelect);
    EXPECT_EQ(helper_.promises_.msg.get_future().get(), EOK);
}

TEST_F(QnxDispatchEngineTestFixture, ServerIoMsgFailureScenarios)
{
    WithEngineRunningAndServerAttachedAndConnectionAccepted();

    EXPECT_CALL(*dispatch_, resmgr_msgget)
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EFAULT))))
        .WillOnce(Return(1U));

    helper_.HelperInsertIoMsg(kIomgrInvalidCode);
    EXPECT_EQ(helper_.promises_.msg.get_future().get(), ENOSYS);
    helper_.promises_.msg = std::promise<std::int32_t>();  // reset

    helper_.HelperInsertIoMsg(kIomgrStickySelect);
    EXPECT_EQ(helper_.promises_.msg.get_future().get(), EFAULT);
    helper_.promises_.msg = std::promise<std::int32_t>();  // reset

    helper_.HelperInsertIoMsg(kIomgrStickySelect);
    EXPECT_EQ(helper_.promises_.msg.get_future().get(), EBADMSG);
}

TEST_F(QnxDispatchEngineTestFixture, PosixEndpoint)
{
    WithEngineRunning();

    EXPECT_CALL(*channel_, MsgRegisterEvent).Times(1);
    EXPECT_CALL(*channel_, MsgSend).Times(1);
    EXPECT_CALL(*channel_, MsgUnregisterEvent).Times(1);

    // use the engine-provided way to run the code on the requirered thread
    ISharedResourceEngine::CommandQueueEntry command;
    std::promise<void> done;
    engine_->EnqueueCommand(
        command,
        ISharedResourceEngine::TimePoint{},
        [this, &done](auto) noexcept {
            ISharedResourceEngine::PosixEndpointEntry posix_endpoint{};
            posix_endpoint.disconnect = [&done]() {
                done.set_value();
            };
            engine_->RegisterPosixEndpoint(posix_endpoint);
            engine_->UnregisterPosixEndpoint(posix_endpoint);
        },
        this);
    done.get_future().wait();
}

TEST_F(QnxDispatchEngineTestFixture, PosixEndpointEventFailures)
{
    std::int32_t register_error_counter{0};
    std::int32_t send_error_counter{0};
    LoggingCallback logger = [&register_error_counter, &send_error_counter](LogSeverity severity,
                                                                            LogItems items) -> void {
        if ((severity == LogSeverity::kError) && (items.size() > 2) &&
            std::holds_alternative<std::string_view>(items[2]))
        {
            const auto& desc = std::get<std::string_view>(items[2]);
            if (desc.find("MsgRegisterEvent") != desc.npos)
            {
                ++register_error_counter;
            }
            if (desc.find("MsgSend") != desc.npos)
            {
                ++send_error_counter;
            }
        }
    };

    WithEngineRunning(std::move(logger));

    EXPECT_CALL(*channel_, MsgRegisterEvent).Times(1).WillOnce(Return(kFakeOsError));
    EXPECT_CALL(*channel_, MsgSend).Times(1).WillOnce(Return(kFakeOsError));
    EXPECT_CALL(*channel_, MsgUnregisterEvent).Times(1);

    // use the engine-provided way to run the code on the requirered thread
    ISharedResourceEngine::CommandQueueEntry command;
    std::promise<void> done;
    engine_->EnqueueCommand(
        command,
        ISharedResourceEngine::TimePoint{},
        [this, &done](auto) noexcept {
            ISharedResourceEngine::PosixEndpointEntry posix_endpoint{};
            posix_endpoint.fd = kTestCoid;
            posix_endpoint.disconnect = [&done]() {
                done.set_value();
            };
            engine_->RegisterPosixEndpoint(posix_endpoint);
            engine_->UnregisterPosixEndpoint(posix_endpoint);
        },
        this);
    done.get_future().wait();

    EXPECT_EQ(register_error_counter, 1);
    EXPECT_EQ(send_error_counter, 1);
}

TEST_F(QnxDispatchEngineTestFixture, PosixEndpointCoidDeathPulse)
{
    std::int32_t obsolete_counter{0};
    std::int32_t crash_counter{0};
    LoggingCallback logger = [&obsolete_counter, &crash_counter](LogSeverity severity, LogItems items) -> void {
        if ((severity == LogSeverity::kDebug) && (items.size() > 0) &&
            std::holds_alternative<std::string_view>(items[0]))
        {
            const auto& desc = std::get<std::string_view>(items[0]);
            if (desc.find("obsolete") != desc.npos)
            {
                ++obsolete_counter;
            }
            if (desc.find("crash") != desc.npos)
            {
                ++crash_counter;
            }
        }
    };

    WithEngineRunning(std::move(logger));

    EXPECT_CALL(*channel_, MsgRegisterEvent).Times(1);
    EXPECT_CALL(*channel_, MsgSend).Times(1);
    EXPECT_CALL(*channel_, MsgUnregisterEvent).Times(1);

    EXPECT_CALL(*channel_, ConnectServerInfo)
        .WillOnce(Return(kTestCoid))
        .WillOnce(Return(kTestCoid))
        .WillOnce(Return(kTestCoid + 1));

    std::promise<void> done;
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint{};
    posix_endpoint.fd = kTestCoid;
    posix_endpoint.input = [&engine = *engine_, &posix_endpoint]() {
        std::cerr << "posix_endpoint.input" << std::endl;
        engine.UnregisterPosixEndpoint(posix_endpoint);
    };
    posix_endpoint.disconnect = [&done]() {
        done.set_value();
    };

    // use the engine-provided way to run the code on the requirered thread
    ISharedResourceEngine::CommandQueueEntry command;
    engine_->EnqueueCommand(
        command,
        ISharedResourceEngine::TimePoint{},
        [&engine = *engine_, &posix_endpoint](auto) noexcept {
            engine.RegisterPosixEndpoint(posix_endpoint);
        },
        this);

    // death pulses for unregistered endpoints are ignored
    helper_.HelperInsertPulse(_PULSE_CODE_COIDDEATH, kTestCoid + 1);
    helper_.promises_.pulse.get_future().wait();
    EXPECT_EQ(obsolete_counter, 0);
    EXPECT_EQ(crash_counter, 0);
    helper_.promises_.pulse = std::promise<void>();  // reset

    // death pulses for the connections existing for QNX kernel
    // (ConnectServerInfo returns the same coid) are considered obsolete
    // (connections are newer than pulses)
    helper_.HelperInsertPulse(_PULSE_CODE_COIDDEATH, kTestCoid);
    helper_.promises_.pulse.get_future().wait();
    EXPECT_EQ(obsolete_counter, 1);
    EXPECT_EQ(crash_counter, 0);
    helper_.promises_.pulse = std::promise<void>();  // reset

    // death pulses for the connections not existing for QNX kernel
    // (ConnectServerInfo returns the "next" coid) are considered the result of server crash
    // and lead to input callback that would cause connection closure
    helper_.HelperInsertPulse(_PULSE_CODE_COIDDEATH, kTestCoid);
    helper_.promises_.pulse.get_future().wait();
    EXPECT_EQ(obsolete_counter, 1);
    EXPECT_EQ(crash_counter, 1);

    done.get_future().wait();
}

TEST_F(QnxDispatchEngineTestFixture, SendProtocolMessageFailure)
{
    WithEngineRunning();

    EXPECT_CALL(*sysuio_, writev).Times(1).WillOnce(Return(kFakeOsError));

    constexpr std::int32_t kFakeFd{-1};
    constexpr std::uint8_t kFakeCode{0U};
    const score::cpp::span<const std::uint8_t> kFakeMessage{};
    EXPECT_FALSE(engine_->SendProtocolMessage(kFakeFd, kFakeCode, kFakeMessage));
}

TEST_F(QnxDispatchEngineTestFixture, ReceiveProtocolMessageFailure)
{
    WithEngineRunning();

    EXPECT_CALL(*unistd_, read).Times(2).WillOnce(Return(kFakeOsError)).WillOnce(Return(0));

    constexpr std::int32_t kFakeFd{-1};
    std::uint8_t code{};

    const auto os_error_result = engine_->ReceiveProtocolMessage(kFakeFd, code);
    EXPECT_FALSE(os_error_result);
    EXPECT_EQ(os_error_result.error().GetOsDependentErrorCode(), EINVAL);

    const auto zero_read_result = engine_->ReceiveProtocolMessage(kFakeFd, code);
    EXPECT_FALSE(zero_read_result);
    EXPECT_EQ(zero_read_result.error().GetOsDependentErrorCode(), EPIPE);
}

TEST_F(QnxDispatchEngineTestFixture, CleanupNonOwner)
{
    WithEngineRunning();

    // purposedly does nothing
    engine_->CleanUpOwner(nullptr);

    // purposedly cleans nothing on a callback thread
    engine_->CleanUpOwner(this);
}

}  // namespace
}  // namespace message_passing
}  // namespace score
