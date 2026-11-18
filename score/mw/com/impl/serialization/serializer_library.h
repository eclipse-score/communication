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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LIBRARY_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LIBRARY_H

#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_abi.h"

#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace score::serialization::someip
{

namespace detail
{

/// \brief Translates an error returned by a library into the error domain of this binary.
/// \details An Error refers to the domain of the module that created it, and two Errors are equal only if they
///          refer to the same domain object. The library has a domain object of its own, so its errors would
///          never equal ours, and they would refer to unloaded memory once the library is unloaded. Codes the
///          library may report are mapped to the same code in our domain, everything else to kInternalError.
///          The user message of the library is dropped for the same reason.
score::result::Error TranslateLibraryError(const score::result::Error& library_error) noexcept;

template <typename T>
score::Result<T> TranslateLibraryResult(score::Result<T> library_result) noexcept
{
    if (library_result.has_value())
    {
        return library_result;
    }
    return MakeUnexpected<T>(TranslateLibraryError(library_result.error()));
}

/// \brief Destroys a serializer in the library that created it, then releases the library.
/// \details The deleter is stored inside the unique_ptr, so the reference to the library is dropped only after
///          the library has run the destructor of the serializer. The library cannot be unloaded while a
///          serializer created from it is alive.
class SerializerDeleter final
{
  public:
    SerializerDeleter(std::shared_ptr<void> library, const DestroySerializerFn destroy) noexcept
        : library_{std::move(library)}, destroy_{destroy}
    {
    }

    void operator()(ISerializer* const serializer) const noexcept
    {
        destroy_(serializer);
    }

  private:
    std::shared_ptr<void> library_;
    DestroySerializerFn destroy_;
};

using SerializerPtr = std::unique_ptr<ISerializer, SerializerDeleter>;

}  // namespace detail

class SerializerLibrary;

/// \brief Serializer for one datatype of a loaded library.
/// \details This is the only place where a Sample is turned into the void* of the ISerializer interface. The
///          pairing of a datatype and its serializer is fixed when the serializer is created, so it cannot be
///          called with a sample of another type. Errors are reported in the error domain of this binary.
///          The serializer keeps its library loaded and may outlive the SerializerLibrary it was created from.
template <typename Sample>
class TypedSerializer final
{
  public:
    /// \brief Encodes sample into buffer using the SOME/IP wire format.
    /// \return The number of bytes written to buffer, or an error.
    score::Result<std::size_t> Serialize(const Sample& sample,
                                         const score::cpp::span<std::uint8_t> buffer) const noexcept
    {
        return detail::TranslateLibraryResult(serializer_->Serialize(std::addressof(sample), buffer));
    }

    /// \brief Decodes a SOME/IP encoded buffer into sample.
    score::Result<void> Deserialize(const score::cpp::span<const std::uint8_t> buffer, Sample& sample) const noexcept
    {
        return detail::TranslateLibraryResult(serializer_->Deserialize(buffer, std::addressof(sample)));
    }

    /// \brief Returns the worst-case serialized size in bytes for Sample.
    score::Result<std::size_t> GetMaxSerializedSize() const noexcept
    {
        return detail::TranslateLibraryResult(serializer_->GetMaxSerializedSize());
    }

  private:
    friend class SerializerLibrary;

    explicit TypedSerializer(detail::SerializerPtr serializer) noexcept : serializer_{std::move(serializer)} {}

    detail::SerializerPtr serializer_;
};

/// \brief A generated serialization library that was loaded and verified.
/// \details Copies share the loaded library, which is unloaded when the last copy of the SerializerLibrary and
///          the last serializer created from it are gone.
class SerializerLibrary final
{
  public:
    /// \brief Loads the library at path and verifies it against expected.
    /// \details The descriptor of the library is compared with expected before any other symbol is resolved,
    ///          and the library is unloaded again if it does not match. expected is only a parameter so that
    ///          tests can provoke a mismatch; mw::com uses the default, the descriptor of its own build.
    /// \return The library, or an error:
    ///         - kCreationFailed if the library cannot be loaded.
    ///         - kIncompatibleLibrary if the library exports no descriptor or one that differs from expected.
    ///           In the latter case the user message names the first field that differs.
    static score::Result<SerializerLibrary> Open(const std::string& path,
                                                 const SerializerAbiInfo& expected = MakeSerializerAbiInfo()) noexcept;

    /// \brief Creates the serializer of the factory CreateSerializer_<symbol_suffix> for Sample.
    /// \return The serializer, or kCreationFailed if the library has no such factory or the factory fails.
    template <typename Sample>
    score::Result<TypedSerializer<Sample>> Create(const std::string_view symbol_suffix) const noexcept
    {
        auto serializer = CreateSerializer(symbol_suffix);
        if (!serializer.has_value())
        {
            return MakeUnexpected<TypedSerializer<Sample>>(serializer.error());
        }
        return TypedSerializer<Sample>{std::move(serializer.value())};
    }

  private:
    SerializerLibrary(std::shared_ptr<void> handle, const DestroySerializerFn destroy) noexcept
        : handle_{std::move(handle)}, destroy_{destroy}
    {
    }

    score::Result<detail::SerializerPtr> CreateSerializer(std::string_view symbol_suffix) const noexcept;

    std::shared_ptr<void> handle_;
    DestroySerializerFn destroy_;
};

}  // namespace score::serialization::someip

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_LIBRARY_H
