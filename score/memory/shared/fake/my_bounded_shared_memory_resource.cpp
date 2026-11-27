/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/memory/shared/fake/my_bounded_shared_memory_resource.h"

namespace score::memory::shared::test
{

MyBoundedSharedMemoryResource::MyBoundedSharedMemoryResource(
    const std::size_t memory_resource_size,
    const bool register_resource_with_registry,
    const std::optional<MemoryResourceRegistry::MemoryResourceIdentifier> memory_resource_id)
    : resource_{memory_resource_size, register_resource_with_registry, memory_resource_id}
{
}

MyBoundedSharedMemoryResource::MyBoundedSharedMemoryResource(
    const std::pair<void*, void*> memory_range,
    const bool register_resource_with_registry,
    const std::optional<MemoryResourceRegistry::MemoryResourceIdentifier> memory_resource_id)
    : resource_{memory_range, register_resource_with_registry, memory_resource_id}
{
}

}  // namespace score::memory::shared::test
