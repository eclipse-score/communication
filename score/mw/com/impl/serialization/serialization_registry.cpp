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
#include "score/mw/com/impl/serialization/serialization_registry.h"

#include "score/mw/com/impl/com_error.h"

namespace score::mw::com::impl::serialization
{

namespace
{

std::string CurrentDlError(const score::os::Dlfcn& dlfcn)
{
    const auto error = dlfcn.dlerror();
    return error.has_value() ? std::string{error.value()} : std::string{};
}

}  // namespace

SerializationInfrastructure::SerializationInfrastructure(const std::string& library_path,
                                                         const score::os::Dlfcn* dlfcn) noexcept
    : default_dlfcn_{}, dlfcn_{dlfcn == nullptr ? &default_dlfcn_ : dlfcn}, handle_{nullptr}
{
    const auto handle_result =
        dlfcn_->dlopen(library_path.c_str(), score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal);
    if (!handle_result.has_value())
    {
        return;
    }

    handle_ = handle_result.value();
}

SerializationInfrastructure::~SerializationInfrastructure() noexcept
{
    (void)Close();
}

const score::os::Dlfcn& SerializationInfrastructure::dlfcn() const noexcept
{
    return *dlfcn_;
}

score::Result<bool> SerializationInfrastructure::IsOpen() const noexcept
{
    if (handle_ == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serialization infrastructure is not open");
    }

    return true;
}

score::Result<void> SerializationInfrastructure::Close() noexcept
{
    if (handle_ == nullptr)
    {
        return score::MakeUnexpected(ComErrc::kBindingFailure, "serialization infrastructure is already closed");
    }

    const auto close_result = dlfcn().dlclose(handle_);
    if (!close_result.has_value())
    {
        const auto error_message = CurrentDlError(dlfcn());
        if (error_message.empty())
        {
            return score::MakeUnexpected(ComErrc::kBindingFailure, "dlclose failed");
        }
        return score::MakeUnexpected(ComErrc::kBindingFailure, error_message);
    }

    handle_ = nullptr;
    symbol_cache_.clear();
    return {};
}

void* SerializationInfrastructure::FindSerializer(const char* symbol_name) noexcept
{
    if (handle_ == nullptr)
    {
        return nullptr;
    }

    if (symbol_name == nullptr || *symbol_name == '\0')
    {
        return nullptr;
    }

    const std::string symbol_key{symbol_name};
    const auto cached_symbol = symbol_cache_.find(symbol_key);
    if (cached_symbol != symbol_cache_.end())
    {
        return cached_symbol->second;
    }

    const auto symbol_result = dlfcn().dlsym(handle_, symbol_name);
    if (!symbol_result.has_value())
    {
        return nullptr;
    }

    void* const symbol = symbol_result.value();
    symbol_cache_.emplace(symbol_key, symbol);
    return symbol;
}

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
