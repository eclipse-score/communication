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

#include "score/os/unistd.h"

#include <sys/socket.h>

#include <tuple>

namespace score::mw::com::gateway
{

UniqueSocket::UniqueSocket(std::int32_t fd) noexcept : fd_(fd) {}

UniqueSocket::~UniqueSocket() noexcept
{
    CloseIfValid();
}

UniqueSocket::UniqueSocket(UniqueSocket&& other) noexcept : fd_(other.fd_.exchange(kInvalidFd)) {}

UniqueSocket& UniqueSocket::operator=(UniqueSocket&& other) noexcept
{
    if (this != &other)
    {
        CloseIfValid();
        fd_.store(other.fd_.exchange(kInvalidFd));
    }
    return *this;
}

std::int32_t UniqueSocket::Get() const noexcept
{
    return fd_.load();
}

bool UniqueSocket::IsValid() const noexcept
{
    return fd_.load() >= 0;
}

UniqueSocket::operator bool() const noexcept
{
    return IsValid();
}

void UniqueSocket::Reset(std::int32_t new_fd) noexcept
{
    CloseIfValid();
    fd_.store(new_fd);
}

void UniqueSocket::ShutdownAndReset() noexcept
{
    const auto fd = fd_.load();
    if (fd >= 0)
    {
        ::shutdown(fd, SHUT_RDWR);
    }
    Reset();
}

void UniqueSocket::ShutdownFd() noexcept
{
    const auto fd = fd_.load();
    if (fd >= 0)
    {
        ::shutdown(fd, SHUT_RDWR);
    }
}

void UniqueSocket::CloseIfValid() noexcept
{
    const auto fd = fd_.exchange(kInvalidFd);
    if (fd >= 0)
    {
        std::ignore = score::os::Unistd::instance().close(fd);
    }
}

}  // namespace score::mw::com::gateway
