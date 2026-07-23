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

#include <limits>
#include <memory>

namespace score::mw::com::impl::lola
{

namespace
{

using ::score::memory::shared::SharedMemoryResourceHeapAllocatorMock;

// \todo: This needs to be revisited on why this should be hardcoded in the first place. until then we use the maximum
// possible value for the memory resource ID to avoid any potential conflicts with memory
//  resource IDs that are be generated during tests.
const std::uint64_t kControlMemoryResourceId{std::numeric_limits<std::uint64_t>::max()};
const std::uint64_t kDataMemoryResourceId{std::numeric_limits<std::uint64_t>::max() - 1U};

// Fixed capacity used for the fixed-capacity containers within the fake ServiceDataStorage. Test-doubles add their
// events dynamically, so we simply reserve a generous upper bound.
constexpr std::size_t kMaxNumberOfServiceElements{100U};

}  // namespace

FakeMockedServiceData::FakeMockedServiceData(const pid_t skeleton_process_pid_in, uid_t skeleton_uid_in)
{
    control_memory =
        std::make_shared<::testing::NiceMock<SharedMemoryResourceHeapAllocatorMock>>(kControlMemoryResourceId);
    data_memory = std::make_shared<::testing::NiceMock<SharedMemoryResourceHeapAllocatorMock>>(kDataMemoryResourceId);

    data_control = control_memory->construct<ServiceDataControl>(*control_memory);
    data_storage = data_memory->construct<ServiceDataStorage>(kMaxNumberOfServiceElements, *data_memory);

    data_storage->skeleton_pid_ = skeleton_process_pid_in;
    data_storage->skeleton_uid_ = skeleton_uid_in;
}

}  // namespace score::mw::com::impl::lola
