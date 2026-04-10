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
/// This file contains unit tests for functionality that is unique to fields.
/// There is additional test functionality in the following locations:
///     - score/mw/com/impl/proxy_event_test.cpp contains unit tests which test the event-like functionality of
///     fields.
///     - score/mw/com/impl/bindings/lola/test/proxy_field_component_test.cpp contains component tests which test
///     binding specific field functionality.

#include "score/mw/com/impl/proxy_field.h"

#include "score/mw/com/impl/mocking/proxy_event_mock.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/proxy_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooservice{make_ServiceIdentifierType("foo")};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooservice,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{10U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};
const auto kFieldName{"DummyField"};

using namespace ::testing;

using TestSampleType = std::uint8_t;

// Type list containing all 8 combinations of EnableGet, EnableSet, EnableNotifier
using ProxyFieldTypes = ::testing::Types<ProxyField<TestSampleType, false, false, false>,
                                         ProxyField<TestSampleType, true, false, false>,
                                         ProxyField<TestSampleType, false, true, false>,
                                         ProxyField<TestSampleType, false, false, true>,
                                         ProxyField<TestSampleType, true, true, false>,
                                         ProxyField<TestSampleType, true, false, true>,
                                         ProxyField<TestSampleType, false, true, true>,
                                         ProxyField<TestSampleType, true, true, true>>;

template <typename T>
class ProxyFieldTypedTest : public ::testing::Test
{
};

TYPED_TEST_SUITE(ProxyFieldTypedTest, ProxyFieldTypes);

// Helper trait to detect if a type has a Get() resp. Set() method
template <typename T, typename = void>
struct HasGetMethod : std::false_type
{
};

template <typename T>
struct HasGetMethod<T, std::void_t<decltype(std::declval<T>().template Get<typename T::FieldType>())>> : std::true_type
{
};

template <typename T, typename = void>
struct HasSetMethod : std::false_type
{
};

template <typename T>
struct HasSetMethod<T,
                    std::void_t<decltype(std::declval<T>().template Set<typename T::FieldType>(
                        std::declval<typename T::FieldType&>()))>> : std::true_type
{
};

TYPED_TEST(ProxyFieldTypedTest, NotCopyable)
{
    this->RecordProperty("Verifies", "SCR-17397027");
    this->RecordProperty("Description", "Checks copy semantics for ProxyField");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<TypeParam>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<TypeParam>::value, "Is wrongly copyable");
}

TYPED_TEST(ProxyFieldTypedTest, IsMoveable)
{
    static_assert(std::is_move_constructible<TypeParam>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<TypeParam>::value, "Is not move assignable");
}

TYPED_TEST(ProxyFieldTypedTest, ProxyFieldContainsPublicFieldType)
{
    this->RecordProperty("Verifies", "SCR-17291997");
    this->RecordProperty("Description",
                         "A ProxyField contains a public member type FieldType which denotes the type of the field.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    using CustomFieldType = TestSampleType;
    static_assert(std::is_same<typename TypeParam::FieldType, CustomFieldType>::value, "Incorrect FieldType.");
}

TEST(ProxyFieldTest, ClassTypeDependsOnFieldDataType)
{
    RecordProperty("Verifies", "SCR-29235459");
    RecordProperty("Description", "ProxyFields with different field data types should be different classes.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using FirstProxyFieldType = ProxyField<bool>;
    using SecondProxyFieldType = ProxyField<std::uint16_t>;
    static_assert(!std::is_same_v<FirstProxyFieldType, SecondProxyFieldType>,
                  "Class type does not depend on field data type");
}

TEST(ProxyFieldTest, GetMethodExistsWhenEnabled)
{
    RecordProperty("Verifies", "ToDo - link to requirement");
    RecordProperty("Description", "ProxyFields with enabled getter should contain a Get() method.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ProxyFieldWithGet = ProxyField<TestSampleType, true, false>;

    static_assert(HasGetMethod<ProxyFieldWithGet>::value, "Get() method does not exist when getter is enabled.");
}

TEST(ProxyFieldTest, GetMethodDoesNotExistWhenDisabled)
{
    RecordProperty("Verifies", "ToDo - link to requirement");
    RecordProperty("Description", "ProxyFields with disabled getter should NOT contain a Get() method.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ProxyFieldWithoutGet = ProxyField<TestSampleType, false, false>;

    static_assert(!HasGetMethod<ProxyFieldWithoutGet>::value, "Get() method should not exist when getter is disabled.");
}

TEST(ProxyFieldTest, SetMethodExistsWhenEnabled)
{
    RecordProperty("Verifies", "ToDo - link to requirement");
    RecordProperty("Description", "ProxyFields with enabled setter should contain a Set() method.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ProxyFieldWithSet = ProxyField<TestSampleType, false, true>;

    static_assert(HasSetMethod<ProxyFieldWithSet>::value, "Set() method does not exist when setter is enabled.");
}

TEST(ProxyFieldTest, SetMethodDoesNotExistWhenDisabled)
{
    RecordProperty("Verifies", "ToDo - link to requirement");
    RecordProperty("Description", "ProxyFields with disabled setter should NOT contain a Set() method.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ProxyFieldWithoutSet = ProxyField<TestSampleType, false, false>;

    static_assert(!HasSetMethod<ProxyFieldWithoutSet>::value, "Set() method should not exist when setter is disabled.");
}

TEST(ProxyFieldTest, EventRelatedCallsGetDispatchedToMockProxyEvent)
{
    ProxyBase empty_proxy{std::make_unique<mock_binding::Proxy>(),
                          make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment))};

    ProxyField<TestSampleType, false, false> unit{
        empty_proxy, std::make_unique<StrictMock<mock_binding::ProxyEvent<TestSampleType>>>(), kFieldName};
    ProxyEventMock<TestSampleType> proxy_event_mock{};
    unit.InjectEventMock(proxy_event_mock);

    EXPECT_CALL(proxy_event_mock, GetNewSamples(_, _)).WillOnce(Return(Result<std::size_t>{1U}));
    auto result = unit.GetNewSamples([](SamplePtr<TestSampleType> sample_ptr) {}, 1U);
    ASSERT_TRUE(result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
