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

using QnxDispatchEngineTestFixture = ResourceManagerFixtureBase;
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
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to create timer");
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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_write_verify));

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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_write_verify));

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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_read_verify));

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
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_read_verify));

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

TEST_F(QnxDispatchEngineTestFixture, ServerNotify)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    ExpectConnectionAccepted();

    EXPECT_CALL(*iofunc_, iofunc_notify)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceManagerMockHelper::iofunc_notify));

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

    EXPECT_CALL(connection, HasSomethingToRead).Times(2).WillOnce(Return(false)).WillOnce(Return(true));

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(score::cpp::blank{});
    helper_.promises_.open.get_future().get();

    helper_.HelperInsertIoNotify(_NOTIFY_COND_OUTPUT);
    EXPECT_EQ(helper_.promises_.notify.get_future().get(), EOK);
    helper_.promises_.notify = std::promise<std::int32_t>();  // reset

    helper_.HelperInsertIoNotify(_NOTIFY_COND_INPUT | _NOTIFY_COND_OUTPUT);
    EXPECT_EQ(helper_.promises_.notify.get_future().get(), EOK);

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, PosixEndpoint)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();

    EXPECT_CALL(*dispatch_, select_attach).Times(1);
    EXPECT_CALL(*dispatch_, select_detach).Times(1);

    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    // use the engine-provided way to run the code on the requirered thread
    ISharedResourceEngine::CommandQueueEntry command;
    std::promise<void> done;
    engine.EnqueueCommand(
        command,
        ISharedResourceEngine::TimePoint{},
        [this, &engine, &done](auto) noexcept {
            ISharedResourceEngine::PosixEndpointEntry posix_endpoint{};
            posix_endpoint.disconnect = [&done]() {
                done.set_value();
            };
            engine.RegisterPosixEndpoint(posix_endpoint);
            engine.UnregisterPosixEndpoint(posix_endpoint);
        },
        this);
    done.get_future().wait();
}

TEST_F(QnxDispatchEngineTestFixture, SendProtocolMessageFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();

    EXPECT_CALL(*sysuio_, writev).Times(1).WillOnce(Return(kFakeOsError));

    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    constexpr std::int32_t kFakeFd{-1};
    constexpr std::uint8_t kFakeCode{0U};
    const score::cpp::span<const std::uint8_t> kFakeMessage{};
    EXPECT_FALSE(engine.SendProtocolMessage(kFakeFd, kFakeCode, kFakeMessage));
}

TEST_F(QnxDispatchEngineTestFixture, ReceiveProtocolMessageFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();

    EXPECT_CALL(*unistd_, read).Times(2).WillOnce(Return(kFakeOsError)).WillOnce(Return(0));

    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    constexpr std::int32_t kFakeFd{-1};
    std::uint8_t code{};

    const auto os_error_result = engine.ReceiveProtocolMessage(kFakeFd, code);
    EXPECT_FALSE(os_error_result);
    EXPECT_EQ(os_error_result.error().GetOsDependentErrorCode(), EINVAL);

    const auto zero_read_result = engine.ReceiveProtocolMessage(kFakeFd, code);
    EXPECT_FALSE(zero_read_result);
    EXPECT_EQ(zero_read_result.error().GetOsDependentErrorCode(), EPIPE);
}

TEST_F(QnxDispatchEngineTestFixture, CleanupNonOwner)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();

    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    // purposedly does nothing
    engine.CleanUpOwner(nullptr);

    // purposedly cleans nothing on a callback thread
    engine.CleanUpOwner(this);
}

}  // namespace
}  // namespace message_passing
}  // namespace score
