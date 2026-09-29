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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_WRITER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_WRITER_H

#include "score/mw/com/impl/serialization_sink.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief Writes SOME/IP wire primitives into a (format agnostic) byte sink.
///
/// \details Scalars are written in network byte order. The bytes are assembled explicitly rather than by swapping a
///          host value, so that the result does not depend on the endianness of the host this runs on.
///
/// \attention This class is stateful and must therefore be created _once_ per serialization and shared by every
///            nested serializer: the length field of a length delimited member has to be reserved before its
///            content is known and patched afterwards, which requires remembering where it was reserved. This is
///            why SomeIpSerializer specializations take a SomeIpWriter& instead of a sink: it makes creating a
///            second writer for a nested member impossible to express.
class SomeIpWriter final
{
  public:
    /// \param sink The destination the produced bytes are appended to. Must outlive this writer.
    explicit SomeIpWriter(ISerializationSink& sink) noexcept;

    SomeIpWriter(const SomeIpWriter&) = delete;
    SomeIpWriter(SomeIpWriter&&) noexcept = delete;
    SomeIpWriter& operator=(const SomeIpWriter&) & = delete;
    SomeIpWriter& operator=(SomeIpWriter&&) & noexcept = delete;

    ~SomeIpWriter() = default;

    bool Write(std::uint8_t value) noexcept;
    bool Write(std::uint16_t value) noexcept;
    bool Write(std::uint32_t value) noexcept;
    bool Write(std::uint64_t value) noexcept;

    /// \brief Writes raw bytes without any interpretation.
    bool WriteBytes(const void* data, std::size_t size) noexcept;

    /// \brief Starts a length delimited member by reserving its (4 byte) length field.
    /// \details The length is not known yet at this point, so a placeholder is written and patched by the matching
    ///          EndLengthDelimited().
    bool BeginLengthDelimited() noexcept;

    /// \brief Ends the innermost length delimited member and patches its length field.
    /// \details Fails if no matching BeginLengthDelimited() is open, or if the content does not fit the length
    ///          field. A failure is also reported to the sink, so that a caller which only inspects the sink still
    ///          sees that the produced content is incomplete.
    bool EndLengthDelimited() noexcept;

    /// \brief Whether any operation on this writer failed.
    bool HasFailed() const noexcept;

  private:
    /// \brief Marks the whole serialization as failed, including for an observer which only sees the sink.
    bool Fail() noexcept;

    ISerializationSink& sink_;

    /// \brief Offsets, relative to the start of this serialization, at which length fields still have to be patched.
    std::vector<std::size_t> pending_length_fields_;

    /// \brief Number of bytes written through this writer so far.
    std::size_t bytes_written_;

    bool failed_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SOMEIP_WRITER_H
