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

#include "score/memory/shared/map.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(ServiceDataStorageTest, GenericProxyEventMetaInfoIsStoredInServiceDataStorage)
{
    RecordProperty("Verifies", "SCR-32391303");
    RecordProperty("Description",
                   "Checks that the EventMataInfo is stored within ServiceDataStorage. Another test checks that "
                   "ServiceDataStorage is read-only.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same_v<score::memory::shared::Map<ElementFqId, EventMetaInfo>,
                                 decltype(ServiceDataStorage::events_metainfo_)>,
                  "ServiceDataStorage does not contain a map of EventMetaInfo.");
}

// When compiling for linux, we use a boost map. Since the tests for satisfying requirements must be on qnx, we only
// check the service element event map type on qnx.
#if !defined(__linux__)
TEST(ServiceDataStorageTest, ServiceElementsAreIndexedUsingElementFqId)
{
    RecordProperty("Verifies", "SCR-21555839");
    RecordProperty("Description",
                   "Checks that service elements are stored in a std::map within ServiceDataStorage. A std::map is "
                   "provided by the standard library which is ASIL-B certified. The standard requires that searching "
                   "for elements e.g. via find() will return the value corresponding to the provided key and not any "
                   "other key. Therefore, resolving a service element from an EventFqId will never return the wrong "
                   "storage location for the service element.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const ServiceDataStorage unit{nullptr};
    using ActualEventMapType = decltype(unit.events_);

    using ExpectedKeyType = ElementFqId;
    using ExpectedValueType = score::memory::shared::OffsetPtr<void>;
    using ExpectedEventMapType = std::map<ExpectedKeyType,
                                          ExpectedValueType,
                                          std::less<ExpectedKeyType>,
                                          std::scoped_allocator_adaptor<memory::shared::PolymorphicOffsetPtrAllocator<
                                              typename std::map<ExpectedKeyType, ExpectedValueType>::value_type>>>;

    static_assert(std::is_same_v<ActualEventMapType, ExpectedEventMapType>, "Event map is not a std::map");
}
#endif  // not __linux__

}  // namespace
}  // namespace score::mw::com::impl::lola
