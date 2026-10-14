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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_LIBRARY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_LIBRARY_H

#include <string>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Backend helper that owns a dynamically loaded SOME/IP serializer library.
///
/// This keeps the runtime loader behavior in one backend file instead of spreading the raw
/// dlopen/dlsym/dlclose calls across tests or generated example code.
class SerializerLibrary final
{
  public:
    explicit SerializerLibrary(const std::string& library_path) noexcept;
    ~SerializerLibrary() noexcept;

    SerializerLibrary(const SerializerLibrary&) = delete;
    SerializerLibrary& operator=(const SerializerLibrary&) = delete;
    SerializerLibrary(SerializerLibrary&&) = delete;
    SerializerLibrary& operator=(SerializerLibrary&&) = delete;

    bool IsOpen() const noexcept;
    const std::string& ErrorMessage() const noexcept;
    bool Close() noexcept;

    template <typename FunctionType>
    FunctionType ResolveSymbol(const char* symbol_name) const noexcept
    {
        return reinterpret_cast<FunctionType>(ResolveRawSymbol(symbol_name));
    }

  private:
    void* ResolveRawSymbol(const char* symbol_name) const noexcept;

    void* handle_;
    mutable std::string error_message_;
};

}  // namespace score::mw::com::impl::someip::serialization

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_LIBRARY_H