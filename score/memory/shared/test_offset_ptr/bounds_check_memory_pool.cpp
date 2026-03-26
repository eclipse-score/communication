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
#include "score/memory/shared/test_offset_ptr/bounds_check_memory_pool.h"

namespace score::memory::shared::test
{

template <>
OffsetPtr<void>* CreateOffsetPtr<void>(
    const typename BoundsCheckMemoryPool<void>::MemoryPool::iterator offset_ptr_address,
    const typename BoundsCheckMemoryPool<void>::MemoryPool::iterator pointed_to_address) noexcept
{
    auto* const pointed_to_object = new (pointed_to_address) int(10);
    auto* const pointed_to_object_void = static_cast<void*>(pointed_to_object);
    return new (offset_ptr_address) OffsetPtr<void>(pointed_to_object_void);
}

}  // namespace score::memory::shared::test
