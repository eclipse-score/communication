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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LOOKUP_KEY_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LOOKUP_KEY_H

#include <cstdint>
#include <string>

namespace score::mw::com::impl::serialization
{

/// \brief Identifies which concrete serializer the binding-independent layer must load.
///
/// The key is composed of two hashes that together select exactly one serializer in the
/// dynamically loaded serializer library:
///   - the `structural-hash` comes from the service interface (the data type/layout fingerprint),
///   - the `attribute-hash` comes from the deployment configuration (discovered through the
///     `InstanceSpecifier` carried by the `SerializationRequest`).
///
/// Deriving the two hashes is the responsibility of the surrounding integration (service interface
/// and deployment configuration). This type only holds them and turns them into the factory symbol
/// name the loader resolves.
class SerializerLookupKey final
{
  public:
    SerializerLookupKey(std::uint64_t structural_hash, std::uint64_t attribute_hash) noexcept;

    std::uint64_t GetStructuralHash() const noexcept;
    std::uint64_t GetAttributeHash() const noexcept;

    /// \brief Build the serializer factory symbol name to resolve from the serializer library.
    ///
    /// The name follows the documented pattern `create_serializer_<structural-hash>_<attribute-hash>`
    /// where each hash is formatted as a zero-padded 16-digit lowercase hex value.
    std::string FactorySymbolName() const;

  private:
    std::uint64_t structural_hash_;
    std::uint64_t attribute_hash_;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LOOKUP_KEY_H
