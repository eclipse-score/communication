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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REGISTRY_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REGISTRY_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/os/dlfcn.h"
#include "score/os/dlfcn_impl.h"
#include "score/result/result.h"

#include <cstddef>
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
    score::Result<FunctionType> ResolveSerializer(const char* symbol_name) noexcept
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
    void* FindSerializer(const char* symbol_name) noexcept;

    score::os::DlfcnImpl default_dlfcn_{};
    const score::os::Dlfcn* dlfcn_{nullptr};
    void* handle_{nullptr};
    std::unordered_map<std::string, void*> symbol_cache_{};
};

/// \brief The serializer object resolved via dlsym from the loaded backend library.
///
/// Forward declared here; the concrete definition is provided by the dynamically loaded backend.
class SerializerBackend;

/// \brief Abstract interface describing the contract every serializer must fulfil.
class ISerializer
{
  public:
    virtual ~ISerializer() = default;

    virtual score::Result<std::size_t> Serialize(const void* input,
                                                 std::byte* output,
                                                 std::size_t output_size) const noexcept = 0;

    virtual score::Result<std::size_t> Deserialize(const std::byte* input,
                                                   std::size_t input_size,
                                                   void* output) const noexcept = 0;

  protected:
    ISerializer() = default;
    ISerializer(const ISerializer&) = default;
    ISerializer(ISerializer&&) = default;
    ISerializer& operator=(const ISerializer&) = default;
    ISerializer& operator=(ISerializer&&) = default;
};

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

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REGISTRY_H
