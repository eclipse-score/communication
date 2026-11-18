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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_ABI_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_ABI_H

#include "score/mw/com/impl/serialization/serializer.h"

#include "score/language/safecpp/string_view/zstring_view.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>

/// \file
/// \brief Binary contract between mw::com and a generated SOME/IP serialization library.
///
/// The library is loaded with dlopen, so the compiler cannot check anything across that boundary. The contract
/// is therefore enforced twice:
///   - at compile time on both sides, by the function types and CreateSerializerOf below. The library asserts
///     that its extern "C" entry points have exactly these types.
///   - at load time, by the SerializerAbiInfo descriptor, which the library exports and mw::com compares against
///     the values of its own build before it calls anything in the library (see SerializerLibrary::Open).

namespace score::mw::com::impl::serialization
{

/// \brief Signature of the factory exported by a library as CreateSerializer_<suffix>.
/// \details Returns a heap-allocated serializer, or nullptr if the allocation fails. The caller releases it
///          with DestroySerializer, so allocation and deallocation happen on the same side of the boundary.
using CreateSerializerFn = ISerializer* (*)() noexcept;

/// \brief Signature of the DestroySerializer function exported by a library.
using DestroySerializerFn = void (*)(ISerializer*) noexcept;

inline constexpr std::string_view kCreateSerializerSymbolPrefix{"CreateSerializer_"};

// The names that are passed to dlsym as they are must be null-terminated.
inline constexpr safecpp::zstring_view kDestroySerializerSymbol{"DestroySerializer"};
inline constexpr safecpp::zstring_view kSerializerAbiInfoSymbol{"mwcom_serializer_abi_info"};

/// \brief Version of the binary contract. Must be incremented by hand whenever the contract changes.
/// \details This covers everything the descriptor cannot measure: the virtual functions of ISerializer, their
///          order and signatures, the values of SerializerErrc, the entry point names and the layout of
///          SerializerAbiInfo itself. A library built against another version is rejected at load time.
///          Running abidiff on the interface in CI would turn forgetting the bump into a build failure.
inline constexpr std::uint32_t kSerializerAbiVersion{1U};

/// \brief Describes the build environment a library was compiled in.
/// \details The library exports one constant instance as kSerializerAbiInfoSymbol. The structure is
///          append-only: struct_size and abi_version stay at offset 0 and 4 in every revision, so that a
///          host can always tell whether it understands the remaining fields. Appending a field requires
///          a bump of kSerializerAbiVersion. Every field has a determined value: an environment that cannot
///          be described, such as an unknown compiler, does not compile (see MakeSerializerAbiInfo).
struct SerializerAbiInfo
{
    std::uint32_t struct_size;
    std::uint32_t abi_version;
    /// 1 = GCC, 2 = Clang.
    std::uint32_t compiler;
    /// The Itanium C++ ABI revision of the compiler (__GXX_ABI_VERSION).
    std::uint32_t cxx_abi;
    /// Low byte: 1 = libstdc++, 2 = libc++. Next byte: _GLIBCXX_USE_CXX11_ABI or _LIBCPP_ABI_VERSION.
    std::uint32_t stdlib;
    /// Sizes of the types that cross the boundary by value.
    std::uint32_t size_of_result_size_t;
    std::uint32_t size_of_result_void;
    std::uint32_t size_of_span;
    std::uint32_t size_of_error;
};

static_assert(std::is_standard_layout_v<SerializerAbiInfo> && std::is_trivially_copyable_v<SerializerAbiInfo>,
              "The descriptor is read from a library built separately and must be plain data.");

namespace detail
{

// The descriptor must describe the build exactly. An "unknown" value would match the "unknown" of a library that
// was built by another toolchain that cannot be identified either, so an environment that is not recognized
// stops the compilation instead.

constexpr std::uint32_t CompilerId() noexcept
{
// Clang defines __GNUC__ as well, so it has to be tested first.
#if defined(__clang__)
    return 2U;
#elif defined(__GNUC__)
    return 1U;
#else
#error "Unknown compiler: SerializerAbiInfo cannot describe it. Add it to CompilerId() in serializer_abi.h."
#endif
}

constexpr std::uint32_t CxxAbiVersion() noexcept
{
#if defined(__GXX_ABI_VERSION)
    return static_cast<std::uint32_t>(__GXX_ABI_VERSION);
#else
#error "__GXX_ABI_VERSION is not defined: the C++ ABI revision of the compiler cannot be determined."
#endif
}

// The macros of the standard libraries are defined by the standard headers included at the top of this file.
constexpr std::uint32_t StandardLibraryId() noexcept
{
#if defined(_LIBCPP_VERSION) && defined(_LIBCPP_ABI_VERSION)
    return 2U | (static_cast<std::uint32_t>(_LIBCPP_ABI_VERSION) << 8U);
#elif defined(__GLIBCXX__) && defined(_GLIBCXX_USE_CXX11_ABI)
    return 1U | (static_cast<std::uint32_t>(_GLIBCXX_USE_CXX11_ABI) << 8U);
#else
#error "Unknown standard library: neither libc++ nor libstdc++ with its ABI macro. See StandardLibraryId()."
#endif
}

}  // namespace detail

/// \brief Describes the environment of the translation unit it is evaluated in.
/// \details Does not compile if the compiler, its C++ ABI revision or the standard library cannot be determined,
///          so a descriptor never contains a value that stands for "unknown".
constexpr SerializerAbiInfo MakeSerializerAbiInfo() noexcept
{
    return SerializerAbiInfo{static_cast<std::uint32_t>(sizeof(SerializerAbiInfo)),
                             kSerializerAbiVersion,
                             detail::CompilerId(),
                             detail::CxxAbiVersion(),
                             detail::StandardLibraryId(),
                             static_cast<std::uint32_t>(sizeof(score::Result<std::size_t>)),
                             static_cast<std::uint32_t>(sizeof(score::Result<void>)),
                             static_cast<std::uint32_t>(sizeof(score::cpp::span<std::uint8_t>)),
                             static_cast<std::uint32_t>(sizeof(score::result::Error))};
}

/// \brief Compares the descriptor of a library with the one of the host.
/// \return The name of the first field that differs, or std::nullopt if the library is compatible. The name refers
///         to a string literal, so it stays valid and can be used as the user message of an error.
/// \details The fields at the fixed offsets are compared first. Only if they match, the library's descriptor
///          has the layout of the host's and the remaining fields are read, so a descriptor of a different
///          revision is never read beyond its end.
constexpr std::optional<std::string_view> FindAbiMismatch(
    const SerializerAbiInfo& library,
    const SerializerAbiInfo& host = MakeSerializerAbiInfo()) noexcept
{
    if (library.abi_version != host.abi_version)
    {
        return "abi_version";
    }
    if (library.struct_size != host.struct_size)
    {
        return "struct_size";
    }
    if (library.compiler != host.compiler)
    {
        return "compiler";
    }
    if (library.cxx_abi != host.cxx_abi)
    {
        return "cxx_abi";
    }
    if (library.stdlib != host.stdlib)
    {
        return "stdlib";
    }
    if (library.size_of_result_size_t != host.size_of_result_size_t)
    {
        return "size_of_result_size_t";
    }
    if (library.size_of_result_void != host.size_of_result_void)
    {
        return "size_of_result_void";
    }
    if (library.size_of_span != host.size_of_span)
    {
        return "size_of_span";
    }
    if (library.size_of_error != host.size_of_error)
    {
        return "size_of_error";
    }
    return std::nullopt;
}

/// \brief Creates the serializer for a factory function of a library.
/// \details A generated library defines its CreateSerializer_<suffix> functions with this, which fixes the
///          allocation policy: a failed allocation yields nullptr instead of terminating.
template <typename Impl>
ISerializer* CreateSerializerOf() noexcept
{
    static_assert(std::is_base_of_v<ISerializer, Impl>, "A serializer implementation must derive from ISerializer.");
    static_assert(std::is_default_constructible_v<Impl>, "The factory constructs the serializer without arguments.");
    return new (std::nothrow) Impl{};
}

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZER_ABI_H
