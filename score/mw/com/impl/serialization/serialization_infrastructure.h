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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_INFRASTRUCTURE_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_INFRASTRUCTURE_H

#include <string>
#include <unordered_map>

namespace score::mw::com::impl::serialization
{

/// \brief Generic infrastructure helper that owns a dynamically loaded serializer backend.
///
/// This keeps the runtime loader behavior in one infrastructure object instead of spreading the raw
/// dlopen/dlsym/dlclose calls across tests or generated example code.
class SerializationInfrastructure final
{
  public:
    explicit SerializationInfrastructure(const std::string& library_path) noexcept;
    ~SerializationInfrastructure() noexcept;

    SerializationInfrastructure(const SerializationInfrastructure&) = delete;
    SerializationInfrastructure& operator=(const SerializationInfrastructure&) = delete;
    SerializationInfrastructure(SerializationInfrastructure&&) = delete;
    SerializationInfrastructure& operator=(SerializationInfrastructure&&) = delete;

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
    mutable std::unordered_map<std::string, void*> symbol_cache_;
    mutable std::string error_message_;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_INFRASTRUCTURE_H