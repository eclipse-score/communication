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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_UNIQUE_SOCKET_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_UNIQUE_SOCKET_H_

#include <atomic>
#include <cstdint>

namespace score::mw::com::gateway
{

/// \brief Thread-safe RAII wrapper for POSIX socket file descriptors.
class UniqueSocket
{
  public:
    UniqueSocket() noexcept = default;

    explicit UniqueSocket(std::int32_t fd) noexcept;

    ~UniqueSocket() noexcept;

    UniqueSocket(UniqueSocket&& other) noexcept;
    UniqueSocket& operator=(UniqueSocket&& other) noexcept;

    UniqueSocket(const UniqueSocket&) = delete;
    UniqueSocket& operator=(const UniqueSocket&) = delete;

    std::int32_t Get() const noexcept;
    bool IsValid() const noexcept;
    explicit operator bool() const noexcept;

    void Reset(std::int32_t new_fd = kInvalidFd) noexcept;
    void ShutdownAndReset() noexcept;

    ///  Use this when another thread might still be inside recv() or send() on the same fd, since close() would race
    ///  with those calls.
    void ShutdownFd() noexcept;

  private:
    static constexpr std::int32_t kInvalidFd = -1;
    std::atomic<std::int32_t> fd_{kInvalidFd};

    void CloseIfValid() noexcept;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_UNIQUE_SOCKET_H_
