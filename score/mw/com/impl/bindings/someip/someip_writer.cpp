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
#include "score/mw/com/impl/bindings/someip/someip_writer.h"

#include <array>
#include <limits>

namespace score::mw::com::impl::someip
{

namespace
{

/// \brief Number of bytes a length field occupies.
constexpr std::size_t kLengthFieldSize{4U};

/// \brief Splits value into its big endian byte representation.
/// \details Assembled by shifting instead of reinterpreting the object representation, so that the result is
///          independent of the endianness of the host.
template <typename UnsignedType, std::size_t Size = sizeof(UnsignedType)>
std::array<std::byte, Size> ToBigEndian(const UnsignedType value) noexcept
{
    std::array<std::byte, Size> bytes{};
    for (std::size_t i = 0U; i < Size; ++i)
    {
        const auto shift = static_cast<UnsignedType>(8U * (Size - 1U - i));
        bytes[i] = static_cast<std::byte>((value >> shift) & static_cast<UnsignedType>(0xFFU));
    }
    return bytes;
}

}  // namespace

SomeIpWriter::SomeIpWriter(ISerializationSink& sink) noexcept
    : sink_{sink}, pending_length_fields_{}, bytes_written_{0U}, failed_{false}
{
}

bool SomeIpWriter::Fail() noexcept
{
    failed_ = true;
    // Route the failure through the sink as well, so that a caller which only inspects the sink (which is what the
    // binding does, since a SerializeSampleCallback cannot return anything) still sees that the content is
    // incomplete and must not be sent.
    score::cpp::ignore = sink_.Write(nullptr, 1U);
    return false;
}

bool SomeIpWriter::WriteBytes(const void* const data, const std::size_t size) noexcept
{
    if (failed_)
    {
        return false;
    }
    if (!sink_.Write(data, size))
    {
        failed_ = true;
        return false;
    }
    bytes_written_ += size;
    return true;
}

bool SomeIpWriter::Write(const std::uint8_t value) noexcept
{
    return WriteBytes(&value, sizeof(value));
}

bool SomeIpWriter::Write(const std::uint16_t value) noexcept
{
    const auto bytes = ToBigEndian(value);
    return WriteBytes(bytes.data(), bytes.size());
}

bool SomeIpWriter::Write(const std::uint32_t value) noexcept
{
    const auto bytes = ToBigEndian(value);
    return WriteBytes(bytes.data(), bytes.size());
}

bool SomeIpWriter::Write(const std::uint64_t value) noexcept
{
    const auto bytes = ToBigEndian(value);
    return WriteBytes(bytes.data(), bytes.size());
}

bool SomeIpWriter::BeginLengthDelimited() noexcept
{
    if (failed_)
    {
        return false;
    }

    const auto length_field_offset = bytes_written_;
    if (!Write(std::uint32_t{0U}))
    {
        return false;
    }

    // The vector allocates, which can throw. This function is noexcept, so an allocation failure is turned into a
    // failed serialization instead.
    try
    {
        pending_length_fields_.push_back(length_field_offset);
    }
    // Suppress "AUTOSAR C++14 A15-3-4": catching all exceptions is intended, the failure is reported via the return
    // value of this noexcept function.
    // coverity[autosar_cpp14_a15_3_4_violation]
    catch (...)
    {
        return Fail();
    }
    return true;
}

bool SomeIpWriter::EndLengthDelimited() noexcept
{
    if (failed_)
    {
        return false;
    }
    if (pending_length_fields_.empty())
    {
        // No matching BeginLengthDelimited(): the produced content would be structurally wrong.
        return Fail();
    }

    const auto length_field_offset = pending_length_fields_.back();
    pending_length_fields_.pop_back();

    const auto content_size = bytes_written_ - length_field_offset - kLengthFieldSize;
    if (content_size > std::numeric_limits<std::uint32_t>::max())
    {
        return Fail();
    }

    const auto bytes = ToBigEndian(static_cast<std::uint32_t>(content_size));
    if (!sink_.Patch(length_field_offset, bytes.data(), bytes.size()))
    {
        return Fail();
    }
    return true;
}

bool SomeIpWriter::HasFailed() const noexcept
{
    // An unterminated length delimited member means the content is structurally incomplete.
    return failed_ || (!pending_length_fields_.empty());
}

}  // namespace score::mw::com::impl::someip
