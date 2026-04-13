/*******************************************************************************
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
 *******************************************************************************/

#include "our/name_space/impl_type_somestruct.h"

#include "our/name_space/impl_type_somestruct.h"
#include "our/name_space/someinterface/someinterface_common.h"
#include "our/name_space/someinterface/someinterface_proxy.h"
#include "our/name_space/someinterface/someinterface_skeleton.h"
#include "score/mw/com/types.h"

#include <gtest/gtest.h>

namespace
{

TEST(API, ServiceHeaderFilesExist)
{
    // SWS_CM_01020, SWS_CM_01002, SWS_CM_01004, SWS_CM_01012
    RecordProperty("Verifies", "SCR-5877613, SCR-5877665, SCR-5877702, SCR-5877830");
    RecordProperty("Description", "Checks whether the header files exist with the right name in the right folder.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
}

TEST(API, ServiceNamespace)
{
    // SWS_CM_01005, SWS_CM_01006, SWS_CM_01007, SWS_CM_01001, SWS_CM_10372, SWS_CM_00002, SWS_CM_00004
    RecordProperty("Verifies",
                   "SCR-5877732, SCR-5877760, SCR-5877810, SCR-5877841, SCR-5877860, SCR-5897714, SCR-5897734");
    RecordProperty("Description", "Checks whether the namespace for services is correct.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using Proxy = our::name_space::someinterface::proxy::SomeInterfaceProxy;
    using Skeleton = our::name_space::someinterface::skeleton::SomeInterfaceSkeleton;

    static_assert(!std::is_same<Proxy, Skeleton>::value, "Proxy and skeleton cannot be the same.");
}

TEST(API, EventNamespace)
{
    // SWS_CM_01009, SWS_CM_00003, SWS_CM_00005
    RecordProperty("Verifies", "SCR-5877823, SCR-5897731, SCR-5897785");
    RecordProperty("Description", "Checks whether the namespace for events is correct.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ProxyEvent = our::name_space::someinterface::proxy::events::Value;
    using SkeletonEvent = our::name_space::someinterface::skeleton::events::Value;

    static_assert(!std::is_same<ProxyEvent, SkeletonEvent>::value, "Proxy and skeleton cannot be the same.");
}

TEST(API, TypesHeaderFileExistence)
{
    RecordProperty("Verifies",
                   "SCR-5877911, SCR-5877916, SCR-5878017, SCR-5898898");  // SWS_CM_01013, SWS_CM_01018, SWS_CM_01019
    RecordProperty("Description",
                   "Checks whether the header files exist with the right name in the right folder including the right "
                   "types in the right namespace.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using namespace score::mw::com;

    static_assert(!std::is_empty<InstanceIdentifier>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<FindServiceHandle>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<ServiceHandleContainer<std::uint8_t>>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<FindServiceHandler<std::uint8_t>>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<SamplePtr<std::uint8_t>>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<SampleAllocateePtr<std::uint8_t>>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<EventReceiveHandler>::value, "InstanceIdentifier does not exists");
    static_assert(!std::is_empty<SubscriptionState>::value, "InstanceIdentifier does not exists");
}

TEST(API, ImplementationDataTypeExistence)
{
    // SWS_CM_10373, SWS_CM_10374, SWS_CM_10375, SWS_CM_00421, SWS_CM_00400
    RecordProperty("Verifies", "SCR-5878057, SCR-5878068, SCR-5878095, SCR-5878759, SCR-5879577");
    RecordProperty("Description",
                   "Checks whether the header files exist in the right name in the right folder. Each of the mentioned "
                   "types will then be tested in his respective requirement.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    our::name_space::SomeStruct unit{};
    static_assert(std::is_same<std::uint8_t, decltype(unit.foo)>::value, "Underlying Type not the same");
}

TEST(API, AvoidsDataTypeRedeclaration)
{
    // SWS_CM_00411
    RecordProperty("Verifies", "SCR-5878780");
    RecordProperty("Description", "Checks whether we have ODR violations if a type is used twice.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
}

TEST(API, SupportsPrimitiveCppImplementationTypes)
{
    // SWS_CM_00504, SWS_CM_00402, SWS_CM_00405, SWS_CM_00414
    RecordProperty("Verifies", "SCR-5879657, SCR-5879672, SCR-5879866, SCR-5880470");
    RecordProperty("Description", "Generates necessary types and checks if they are usable (all primitive types)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    our::name_space::CollectionOfTypes unit{};

    static_assert(std::is_same<decltype(unit.a), std::uint8_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.b), std::uint16_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.c), std::uint32_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.d), std::uint64_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.e), std::int8_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.f), std::int16_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.g), std::int32_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.h), std::int64_t>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.i), bool>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.j), float>::value, "Wrong underlying type");
    static_assert(std::is_same<decltype(unit.k), double>::value, "Wrong underlying type");
}

TEST(API, ArrayDeclarationWithOneDimension)
{
    // SWS_CM_00403
    RecordProperty("Verifies", "SCR-5879819");
    RecordProperty("Description",
                   "Checks whether array with one dimension is generated. Inplace are not supported by Franca.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    our::name_space::SomeStruct unit{};

    static_assert(sizeof(unit.access_array) == 5, "Wrong size");
    static_assert(std::is_same<decltype(unit.access_array)::value_type, std::uint8_t>::value, "Wrong underlying type");
}

TEST(API, ArrayDeclarationWithMultiDimArray)
{
    // SWS_CM_00404
    RecordProperty("Verifies", "SCR-5879837");
    RecordProperty("Description",
                   "Checks whether array with multiple dimension is generated. Inplace are not supported by Franca.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(sizeof(our::name_space::MultiDimArray) == 5 * 5, "Wrong size");
    static_assert(std::is_same<our::name_space::MultiDimArray::value_type, our::name_space::SomeArray>::value,
                  "Wrong underlying type");
}

#if !defined(__QNX__) && !defined(__clang__)
// TODO String type not supported due to a bug in the LLVM STL for QNX: [Ticket-54614]
TEST(API, StringIsSupported)
{
    // SWS_CM_00406
    RecordProperty("Verifies", "SCR-5880416");
    RecordProperty("Description", "Checks whether strings are supported");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    our::name_space::SomeStruct unit{};

    static_assert(std::is_same<decltype(unit.access_string), score::memory::shared::String>::value,
                  "Wrong underlying type");
}
#endif

TEST(API, VectorDeclarationWithOneDimension)
{
    // SWS_CM_00407
    RecordProperty("Verifies", "SCR-5880510");
    RecordProperty("Description",
                   "Checks whether vector with one dimension is generated. Inplace are not supported by Franca.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    our::name_space::SomeStruct unit{};

    static_assert(std::is_same<decltype(unit.access_vector)::value_type, std::int32_t>::value, "Wrong underlying type");
}

TEST(API, VectorDeclarationWithMultiDimVector)
{
    // SWS_CM_00408
    RecordProperty("Verifies", "SCR-5880804");
    RecordProperty("Description",
                   "Checks whether vector with multiple dimension is generated. Inplace are not supported by Franca.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<our::name_space::MultiDimVector::value_type, our::name_space::SomeVector>::value,
                  "Wrong underlying type");
}

TEST(API, TypeDefToCustomType)
{
    // SWS_CM_00410
    RecordProperty("Verifies", "SCR-5881043");
    RecordProperty("Description", "Checks whether typedefs are generated correctly");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<our::name_space::MyType, std::uint8_t>::value, "Wrong underlying type");
}

TEST(API, EnumerationGenerated)
{
    // SWS_CM_00424
    RecordProperty("Verifies", "SCR-5881063");
    RecordProperty("Description", "Checks whether enums are generated correctly");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<std::underlying_type<our::name_space::MyEnum>::type, std::uint8_t>::value,
                  "Wrong underlying type.");

    EXPECT_EQ(static_cast<std::uint32_t>(our::name_space::MyEnum::kFirst), 0U);
    EXPECT_EQ(static_cast<std::uint32_t>(our::name_space::MyEnum::kSecond), 1U);
}

}  // namespace
