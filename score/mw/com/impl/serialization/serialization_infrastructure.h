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

#include "score/mw/com/impl/com_error.h"
#include "score/os/dlfcn.h"
#include "score/os/dlfcn_impl.h"
#include "score/result/result.h"

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
    explicit SerializationInfrastructure(const std::string& library_path,
                                        const score::os::Dlfcn* dlfcn = nullptr) noexcept;
    ~SerializationInfrastructure() noexcept;

    SerializationInfrastructure(const SerializationInfrastructure&) = delete;
    SerializationInfrastructure& operator=(const SerializationInfrastructure&) = delete;
    SerializationInfrastructure(SerializationInfrastructure&&) = delete;
    SerializationInfrastructure& operator=(SerializationInfrastructure&&) = delete;

    score::Result<bool> IsOpen() const noexcept;
    score::Result<void> Close() noexcept;

    template <typename FunctionType>
    score::Result<FunctionType> ResolveSerializer(const char* symbol_name) const noexcept
    {
        void* const serializer_symbol = FindSerializer(symbol_name);
        if (serializer_symbol == nullptr)
        {
            return score::MakeUnexpected(ComErrc::kBindingFailure, "could not resolve serializer symbol");
        }
        return reinterpret_cast<FunctionType>(serializer_symbol);
    }

  private:
    const score::os::Dlfcn& dlfcn() const noexcept;
    void* FindSerializer(const char* symbol_name) const noexcept;

    score::os::DlfcnImpl default_dlfcn_{};
    const score::os::Dlfcn* dlfcn_{nullptr};
    void* handle_{nullptr};
    mutable std::unordered_map<std::string, void*> symbol_cache_{};
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_INFRASTRUCTURE_H
