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
#include "score/mw/com/impl/serialization/serializer_library.h"

#include <dlfcn.h>

#include <tuple>
#include <type_traits>

namespace score::serialization::someip
{

namespace
{

void CloseLibrary(void* const handle) noexcept
{
    std::ignore = dlclose(handle);
}

template <typename FunctionPointer>
FunctionPointer ResolveFunction(void* const library, const char* const symbol) noexcept
{
    static_assert(std::is_pointer_v<FunctionPointer> && std::is_function_v<std::remove_pointer_t<FunctionPointer>>,
                  "Only function pointers are resolved this way.");
    // This is the only conversion from the void* returned by dlsym to a function pointer. POSIX requires it to
    // work, which is why ISO C++ makes it conditionally supported. The type is checked by the library with a
    // static_assert on its side of the boundary.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): see above
    return reinterpret_cast<FunctionPointer>(dlsym(library, symbol));
}

}  // namespace

namespace detail
{

score::result::Error TranslateLibraryError(const score::result::Error& library_error) noexcept
{
    const auto code = static_cast<SerializerErrc>(*library_error);
    switch (code)
    {
        case SerializerErrc::kBufferTooSmall:
        case SerializerErrc::kBufferTooLarge:
        case SerializerErrc::kDeserializationFailed:
        case SerializerErrc::kInvalidArgument:
        case SerializerErrc::kInternalError:
            return MakeError(code);
        case SerializerErrc::kCreationFailed:
        case SerializerErrc::kIncompatibleLibrary:
        default:
            break;
    }
    return MakeError(SerializerErrc::kInternalError);
}

}  // namespace detail

score::Result<SerializerLibrary> SerializerLibrary::Open(const std::string& path,
                                                         const SerializerAbiInfo& expected) noexcept
{
    void* const raw_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (raw_handle == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kCreationFailed, "library could not be loaded");
    }
    // From here on, every return unloads the library again.
    std::shared_ptr<void> handle{raw_handle, &CloseLibrary};

    const auto* const library_info = static_cast<const SerializerAbiInfo*>(dlsym(raw_handle, kSerializerAbiInfoSymbol));
    if (library_info == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kIncompatibleLibrary, "library exports no ABI descriptor");
    }
    if (const char* const mismatch = FindAbiMismatch(*library_info, expected); mismatch != nullptr)
    {
        return MakeUnexpected(SerializerErrc::kIncompatibleLibrary, mismatch);
    }

    const auto destroy = ResolveFunction<DestroySerializerFn>(raw_handle, kDestroySerializerSymbol);
    if (destroy == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kIncompatibleLibrary, "library exports no DestroySerializer");
    }
    return SerializerLibrary{std::move(handle), destroy};
}

score::Result<detail::SerializerPtr> SerializerLibrary::CreateSerializer(
    const std::string_view symbol_suffix) const noexcept
{
    const std::string symbol = std::string{kCreateSerializerSymbolPrefix}.append(symbol_suffix);
    const auto create = ResolveFunction<CreateSerializerFn>(handle_.get(), symbol.c_str());
    if (create == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kCreationFailed, "library has no factory for this datatype");
    }

    ISerializer* const serializer = create();
    if (serializer == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kCreationFailed, "factory of the library returned no serializer");
    }
    return detail::SerializerPtr{serializer, detail::SerializerDeleter{handle_, destroy_}};
}

}  // namespace score::serialization::someip
