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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_VECTOR_SERIALIZATION_SINK_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_VECTOR_SERIALIZATION_SINK_H

#include "score/mw/com/impl/serialization_sink.h"

#include <cstddef>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief ISerializationSink which appends the serialized bytes to a std::vector, up to a fixed capacity.
///
/// \details The sink does not own the vector. It is meant to be constructed on the stack around a buffer which is
///          kept (and reused) by the caller across sends, so that the steady state does not allocate: the buffer is
///          reserved once up front, and clearing it before each serialization only resets its size.
///          Writes which would exceed the capacity fail instead of growing the buffer, so that the memory used per
///          sample stays within the bound the calling layer declared.
class VectorSerializationSink final : public ISerializationSink
{
  public:
    /// \param buffer The buffer to append to. Must outlive this sink. It is _not_ cleared by the sink, so that
    ///        several writes accumulate; the caller decides when a new serialization starts.
    /// \param max_size Maximum number of bytes the buffer may hold. A write beyond it fails.
    VectorSerializationSink(std::vector<std::byte>& buffer, std::size_t max_size) noexcept;

    bool Write(const void* data, std::size_t size) noexcept override;

    bool Patch(std::size_t offset, const void* data, std::size_t size) noexcept override;

    /// \brief Whether any Write() on this sink failed.
    /// \details A SerializeSampleCallback returns void, so a serializer which ignores the result of Write() cannot
    ///          report a failure. The flag is sticky, which lets the caller check once, after serialization
    ///          finished, whether the accumulated content is complete and may be sent.
    bool HasWriteFailed() const noexcept;

  private:
    std::vector<std::byte>& buffer_;
    std::size_t max_size_;
    bool write_failed_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_VECTOR_SERIALIZATION_SINK_H
