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
#include "score/mw/com/impl/serializer_manager.h"

#include "score/mw/log/logging.h"

#include <dlfcn.h>

namespace score::mw::com::impl
{

namespace
{

// TODO: Do not hard-code the serialization library here. The library is a separately generated and
// deployed.
constexpr const char* kSerializerLibraryName = "libmwcom_serializer_someip.so";

using serialization::someip::SerializerErrc;

}  // namespace

SerializerManager::~SerializerManager() noexcept
{
    // Destroy every serializer (each unique_ptr deleter calls DestroySerializer) before unloading the
    // library, so that the module which created them also deletes them and no serializer outlives the code
    // that implements it.
    serializers_.clear();
    if (library_ != nullptr)
    {
        // NOLINTNEXTLINE(score-banned-function): dlclose is required to unload the serialization library.
        score::cpp::ignore = dlclose(library_);
        library_ = nullptr;
    }
}

score::ResultBlank SerializerManager::EnsureLibraryLoaded() noexcept
{
    if (library_ != nullptr)
    {
        return {};
    }

    // RTLD_LAZY defers resolution of the library's own external dependencies until first used;
    // RTLD_LOCAL keeps the generated CreateSerializer / DestroySerializer symbols out of the global
    // scope so later-loaded libraries cannot interpose them.
    void* const library = dlopen(kSerializerLibraryName, RTLD_LAZY | RTLD_LOCAL);
    if (library == nullptr)
    {
        mw::log::LogError("lola") << "Failed to load serialization library" << kSerializerLibraryName << ":"
                                  << dlerror();
        return score::MakeUnexpected(SerializerErrc::kCreationFailed);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): required for dlsym results.
    auto* const create = reinterpret_cast<serialization::someip::ISerializer* (*)(std::uint64_t) noexcept>(
        dlsym(library, "CreateSerializer"));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): required for dlsym results.
    auto* const destroy = reinterpret_cast<DestroySerializerFn>(dlsym(library, "DestroySerializer"));
    if ((create == nullptr) || (destroy == nullptr))
    {
        mw::log::LogError("lola") << "Failed to resolve serializer factory symbols in" << kSerializerLibraryName;
        score::cpp::ignore = dlclose(library);
        return score::MakeUnexpected(SerializerErrc::kCreationFailed);
    }

    library_ = library;
    create_serializer_ = create;
    destroy_serializer_ = destroy;
    return {};
}

score::Result<std::reference_wrapper<const serialization::someip::ISerializer>> SerializerManager::GetSerializer(
    const std::uint64_t datatype_hash) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    const auto cached = serializers_.find(datatype_hash);
    if (cached != serializers_.cend())
    {
        return std::cref(*cached->second);
    }

    const auto load_result = EnsureLibraryLoaded();
    if (!load_result.has_value())
    {
        return score::MakeUnexpected<std::reference_wrapper<const serialization::someip::ISerializer>>(
            load_result.error());
    }

    SerializerPtr serializer{create_serializer_(datatype_hash), destroy_serializer_};
    if (serializer == nullptr)
    {
        mw::log::LogError("lola") << "No serializer available for datatype hash" << datatype_hash;
        return score::MakeUnexpected(SerializerErrc::kUnknownDatatypeHash);
    }

    const auto inserted = serializers_.emplace(datatype_hash, std::move(serializer));
    return std::cref(*inserted.first->second);
}

}  // namespace score::mw::com::impl
