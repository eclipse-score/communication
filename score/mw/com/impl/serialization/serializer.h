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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_H

#include <cstddef>

namespace score::mw::com::impl::serialization
{

/// \brief Strategy interface implemented by a concrete, binding-specific serializer.
///
/// The binding-independent layer never implements this interface. It only obtains an instance of it
/// from the dynamically loaded serializer library (see `SerializationOrchestrator`) and drives the
/// serialize/deserialize calls. The concrete implementation lives in the binding-specific
/// serialization unit and knows the actual wire format.
class ISerializer
{
  public:
    virtual ~ISerializer() = default;

    /// \brief Serialize `input` into the `output` buffer.
    /// \return The number of bytes written to `output`.
    virtual std::size_t Serialize(const void* input, std::byte* output, std::size_t size) = 0;

    /// \brief Deserialize `input` bytes into the `output` object.
    /// \return The number of bytes consumed from `input`.
    virtual std::size_t Deserialize(void* output, const std::byte* input, std::size_t size) = 0;

  protected:
    ISerializer() = default;
    ISerializer(const ISerializer&) = default;
    ISerializer& operator=(const ISerializer&) = default;
    ISerializer(ISerializer&&) = default;
    ISerializer& operator=(ISerializer&&) = default;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_H
