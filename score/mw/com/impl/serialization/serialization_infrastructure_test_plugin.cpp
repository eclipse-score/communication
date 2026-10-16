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

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{

constexpr const char* kLifecycleFileEnv = "SERIALIZER_BACKEND_LIFECYCLE_FILE";

void AppendLifecycleMarker(const char* marker) noexcept
{
    const char* const path = std::getenv(kLifecycleFileEnv);
    if (path == nullptr || path[0] == '\0')
    {
        return;
    }

    std::FILE* const file = std::fopen(path, "a");
    if (file == nullptr)
    {
        return;
    }

    std::fprintf(file, "%s\n", marker);
    std::fclose(file);
}

class LifecycleRecorder final
{
  public:
    LifecycleRecorder()
    {
        AppendLifecycleMarker("loaded");
    }

    ~LifecycleRecorder() noexcept
    {
        AppendLifecycleMarker("unloaded");
    }
};

LifecycleRecorder g_lifecycle_recorder{};

}  // namespace

extern "C" {

std::uint32_t BackendProbeValue() noexcept
{
    return 0xC0FFEE42U;
}

}  // extern "C"