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
#include "score/mw/com/impl/bindings/someip/serialization/serializer_library.h"

#include <dlfcn.h>

namespace score::mw::com::impl::someip::serialization
{

namespace
{

std::string CurrentDlError()
{
    const char* const error = dlerror();
    return error == nullptr ? std::string{} : std::string{error};
}

}  // namespace

SerializerLibrary::SerializerLibrary(const std::string& library_path) noexcept : handle_{nullptr}, error_message_{}
{
    dlerror();
    handle_ = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr)
    {
        error_message_ = CurrentDlError();
    }
}

SerializerLibrary::~SerializerLibrary() noexcept
{
    (void)Close();
}

bool SerializerLibrary::IsOpen() const noexcept
{
    return handle_ != nullptr;
}

const std::string& SerializerLibrary::ErrorMessage() const noexcept
{
    return error_message_;
}

bool SerializerLibrary::Close() noexcept
{
    if (handle_ == nullptr)
    {
        error_message_.clear();
        return false;
    }

    if (dlclose(handle_) != 0)
    {
        error_message_ = CurrentDlError();
        return false;
    }

    handle_ = nullptr;
    error_message_.clear();
    return true;
}

void* SerializerLibrary::ResolveRawSymbol(const char* symbol_name) const noexcept
{
    if (handle_ == nullptr)
    {
        return nullptr;
    }

    dlerror();
    void* const symbol = dlsym(handle_, symbol_name);
    error_message_ = CurrentDlError();
    return error_message_.empty() ? symbol : nullptr;
}

}  // namespace score::mw::com::impl::someip::serialization