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

#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/serialization/serialization_registry.h"
#include "score/result/result.h"

#include <cstddef>
#include <string>

namespace score::mw::com::impl::serialization
{

/// \brief Concrete serializer bound to one service instance.
class Serializer final : public ISerializer
{
  public:
    explicit Serializer(InstanceSpecifier instance_specifier) noexcept;
    ~Serializer() override;

    /// \brief Builds the factory symbol name, e.g. "create_serializer_<structural-hash>_<attribute-hash>".
    std::string GetSymbolName() const;

    /// \brief Injects the backend object resolved via dlsym by SerializationInfrastructure.
    void SetResolvedSymbol(SerializerBackend* backend) noexcept;

    score::Result<std::size_t> GetMaxSerializedSize() const noexcept override;
    score::Result<std::size_t> Serialize(const void* input,
                                         std::byte* output,
                                         std::size_t output_size) const noexcept override;
    score::Result<std::size_t> Deserialize(const std::byte* input,
                                           std::size_t input_size,
                                           void* output) const noexcept override;

  private:
    InstanceSpecifier instance_specifier_;
    SerializerBackend* backend_{nullptr};  // dlsym result; not owned
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_H
