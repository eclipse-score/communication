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
#include "score/message_passing/log/logging_callback.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <locale>
#include <ostream>
#include <streambuf>
#include <string_view>
#include <variant>

namespace score::message_passing
{

namespace
{

// A std::streambuf that writes into a fixed, caller-provided memory block. When the block is full the default
// overflow() returns EOF, which sets the stream's badbit and silently drops the remainder - so the logger neither
// allocates nor overflows the buffer. Wrapping a stack buffer this way lets the console fallback logger reuse the
// standard ostream inserters for formatting while still emitting the whole line with a single write.
//
// Rationale for a single write: the previous implementation serialised its per-item output with a function-local
// `static std::mutex`. Such a static is registered for destruction via __cxa_atexit and is therefore destroyed
// during process shutdown (__cxa_finalize). Background dispatch threads (e.g. a ClientConnection retrying
// TryOpenClientConnection) can still be logging at that point; locking the already-destroyed mutex returns EINVAL,
// libc++ throws std::system_error, and in the no-exception QNX build __cxa_allocate_exception aborts the process.
// Assembling the whole line first and emitting it with one std::cerr.write() removes the need for any shared state:
// concurrent log calls no longer interleave, and std::cerr is guaranteed valid for the whole program execution
// ([iostream.objects]). Content exceeding the buffer is truncated, which is acceptable for a debug fallback logger.
class FixedBufferStreamBuf final : public std::streambuf
{
  public:
    FixedBufferStreamBuf(char* const buffer, const std::size_t size) noexcept
    {
        setp(buffer, std::next(buffer, static_cast<std::ptrdiff_t>(size)));
    }

    std::string_view View() const noexcept
    {
        return std::string_view{pbase(), static_cast<std::size_t>(pptr() - pbase())};
    }
};

}  // namespace

LoggingCallback GetCerrLogger()
{
    return [](LogSeverity /*severity*/, LogItems items) -> void {
        std::array<char, 1024U> buffer{};
        FixedBufferStreamBuf stream_buffer{buffer.data(), buffer.size()};
        std::ostream stream{&stream_buffer};
        // Force the classic locale so integer formatting is deterministic (no digit grouping) and allocation-free.
        stream.imbue(std::locale::classic());
        for (const auto& item : items)
        {
            std::visit(
                [&stream](auto&& arg) -> void {
                    stream << arg;
                },
                item);
        }
        stream << '\n';
        const std::string_view line = stream_buffer.View();
        static_cast<void>(std::cerr.write(line.data(), static_cast<std::streamsize>(line.size())));
    };
}

}  // namespace score::message_passing
