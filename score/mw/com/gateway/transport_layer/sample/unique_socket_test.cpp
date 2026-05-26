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

#include "score/mw/com/gateway/transport_layer/sample/unique_socket.h"

#include "score/os/mocklib/unistdmock.h"

#include <gtest/gtest.h>
#include <sys/socket.h>

namespace score::mw::com::gateway
{

namespace
{

TEST(UniqueSocketTest, DefaultConstructorCreatesInvalidSocket)
{
    UniqueSocket socket;
    EXPECT_FALSE(socket.IsValid());
    EXPECT_EQ(socket.Get(), -1);
}

TEST(UniqueSocketTest, ConstructorWithFdCreatesValidSocket)
{
    UniqueSocket socket(42);
    EXPECT_TRUE(socket.IsValid());
    EXPECT_EQ(socket.Get(), 42);
}

TEST(UniqueSocketTest, MoveConstructorTransfersOwnership)
{
    UniqueSocket original(42);
    UniqueSocket moved(std::move(original));
    EXPECT_FALSE(original.IsValid());
    EXPECT_TRUE(moved.IsValid());
    EXPECT_EQ(moved.Get(), 42);
}

TEST(UniqueSocketTest, MoveAssignmentOperatorTransfersOwnership)
{
    UniqueSocket original(42);
    UniqueSocket moved;
    moved = std::move(original);
    EXPECT_FALSE(original.IsValid());
    EXPECT_TRUE(moved.IsValid());
    EXPECT_EQ(moved.Get(), 42);
}

TEST(UniqueSocketTest, OperatorBoolReturnsTrueForValidSocket)
{
    UniqueSocket socket(42);
    EXPECT_TRUE(socket);
}

TEST(UniqueSocketTest, ResetReplacesFileDescriptor)
{
    UniqueSocket socket(42);
    socket.Reset(84);
    EXPECT_TRUE(socket.IsValid());
    EXPECT_EQ(socket.Get(), 84);
}

TEST(UniqueSocketTest, ResetWithoutArgumentInvalidatesSocket)
{
    UniqueSocket socket(42);
    socket.Reset();

    EXPECT_FALSE(socket.IsValid());
    EXPECT_EQ(socket.Get(), -1);
}

TEST(UniqueSocketTest, ShutdownFdCallsShutdownIfValidFd)
{
    // Given a valid socket
    UniqueSocket socket(42);
    // ... when calling ShutdownFd()
    socket.ShutdownFd();
    // ... then the socket should still be valid (only the POSIX shutdown() syscall is called)
    EXPECT_TRUE(socket.IsValid());
    EXPECT_EQ(socket.Get(), 42);
}

TEST(UniqueSocketTest, ShutdownAndResetCallsResetForValidFd)
{
    // Given a valid socket and the underlying close() syscall is expected to succeed
    UniqueSocket socket(42);
    score::os::MockGuard<score::os::UnistdMock> unistd_mock;
    EXPECT_CALL(*unistd_mock, close(42)).WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));
    // ... when calling ShutdownAndReset()
    socket.ShutdownAndReset();
    // ... then the socket should be invalid
    EXPECT_FALSE(socket.IsValid());
    EXPECT_EQ(socket.Get(), -1);
}

}  // namespace

}  // namespace score::mw::com::gateway
