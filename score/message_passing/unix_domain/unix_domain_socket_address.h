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
#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H

#include <algorithm>
#include <string_view>

#include <sys/socket.h>
#include <sys/un.h>

namespace score::message_passing::detail
{

class UnixDomainSocketAddress
{
  public:
    UnixDomainSocketAddress(std::string_view path, bool isAbstract) noexcept;
    const char* GetAddressString() const noexcept
    {
        return &(addr_.sun_path[IsAbstract() ? 1 : 0]);
    }

    bool IsAbstract() const noexcept
    {
        return addr_.sun_path[0] == '\0';
    }

    const sockaddr* data() const& noexcept
    {
        return reinterpret_cast<const sockaddr*>(&addr_);
    }
    const sockaddr* data() const&& noexcept = delete;
    static constexpr std::size_t size() noexcept
    {
        return sizeof(addr_);
    }

  private:
    struct sockaddr_un addr_{};
};

inline UnixDomainSocketAddress::UnixDomainSocketAddress(const std::string_view path, const bool isAbstract) noexcept
{
    addr_ = sockaddr_un{};
    addr_.sun_family = static_cast<sa_family_t>(AF_UNIX);
    const size_t len = std::min(sizeof(addr_.sun_path) - 1U - (isAbstract ? 1U : 0U), path.size());
    std::copy_n(path.data(), len, addr_.sun_path + (isAbstract ? 1 : 0));
}

}  // namespace score::message_passing::detail

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H
