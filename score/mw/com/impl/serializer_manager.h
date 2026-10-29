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
#ifndef SCORE_MW_COM_IMPL_SERIALIZER_MANAGER_H
#define SCORE_MW_COM_IMPL_SERIALIZER_MANAGER_H

#include "score/serialization/someip/serializer.h"

#include "score/result/result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace score::mw::com::impl
{

/// \brief Binding-independent owner and cache of SOME/IP serializers.
///
/// \details The manager loads the generated serialization library once per process with
///          dlopen(RTLD_LAZY | RTLD_LOCAL), resolves the CreateSerializer / DestroySerializer factory
///          symbols with dlsym, and hands out non-owning const ISerializer& references to proxies,
///          skeletons and SampleViews.
///
///          Exactly one serialization::someip::ISerializer is created per datatype_hash on first use
///          and reused afterwards. The manager remains the sole owner: each serializer is stored in a
///          std::unique_ptr whose deleter calls DestroySerializer, so every serializer outlives all of
///          its users and is destroyed at manager shutdown, before the library is unloaded with dlclose.
class SerializerManager
{
  public:
    SerializerManager() noexcept = default;

    SerializerManager(const SerializerManager&) = delete;
    SerializerManager& operator=(const SerializerManager&) & = delete;
    SerializerManager(SerializerManager&&) noexcept = delete;
    SerializerManager& operator=(SerializerManager&&) & noexcept = delete;

    ~SerializerManager() noexcept;

    /// \brief Returns the serializer for the given datatype, creating it once on first use.
    /// \param datatype_hash identifies the datatype whose serializer is requested.
    /// \return non-owning reference to the runtime-owned serializer, or an error if the library could not
    ///         be loaded or the datatype is unknown.
    score::Result<std::reference_wrapper<const serialization::someip::ISerializer>> GetSerializer(
        const std::uint64_t datatype_hash) noexcept;

  private:
    using DestroySerializerFn = void (*)(serialization::someip::ISerializer*) noexcept;
    using SerializerPtr = std::unique_ptr<serialization::someip::ISerializer, DestroySerializerFn>;

    /// \brief Loads the serialization library and resolves its factory symbols on first use.
    score::ResultBlank EnsureLibraryLoaded() noexcept;

    std::mutex mutex_{};

    /// \brief dlopen handle of the serialization library. nullptr until the library is loaded.
    void* library_{nullptr};

    /// \brief Resolved CreateSerializer factory symbol.
    serialization::someip::ISerializer* (*create_serializer_)(std::uint64_t) noexcept {nullptr};

    /// \brief Resolved DestroySerializer factory symbol.
    DestroySerializerFn destroy_serializer_{nullptr};

    /// \brief One serializer per datatype_hash, owned by this manager for the whole process lifetime.
    std::unordered_map<std::uint64_t, SerializerPtr> serializers_{};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERIALIZER_MANAGER_H
