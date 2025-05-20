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
        // , , ,
        RecordProperty("Verifies", ", , , ");
        RecordProperty("Description", "Checks whether the header files exist with the right name in the right folder.");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");
    }

    TEST(API, ServiceNamespace)
    {
        // , , , , , ,
        RecordProperty("Verifies",
                       ", , , , , , ");
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
        // , ,
        RecordProperty("Verifies", ", , ");
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
                       ", , , "); // , ,
        RecordProperty("Description",
                       "Checks whether the header files exist with the right name in the right folder including the right "
                       "types in the right namespace.");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");

        using namespace bmw::mw::com;

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
        // , , , ,
        RecordProperty("Verifies", ", , , , ");
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
        //
        RecordProperty("Verifies", "");
        RecordProperty("Description", "Checks whether we have ODR violations if a type is used twice.");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");
    }

    TEST(API, SupportsPrimitiveCppImplementationTypes)
    {
        // , , ,
        RecordProperty("Verifies", ", , , ");
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
        //
        RecordProperty("Verifies", "");
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
        //
        RecordProperty("Verifies", "");
        RecordProperty("Description",
                       "Checks whether array with multiple dimension is generated. Inplace are not supported by Franca.");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");

        static_assert(sizeof(our::name_space::MultiDimArray) == 5 * 5, "Wrong size");
        static_assert(std::is_same<our::name_space::MultiDimArray::value_type, our::name_space::SomeArray>::value,
                      "Wrong underlying type");
    }

#ifndef __QNX__
    // TODO String type not supported due to a bug in the LLVM STL for QNX: []
    TEST(API, StringIsSupported)
    {
        //
        RecordProperty("Verifies", "");
        RecordProperty("Description", "Checks whether strings are supported");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");

        our::name_space::SomeStruct unit{};

        static_assert(std::is_same<decltype(unit.access_string), bmw::memory::shared::String>::value,
                      "Wrong underlying type");
    }
#endif

    TEST(API, VectorDeclarationWithOneDimension)
    {
        //
        RecordProperty("Verifies", "");
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
        //
        RecordProperty("Verifies", "");
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
        //
        RecordProperty("Verifies", "");
        RecordProperty("Description", "Checks whether typedefs are generated correctly");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");

        static_assert(std::is_same<our::name_space::MyType, std::uint8_t>::value, "Wrong underlying type");
    }

    TEST(API, EnumerationGenerated)
    {
        //
        RecordProperty("Verifies", "");
        RecordProperty("Description", "Checks whether enums are generated correctly");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");

        static_assert(std::is_same<std::underlying_type<our::name_space::MyEnum>::type, std::uint8_t>::value,
                      "Wrong underlying type.");

        EXPECT_EQ(static_cast<std::uint32_t>(our::name_space::MyEnum::kFirst), 0U);
        EXPECT_EQ(static_cast<std::uint32_t>(our::name_space::MyEnum::kSecond), 1U);
    }

} // namespace
