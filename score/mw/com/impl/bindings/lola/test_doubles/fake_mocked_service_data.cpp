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
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_mocked_service_data.h"

#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"

#include <memory>

namespace score::mw::com::impl::lola
{

namespace
{

using ::score::memory::shared::SharedMemoryResourceHeapAllocatorMock;

const std::uint64_t kControlMemoryResourceId{10U};
const std::uint64_t kDataMemoryResourceId{11U};

}  // namespace

FakeMockedServiceData::FakeMockedServiceData(const pid_t skeleton_process_pid_in)
{
    control_memory = std::make_shared<SharedMemoryResourceHeapAllocatorMock>(kControlMemoryResourceId);
    data_memory = std::make_shared<SharedMemoryResourceHeapAllocatorMock>(kDataMemoryResourceId);

    data_control = control_memory->construct<ServiceDataControl>(control_memory->getMemoryResourceProxy());
    data_storage = data_memory->construct<ServiceDataStorage>(data_memory->getMemoryResourceProxy());

    data_storage->skeleton_pid_ = skeleton_process_pid_in;
}

}  // namespace score::mw::com::impl::lola
