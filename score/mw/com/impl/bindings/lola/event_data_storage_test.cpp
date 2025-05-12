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
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"

#include "score/containers/dynamic_array.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(EventDataStorageTest, EventDataStorageUsesADynamicArrayToRepresentSlots)
{
    // Our detailed design (aas/lib/memory/design/shared_memory/OffsetPtrDesign.md#dynamic-array-considerations)
    // requires that we use a DynamicArray to represent our slots so that bounds checking is done.
    using DummySampleType = int;
    static_assert(
        std::is_same_v<
            score::containers::DynamicArray<
                DummySampleType,
                std::scoped_allocator_adaptor<memory::shared::PolymorphicOffsetPtrAllocator<DummySampleType>>>,
            EventDataStorage<DummySampleType>>,
        "EventDataControl should use a dynamic array to represent slots.");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
