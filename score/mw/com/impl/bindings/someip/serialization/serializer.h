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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_H

#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Error codes returned by SOME/IP serializer operations.
enum class SerializerErrc : score::result::ErrorCode
{
    kBufferTooSmall = 1,
    kBufferTooLarge,
    kDeserializationFailed,
    kUnknownDatatypeHash,
    kCreationFailed,
    kInvalidArgument,
    kInternalError,
};

score::result::Error MakeError(const SerializerErrc code, const std::string_view message = "");

/// \brief ABI-stable interface implemented by every generated SOME/IP serializer.
///
/// The generated serialization library (libmwcom_serializer_someip.so) is loaded by mw::com via
/// dlopen and provides one implementation of this interface per datatype. Because this interface is
/// the ABI boundary between mw::com and the library, its layout must remain fixed: all methods are
/// noexcept and report failures through score::Result instead of throwing.
///
/// \note Only big-endian encoding is implemented right now. Little-endian
///       is not supported yet.
/// \note alignment/padding is not supported right now; payloads are encoded packed.
class ISerializer
{
  public:
    virtual ~ISerializer() noexcept = default;

    /// \brief Encodes sample into buffer using the SOME/IP wire format.
    virtual score::Result<void> Serialize(const void* sample, score::cpp::span<std::uint8_t> buffer) const noexcept = 0;

    /// \brief Decodes a SOME/IP encoded buffer into sample.
    virtual score::Result<void> Deserialize(score::cpp::span<const std::uint8_t> buffer,
                                            void* sample) const noexcept = 0;

    /// \brief Returns the worst-case serialized size in bytes for this datatype.
    virtual score::Result<std::size_t> GetMaxSerializedSize() const noexcept = 0;

  protected:
    ISerializer() noexcept = default;
    ISerializer(const ISerializer&) = default;
    ISerializer(ISerializer&&) noexcept = default;
    ISerializer& operator=(const ISerializer&) = default;
    ISerializer& operator=(ISerializer&&) noexcept = default;
};

}  // namespace score::mw::com::impl::someip::serialization

extern "C" {

/// \brief Creates a serializer for the datatype identified by datatype_hash.
///
/// Resolved by mw::com via dlsym. Returns a heap-allocated ISerializer on success, or nullptr when
/// the datatype is unknown or allocation fails. The caller owns the returned pointer and releases it
/// with DestroySerializer.
score::mw::com::impl::someip::serialization::ISerializer* CreateSerializer(std::uint64_t datatype_hash) noexcept;

/// \brief Destroys a serializer previously obtained from CreateSerializer.
void DestroySerializer(score::mw::com::impl::someip::serialization::ISerializer* serializer) noexcept;

}  // extern "C"

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_H
