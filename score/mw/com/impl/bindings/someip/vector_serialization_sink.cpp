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
#include "score/mw/com/impl/bindings/someip/vector_serialization_sink.h"

#include <cstring>

namespace score::mw::com::impl::someip
{

VectorSerializationSink::VectorSerializationSink(std::vector<std::byte>& buffer, const std::size_t max_size) noexcept
    : ISerializationSink{}, buffer_{buffer}, max_size_{max_size}, write_failed_{false}
{
}

bool VectorSerializationSink::Write(const void* const data, const std::size_t size) noexcept
{
    if (size == 0U)
    {
        return true;
    }
    if (data == nullptr)
    {
        write_failed_ = true;
        return false;
    }

    const auto previous_size = buffer_.size();
    // Staying within the declared bound is what keeps the memory per sample predictable. Growing the buffer instead
    // would silently exceed the size the calling layer reserved for this event.
    if (size > (max_size_ - previous_size))
    {
        write_failed_ = true;
        return false;
    }

    // The buffer was reserved to max_size_ up front, so this does not allocate. Should it still do so (and throw),
    // a sink write must not propagate an exception to the (noexcept) serializer callback, so the allocation failure
    // is reported as a failed write instead.
    try
    {
        buffer_.resize(previous_size + size);
    }
    // Suppress "AUTOSAR C++14 A15-3-4": catching all exceptions is intended here, as the failure is translated into
    // the bool return value of this noexcept function.
    // coverity[autosar_cpp14_a15_3_4_violation]
    catch (...)
    {
        write_failed_ = true;
        return false;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) writing at the former end of the buffer
    std::memcpy(buffer_.data() + previous_size, data, size);
    return true;
}

bool VectorSerializationSink::HasWriteFailed() const noexcept
{
    return write_failed_;
}

bool VectorSerializationSink::Patch(const std::size_t offset, const void* const data, const std::size_t size) noexcept
{
    if (size == 0U)
    {
        return true;
    }
    if ((data == nullptr) || (size > (buffer_.size() - offset)) || (offset > buffer_.size()))
    {
        write_failed_ = true;
        return false;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) bounds verified above
    std::memcpy(buffer_.data() + offset, data, size);
    return true;
}

}  // namespace score::mw::com::impl::someip
