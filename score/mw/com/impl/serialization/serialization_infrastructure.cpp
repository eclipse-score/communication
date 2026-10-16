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
#include "score/mw/com/impl/serialization/serialization_infrastructure.h"

#include "score/os/dlfcn_impl.h"

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

SerializationInfrastructure::SerializationInfrastructure(const std::string& library_path) noexcept
    : handle_{nullptr}, error_message_{}
{
    score::os::DlfcnImpl dlfcn{};
    const auto handle_result =
        dlfcn.dlopen(library_path.c_str(), score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal);
    if (!handle_result.has_value())
    {
        error_message_ = CurrentDlError(dlfcn);
        if (error_message_.empty())
        {
            error_message_ = "dlopen failed";
        }
        return;
    }

    handle_ = handle_result.value();
}

SerializationInfrastructure::~SerializationInfrastructure() noexcept
{
    (void)Close();
}

bool SerializationInfrastructure::IsOpen() const noexcept
{
    return handle_ != nullptr;
}

const std::string& SerializationInfrastructure::ErrorMessage() const noexcept
{
    return error_message_;
}

bool SerializationInfrastructure::Close() noexcept
{
    if (handle_ == nullptr)
    {
        error_message_.clear();
        return false;
    }

    score::os::DlfcnImpl dlfcn{};
    const auto close_result = dlfcn.dlclose(handle_);
    if (!close_result.has_value())
    {
        error_message_ = CurrentDlError(dlfcn);
        if (error_message_.empty())
        {
            error_message_ = "dlclose failed";
        }
        return false;
    }

    handle_ = nullptr;
    symbol_cache_.clear();
    error_message_.clear();
    return true;
}

void* SerializationInfrastructure::ResolveRawSymbol(const char* symbol_name) const noexcept
{
    if (handle_ == nullptr)
    {
        error_message_.clear();
        return nullptr;
    }

    if (symbol_name == nullptr || symbol_name[0] == '\0')
    {
        error_message_ = "symbol_name must not be empty";
        return nullptr;
    }

    const std::string symbol_key{symbol_name};
    const auto cached_symbol = symbol_cache_.find(symbol_key);
    if (cached_symbol != symbol_cache_.end())
    {
        error_message_.clear();
        return cached_symbol->second;
    }

    score::os::DlfcnImpl dlfcn{};
    const auto symbol_result = dlfcn.dlsym(handle_, symbol_name);
    if (!symbol_result.has_value())
    {
        error_message_ = CurrentDlError(dlfcn);
        if (error_message_.empty())
        {
            error_message_ = "dlsym failed";
        }
        return nullptr;
    }

    void* const symbol = symbol_result.value();
    symbol_cache_.emplace(symbol_key, symbol);
    error_message_.clear();
    return symbol;
}

}  // namespace score::mw::com::impl::serialization