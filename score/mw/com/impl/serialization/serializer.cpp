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
#include "score/mw/com/impl/serialization/serializer.h"

#include "score/mw/com/impl/com_error.h"

#include <utility>

namespace score::mw::com::impl::serialization
{

Serializer::Serializer(InstanceSpecifier instance_specifier) noexcept
    : instance_specifier_{std::move(instance_specifier)}
{
}

Serializer::~Serializer() = default;

std::string Serializer::GetSymbolName() const
{
    // TODO: derive the structural and attribute hashes from the service interface and deployment
    // configuration. Placeholder below uses the instance specifier only.
    return "create_serializer_" + std::string{instance_specifier_.ToString()};
}

void Serializer::SetResolvedSymbol(SerializerBackend* backend) noexcept
{
    backend_ = backend;
}

score::Result<std::size_t> Serializer::GetMaxSerializedSize() const noexcept
{
    if (backend_ == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serializer backend has not been resolved");
    }
    return score::MakeUnexpected(ComErrc::kBindingFailure, "max serialized size is not implemented yet");
}

score::Result<std::size_t> Serializer::Serialize(const void* /*input*/,
                                                 std::byte* /*output*/,
                                                 std::size_t /*output_size*/) const noexcept
{
    if (backend_ == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serializer backend has not been resolved");
    }
    return score::MakeUnexpected(ComErrc::kBindingFailure, "serialize is not implemented yet");
}

score::Result<std::size_t> Serializer::Deserialize(const std::byte* /*input*/,
                                                   std::size_t /*input_size*/,
                                                   void* /*output*/) const noexcept
{
    if (backend_ == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serializer backend has not been resolved");
    }
    return score::MakeUnexpected(ComErrc::kBindingFailure, "deserialize is not implemented yet");
}

}  // namespace score::mw::com::impl::serialization
