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
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/os/ObjectSeam.h"
#include "score/os/mocklib/unistdmock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sched.h>
#include <sys/types.h>
#include <type_traits>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{10U};
constexpr std::size_t kNumberOfServiceElements{4U};

using namespace ::testing;

class ServiceDataStorageFixture : public ::testing::Test
{
  public:
    ServiceDataStorageFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(::testing::Return(&lola_runtime_mock_));
    }

    RuntimeMockGuard runtime_mock_guard_{};
    RuntimeMock lola_runtime_mock_{};
};

TEST(ServiceDataStorageTest, GenericProxyEventMetaInfoIsStoredInServiceDataStorage)
{
    RecordProperty("Verifies", "SCR-32391303");
    RecordProperty("Description",
                   "Checks that the EventMataInfo is stored within ServiceDataStorage. Another test checks that "
                   "ServiceDataStorage is read-only.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same_v<ServiceDataStorage::EventMetaInfoMap, decltype(ServiceDataStorage::events_metainfo_)>,
                  "ServiceDataStorage does not contain a map of EventMetaInfo.");
}

// When compiling for linux, the underlying container allocator differs. Since the tests for satisfying requirements
// must be on qnx, we only check the service element event map type on qnx.
#if !defined(__linux__)
TEST_F(ServiceDataStorageFixture, ServiceElementsAreIndexedUsingElementFqId)
{
    RecordProperty("Verifies", "SCR-21555839");
    RecordProperty("Description",
                   "Checks that service elements are stored in a fixed-capacity LinearSearchMap within "
                   "ServiceDataStorage. The LinearSearchMap resolves a key via find() by comparing it against the "
                   "stored keys and returns the value corresponding to the provided key and not any other key. "
                   "Therefore, resolving a service element from an EventFqId will never return the wrong storage "
                   "location for the service element. Unlike dynamically allocating map-types, its memory footprint is "
                   "deterministic and can be calculated up-front without a simulation run.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};
    using ActualEventMapType = decltype(unit.events_);

    using ExpectedKeyType = ElementFqId;
    using ExpectedValueType = score::memory::shared::OffsetPtr<void>;
    using ExpectedEventMapType = LinearSearchMap<ExpectedKeyType, ExpectedValueType>;

    static_assert(std::is_same_v<ActualEventMapType, ExpectedEventMapType>, "Event map is not a LinearSearchMap");
}
#endif  // not __linux__

TEST_F(ServiceDataStorageFixture, GetsPidFromUnistdAndStoresItOnConstruction)
{
    // Expecting that getpid will be called
    const pid_t pid{123};
    EXPECT_CALL(lola_runtime_mock_, GetPid()).WillOnce(Return(pid));

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};

    // Then the ServiceDataStorage will contain the returned PID
    EXPECT_EQ(unit.skeleton_pid_, pid);
}

TEST_F(ServiceDataStorageFixture, GetsUidFromRuntimAndStoresItOnConstruction)
{
    const os::MockGuard<os::UnistdMock> unistd_mock{};

    // Expecting that getuid will be called
    const uid_t uid{456};
    EXPECT_CALL(*unistd_mock, getuid()).WillOnce(Return(uid));

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};

    // Then the ServiceDataStorage will contain the returned UID
    EXPECT_EQ(unit.skeleton_uid_, uid);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
