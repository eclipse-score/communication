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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SINK_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SINK_H

#include <score/utility.hpp>

#include <cstddef>

namespace score::mw::com::impl
{

/// \brief Destination a SerializeSampleCallback writes the serialized representation of a sample into.
///
/// \details This decouples the (strongly typed) serialization logic from the storage strategy of the binding which
///          consumes the bytes. The serializer only appends bytes; where they end up (a heap buffer, a pre-allocated
///          transmission buffer, ...) is the binding's decision. Using a sink instead of a fixed destination buffer
///          keeps variable-length payloads (strings, dynamic arrays) expressible, which the SOME/IP wire format
///          requires, without changing the callback signature again later.
class ISerializationSink
{
  public:
    ISerializationSink() = default;
    virtual ~ISerializationSink();

    ISerializationSink(const ISerializationSink&) = delete;
    ISerializationSink(ISerializationSink&&) noexcept = delete;
    ISerializationSink& operator=(const ISerializationSink&) & = delete;
    ISerializationSink& operator=(ISerializationSink&&) & noexcept = delete;

    /// \brief Appends size bytes read from data to the sink.
    /// \param data Pointer to the first byte to append. Only read during the call.
    /// \param size Number of bytes to append.
    /// \return True on success. False if the bytes could not be appended (e.g. a bounded sink ran out of capacity).
    ///         Once a write failed, the content of the sink shall be considered incomplete and must not be sent.
    virtual bool Write(const void* data, std::size_t size) noexcept = 0;

    /// \brief Overwrites already written bytes at the given offset.
    /// \details This is a capability of the underlying storage, not a statement about any wire format: it exists so
    ///          that a producer can reserve a field whose value is only known later (e.g. the length of a variable
    ///          length member) and fill it in afterwards. Sinks which cannot revisit written bytes (e.g. a streaming
    ///          sink) keep the default and report that they do not support it.
    /// \param offset Offset in bytes from the beginning of the sink's content.
    /// \param data Pointer to the first byte to write. Only read during the call.
    /// \param size Number of bytes to overwrite. The range has to lie completely within what was already written.
    /// \return True on success, false if the sink does not support patching or the range is out of bounds.
    virtual bool Patch(const std::size_t offset, const void* const data, const std::size_t size) noexcept
    {
        score::cpp::ignore = offset;
        score::cpp::ignore = data;
        score::cpp::ignore = size;
        return false;
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SINK_H
