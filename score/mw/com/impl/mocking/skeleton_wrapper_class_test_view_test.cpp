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
#include "score/mw/com/impl/mocking/skeleton_wrapper_class_test_view.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/mocking/skeleton_base_mock.h"
#include "score/mw/com/impl/mocking/skeleton_event_mock.h"
#include "score/mw/com/impl/mocking/skeleton_field_mock.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"

#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <queue>
#include <string_view>
#include <tuple>
#include <utility>

using namespace ::testing;

namespace score::mw::com::impl
{
namespace
{

using TestEventType = std::int32_t;
using TestEventType2 = float;
using TestFieldType = std::uint64_t;

const auto kInstanceSpecifier1 = InstanceSpecifier::Create(std::string{"MyInstanceSpecifier1"}).value();
const auto kInstanceSpecifier2 = InstanceSpecifier::Create(std::string{"MyInstanceSpecifier2"}).value();

const std::string_view kEventName{"SomeEventName"};
const std::string_view kEventName2{"SomeEventName2"};
const std::string_view kFieldName{"SomeFieldName"};

template <typename InterfaceTrait>
class MyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Event<TestEventType> some_event{*this, kEventName};
    typename InterfaceTrait::template Event<TestEventType2> some_event_2{*this, kEventName2};
    typename InterfaceTrait::template Field<TestFieldType> some_field{*this, kFieldName};
};
using MySkeleton = AsSkeleton<MyInterface>;

class SkeletonWrapperTestClassFixture : public ::testing::Test
{
  public:
    ~SkeletonWrapperTestClassFixture()
    {
        ClearCreationResults();
    }

    void AddErrorCodes(const InstanceSpecifier& specifier, std::vector<ComErrc> error_codes)
    {
        std::queue<Result<MySkeleton>> error_results{};

        for (auto error_code : error_codes)
        {
            error_results.push(MakeUnexpected(error_code));
        }
        instance_specifier_creation_results_.insert({specifier, std::move(error_results)});
    }

    void AddErrorCodes(const InstanceIdentifier& identifier, std::vector<ComErrc> error_codes)
    {
        std::queue<Result<MySkeleton>> error_results{};

        for (auto error_code : error_codes)
        {
            error_results.push(MakeUnexpected(error_code));
        }
        instance_identifier_creation_results_.insert({identifier, std::move(error_results)});
    }

    void InjectCreationResults()
    {
        SkeletonWrapperClassTestView<MySkeleton>::InjectCreationResults(
            std::move(instance_specifier_creation_results_), std::move(instance_identifier_creation_results_));
    }

    void ClearCreationResults()
    {
        SkeletonWrapperClassTestView<MySkeleton>::ClearCreationResults();
    }

    InstanceIdentifier instance_identifier_1_{MakeFakeInstanceIdentifier(1U)};
    InstanceIdentifier instance_identifier_2_{MakeFakeInstanceIdentifier(2U)};

    std::unordered_map<InstanceSpecifier, std::queue<Result<MySkeleton>>> instance_specifier_creation_results_{};
    std::unordered_map<InstanceIdentifier, std::queue<Result<MySkeleton>>> instance_identifier_creation_results_{};
};

using SkeletonWrapperTestClassInjectionFixture = SkeletonWrapperTestClassFixture;
TEST_F(SkeletonWrapperTestClassInjectionFixture, CreateDispatchesToInjectedInstanceSpecifierCreationResults)
{
    // Given that multiple creation results were injected for two instance specifiers
    AddErrorCodes(kInstanceSpecifier1, {ComErrc::kBindingFailure, ComErrc::kServiceNotAvailable});
    AddErrorCodes(kInstanceSpecifier2, {ComErrc::kCommunicationStackError});
    InjectCreationResults();

    // When creating the skeletons
    auto result_first_specifier = MySkeleton::Create(kInstanceSpecifier1);
    auto result_second_specifier = MySkeleton::Create(kInstanceSpecifier2);
    auto result_first_specifier_2 = MySkeleton::Create(kInstanceSpecifier1);

    // Then the results correspond to the injected creation results
    ASSERT_FALSE(result_first_specifier.has_value());
    EXPECT_EQ(result_first_specifier.error(), ComErrc::kBindingFailure);

    ASSERT_FALSE(result_first_specifier_2.has_value());
    EXPECT_EQ(result_first_specifier_2.error(), ComErrc::kServiceNotAvailable);

    ASSERT_FALSE(result_second_specifier.has_value());
    EXPECT_EQ(result_second_specifier.error(), ComErrc::kCommunicationStackError);
}

TEST_F(SkeletonWrapperTestClassInjectionFixture, CreateDispatchesToInjectedInstanceIdentifierCreationResults)
{
    // Given that multiple creation results were injected for two instance specifiers
    AddErrorCodes(instance_identifier_1_, {ComErrc::kBindingFailure, ComErrc::kServiceNotAvailable});
    AddErrorCodes(instance_identifier_2_, {ComErrc::kCommunicationStackError});
    InjectCreationResults();

    // When creating the skeletons
    auto result_first_identifier = MySkeleton::Create(instance_identifier_1_);
    auto result_second_identifier = MySkeleton::Create(instance_identifier_2_);
    auto result_first_identifier_2 = MySkeleton::Create(instance_identifier_1_);

    // Then the results correspond to the injected creation results
    ASSERT_FALSE(result_first_identifier.has_value());
    EXPECT_EQ(result_first_identifier.error(), ComErrc::kBindingFailure);

    ASSERT_FALSE(result_first_identifier_2.has_value());
    EXPECT_EQ(result_first_identifier_2.error(), ComErrc::kServiceNotAvailable);

    ASSERT_FALSE(result_second_identifier.has_value());
    EXPECT_EQ(result_second_identifier.error(), ComErrc::kCommunicationStackError);
}

TEST_F(SkeletonWrapperTestClassInjectionFixture,
       CreateDispatchesToInjectedInstanceSpecifierAndIdentifierCreationResults)
{
    // Given that multiple creation results were injected for an instance specifier and an instance identifier
    AddErrorCodes(kInstanceSpecifier1, {ComErrc::kBindingFailure});
    AddErrorCodes(instance_identifier_1_, {ComErrc::kCommunicationStackError});
    InjectCreationResults();

    // When creating the skeletons
    auto result_specifier = MySkeleton::Create(kInstanceSpecifier1);
    auto result_identifier = MySkeleton::Create(instance_identifier_1_);

    // Then the results correspond to the injected creation results
    ASSERT_FALSE(result_specifier.has_value());
    EXPECT_EQ(result_specifier.error(), ComErrc::kBindingFailure);

    ASSERT_FALSE(result_identifier.has_value());
    EXPECT_EQ(result_identifier.error(), ComErrc::kCommunicationStackError);
}

TEST_F(SkeletonWrapperTestClassInjectionFixture, CallingCreateWithSpecifierThatWasNotInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance specifier
    AddErrorCodes(kInstanceSpecifier1, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // When creating a skeleton with a different specifier
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MySkeleton::Create(kInstanceSpecifier2), ".*");
}

TEST_F(SkeletonWrapperTestClassInjectionFixture, CallingCreateWithIdentifierThatWasNotInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance identifier
    AddErrorCodes(instance_identifier_1_, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // When creating a skeleton with a different identifier
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MySkeleton::Create(instance_identifier_2_), ".*");
}

TEST_F(SkeletonWrapperTestClassInjectionFixture, CallingCreateWithSpecifierMoreTimesThanResultWasInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance specifier
    AddErrorCodes(kInstanceSpecifier1, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // and given that a skeleton was created with the specifier once
    score::cpp::ignore = MySkeleton::Create(kInstanceSpecifier1);

    // When creating a skeleton with the specifier again
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MySkeleton::Create(kInstanceSpecifier1), ".*");
}

TEST_F(SkeletonWrapperTestClassInjectionFixture, CallingCreateWithIdentifierMoreTimesThanResultWasInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance identifier
    AddErrorCodes(instance_identifier_1_, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // and given that a skeleton was created with the identifier once
    score::cpp::ignore = MySkeleton::Create(instance_identifier_1_);

    // When creating a skeleton with the identifier again
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MySkeleton::Create(instance_identifier_1_), ".*");
}

using SkeletonWrapperTestClassCreateFixture = SkeletonWrapperTestClassFixture;
TEST_F(SkeletonWrapperTestClassCreateFixture, CreatingMockSkeletonWithAllEventsAndFieldsReturnsSkeleton)
{
    // Given a SkeletonBaseMock and a mock per event and field.
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonEventMock<TestEventType>, NamedSkeletonEventMock<TestEventType2>> events_tuple{kEventName,
                                                                                                           kEventName2};
    std::tuple<NamedSkeletonFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // When creating a mocked skeleton
    auto skeleton = SkeletonWrapperClassTestView<MySkeleton>::Create(skeleton_mock, events_tuple, fields_tuple);

    // Then a skeleton is succesfully constructed (i.e. we don't crash during construction)
}

TEST_F(SkeletonWrapperTestClassCreateFixture, CallingFunctionsOnMockSkeletonDispatchesToMocks)
{
    // Given a SkeletonBaseMock and a mock per event and field.
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonEventMock<TestEventType>, NamedSkeletonEventMock<TestEventType2>> events_tuple{kEventName,
                                                                                                           kEventName2};
    std::tuple<NamedSkeletonFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // and a mocked skeleton
    auto skeleton = SkeletonWrapperClassTestView<MySkeleton>::Create(skeleton_mock, events_tuple, fields_tuple);

    // Expecting that OfferService will be called on the Skeleton mock and Allocate on the event and field mocks
    auto& skeleton_event_mock = std::get<0>(events_tuple).mock;
    auto& skeleton_event_mock_2 = std::get<1>(events_tuple).mock;
    auto& skeleton_field_mock = std::get<0>(fields_tuple).mock;
    EXPECT_CALL(skeleton_mock, OfferService());
    EXPECT_CALL(skeleton_event_mock, Allocate());
    EXPECT_CALL(skeleton_event_mock_2, Allocate());
    EXPECT_CALL(skeleton_field_mock, Allocate());

    // When calling OfferService on the skeleton and Allocate on the events and fields.
    score::cpp::ignore = skeleton.OfferService();

    score::cpp::ignore = skeleton.some_event.Allocate();
    score::cpp::ignore = skeleton.some_event_2.Allocate();
    score::cpp::ignore = skeleton.some_field.Allocate();
}

template <typename InterfaceTrait>
class EventOnlyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Event<TestEventType> some_event{*this, kEventName};
};
using EventOnlySkeleton = AsSkeleton<EventOnlyInterface>;

using SkeletonWrapperTestClassEventsOnlyCreateFixture = SkeletonWrapperTestClassCreateFixture;
TEST_F(SkeletonWrapperTestClassCreateFixture, CreatingMockSkeletonWithAllEventsReturnsSkeleton)
{
    // Given a SkeletonBaseMock and a mock per Event
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonEventMock<TestEventType>> events_tuple{kEventName};

    // When creating a mocked skeleton
    auto skeleton = SkeletonWrapperClassTestView<EventOnlySkeleton>::Create(skeleton_mock, events_tuple);

    // Then a skeleton is succesfully constructed (i.e. we don't crash during construction)
}

TEST_F(SkeletonWrapperTestClassEventsOnlyCreateFixture, CallingFunctionsOnMockSkeletonDispatchesToMocks)
{
    // Given a SkeletonBaseMock and a mock per Event
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonEventMock<TestEventType>> events_tuple{kEventName};

    // and given a mocked skeleton was created
    auto skeleton = SkeletonWrapperClassTestView<EventOnlySkeleton>::Create(skeleton_mock, events_tuple);

    // Expecting that OfferService is called on the skeleton and Allocate on the event
    auto& skeleton_event_mock = (std::get<0>(events_tuple).mock);
    EXPECT_CALL(skeleton_mock, OfferService());
    EXPECT_CALL(skeleton_event_mock, Allocate());

    // When calling OfferService on the skeleton and Allocate on the event
    score::cpp::ignore = skeleton.OfferService();

    std::ignore = skeleton.some_event.Allocate();
}

template <typename InterfaceTrait>
class FieldOnlyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Field<TestFieldType> some_field{*this, kFieldName};
};
using FieldOnlySkeleton = AsSkeleton<FieldOnlyInterface>;

using SkeletonWrapperTestClassFieldsOnlyCreateFixture = SkeletonWrapperTestClassCreateFixture;
TEST_F(SkeletonWrapperTestClassFieldsOnlyCreateFixture, CreatingMockSkeletonWithAllFieldsReturnsSkeleton)
{
    // Given a SkeletonBaseMock and a mock per Field
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // When creating a mocked skeleton
    auto skeleton = SkeletonWrapperClassTestView<FieldOnlySkeleton>::Create(skeleton_mock, fields_tuple);

    // Then a skeleton is successfully constructed (i.e. we don't crash during construction)
}

TEST_F(SkeletonWrapperTestClassFieldsOnlyCreateFixture, CallingFunctionsOnMockSkeletonDispatchesToMocks)
{
    // Given a SkeletonBaseMock and a mock per Field
    SkeletonBaseMock skeleton_mock{};
    std::tuple<NamedSkeletonFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // and given a mocked skeleton was created
    auto skeleton = SkeletonWrapperClassTestView<FieldOnlySkeleton>::Create(skeleton_mock, fields_tuple);

    // Expecting than OfferService is called on the skeleton and Allocate on the field
    auto& skeleton_field_mock = (std::get<0>(fields_tuple).mock);
    EXPECT_CALL(skeleton_mock, OfferService());
    EXPECT_CALL(skeleton_field_mock, Allocate());

    // When calling OfferService on the skeleton and Allocate on the field
    score::cpp::ignore = skeleton.OfferService();

    std::ignore = skeleton.some_field.Allocate();
}

}  // namespace
}  // namespace score::mw::com::impl
