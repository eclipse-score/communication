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
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;

// --- Helper Mocks ---
class IBindingRuntimeMock : public IBindingRuntime
{
  public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (ref(&), noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

class GenericSkeletonTest : public ::testing::Test
{
  public:
    GenericSkeletonTest()
    {
        GenericSkeletonEventBindingFactory::mock_ = &generic_skeleton_event_binding_factory_mock_;

        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_mock_));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(ReturnRef(service_discovery_mock_));
        ON_CALL(binding_runtime_mock_, GetBindingType()).WillByDefault(Return(BindingType::kLoLa));
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(ReturnRef(service_discovery_client_mock_));

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Invoke([this](const auto&) {
                auto mock = std::make_unique<NiceMock<mock_binding::Skeleton>>();
                this->skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Result<void>{}));
                return mock;
            }));
    }

    ~GenericSkeletonTest() override
    {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    RuntimeMockGuard runtime_mock_guard_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    NiceMock<GenericSkeletonEventBindingFactoryMock> generic_skeleton_event_binding_factory_mock_{};

    NiceMock<IBindingRuntimeMock> binding_runtime_mock_{};
    NiceMock<ServiceDiscoveryMock> service_discovery_mock_{};
    NiceMock<ServiceDiscoveryClientMock> service_discovery_client_mock_{};

    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};

    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GenericSkeletonTest, CreateWithInstanceSpecifierResolvesIdentifier)
{
    RecordProperty("Description", "Checks that GenericSkeleton resolves the InstanceSpecifier.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a valid string specifier
    auto instance_specifier = InstanceSpecifier::Create(std::string("path/to/my/service")).value();
    auto expected_identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();

    // Expect the Runtime to be asked to resolve it
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(instance_specifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{expected_identifier}));

    // When creating the skeleton
    GenericSkeletonServiceElementInfo params;
    auto result = GenericSkeleton::Create(instance_specifier, params);

    // Then creation succeeds
    ASSERT_TRUE(result.has_value());
}

TEST_F(GenericSkeletonTest, CreateWithUnresolvedInstanceSpecifierFails)
{
    RecordProperty(
        "Description",
        "Checks that GenericSkeleton returns kInstanceIDCouldNotBeResolved when InstanceSpecifier cannot be resolved.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a valid string specifier
    auto instance_specifier = InstanceSpecifier::Create(std::string("path/to/unknown/service")).value();

    // Expect the Runtime to attempt to resolve it, but simulate failure by returning an empty vector
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(instance_specifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{}));

    // When creating the skeleton
    GenericSkeletonServiceElementInfo params;
    auto result = GenericSkeleton::Create(instance_specifier, params);

    // Then creation fails with the expected error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInstanceIDCouldNotBeResolved);
}

TEST_F(GenericSkeletonTest, CreateFailsIfEventBindingCannotBeCreated)
{
    RecordProperty(
        "Description",
        "Checks that creation fails if the GenericSkeletonEventBindingFactory returns an error for any event.");
    RecordProperty("TestType", "Requirements-based test");

    // 1. Given an identifier and configuration with one valid event
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event";

    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, {16, 8}});

    GenericSkeletonServiceElementInfo params;
    params.events = event_storage;

    // 2. Expect the Event Binding Factory to be called, but force it to FAIL
    // We simulate an internal failure by returning MakeUnexpected
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kBindingFailure))));

    // 3. When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // 4. Then creation fails and correctly propagates the kBindingFailure error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonTest, CreateWithEventsInitializesEventBindings)
{
    RecordProperty("Description", "Checks that GenericSkeleton creates bindings for configured events.");
    RecordProperty("TestType", "Requirements-based test");

    // Given configuration for one event
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event";
    const DataTypeMetaInfo meta_info{16, 8};

    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, meta_info});

    GenericSkeletonServiceElementInfo params;
    params.events = event_storage;

    // Expect the Event Factory to be called
    auto MetaMatcher = AllOf(Property(&score::memory::DataTypeSizeInfo::Size, meta_info.size),
                             Property(&score::memory::DataTypeSizeInfo::Alignment, meta_info.alignment));

    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, event_name, MetaMatcher))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then the skeleton contains the event
    ASSERT_TRUE(result.has_value());
    const auto& events = result.value().GetEvents();
    ASSERT_EQ(events.size(), 1);

    EXPECT_NE(events.find(event_name), events.cend());
}

TEST_F(GenericSkeletonTest, CreateWithInvalidDataTypeMetaInfoAlignmentFails)
{
    // Given configuration for one event whose meta-info has an alignment that is not a power of two
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event";
    const DataTypeMetaInfo meta_info{16, 6};

    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, meta_info});

    GenericSkeletonServiceElementInfo params;
    params.events = event_storage;

    // Expecting that the event factory is never be called for invalid meta-info
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, _, _)).Times(0);

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails with kInvalidConfiguration
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidConfiguration);
}

TEST_F(GenericSkeletonTest, CreateWithInvalidDataTypeMetaInfoSizeFails)
{
    // Given configuration for one event whose size is not a multiple of its (valid) alignment
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event";
    const DataTypeMetaInfo meta_info{10, 8};

    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, meta_info});

    GenericSkeletonServiceElementInfo params;
    params.events = event_storage;

    // Expecting that the event factory is never be called for invalid meta-info
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, _, _)).Times(0);

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails with kInvalidConfiguration
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidConfiguration);
}

TEST_F(GenericSkeletonTest, CreateWithDuplicateEventNamesFails)
{
    RecordProperty("Description", "Checks that creating a skeleton with duplicate event names returns an error.");
    RecordProperty("TestType", "Requirements-based test");

    // Given an identifier and configuration with duplicate event names
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event";

    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, {1, 1}});
    event_storage.push_back({event_name, {2, 2}});  // Duplicate

    GenericSkeletonServiceElementInfo params;
    params.events = event_storage;

    // Expecting at least one attempt to create an event binding
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, event_name, _))
        .WillRepeatedly(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails with kServiceElementAlreadyExists
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kServiceElementAlreadyExists);
}

TEST_F(GenericSkeletonTest, CreateFailsIfMainBindingCannotBeCreated)
{
    RecordProperty("Description", "Checks that creation fails if the main SkeletonBinding factory returns null.");
    RecordProperty("TestType", "Requirements-based test");

    // Given the binding factory returns nullptr
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_)).WillOnce(Return(ByMove(nullptr)));

    GenericSkeletonServiceElementInfo params;

    // When creating the skeleton
    auto result =
        GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier(), params);

    // Then creation fails with kBindingFailure
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonTest, OfferServicePropagatesToBindingAndDiscovery)
{
    RecordProperty("Description",
                   "Checks that OfferService calls PrepareOffer on the binding and notifies ServiceDiscovery.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a created skeleton
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();
    auto skeleton = GenericSkeleton::Create(identifier, {}).value();

    // Expecting OfferService to trigger binding and discovery
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodHandlersRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(service_discovery_mock_, OfferService(identifier)).WillOnce(Return(score::Result<void>{}));

    // When offering service
    auto result = skeleton.OfferService();

    // Then it succeeds
    ASSERT_TRUE(result.has_value());
}

TEST_F(GenericSkeletonTest, StopOfferServicePropagatesToBindingAndDiscovery)
{
    RecordProperty("Description", "Checks that StopOfferService calls PrepareStopOffer and notifies ServiceDiscovery.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a created skeleton
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();
    auto skeleton = GenericSkeleton::Create(identifier, {}).value();

    // And given the service is already Offered
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodHandlersRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(service_discovery_mock_, OfferService(identifier)).WillOnce(Return(score::Result<void>{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    // Expecting StopOffer to trigger binding and discovery
    EXPECT_CALL(*skeleton_binding_mock_, PrepareStopOffer(_));
    EXPECT_CALL(service_discovery_mock_, StopOfferService(identifier));

    // When stopping offer
    skeleton.StopOfferService();

    // Then (Verified by mock expectations)
}

TEST_F(GenericSkeletonTest, OfferServiceReturnsErrorIfBindingFails)
{
    RecordProperty("Description", "Checks that OfferService returns an error if the binding's PrepareOffer fails.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a created skeleton
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();
    auto skeleton = GenericSkeleton::Create(identifier, {}).value();

    // Expecting Binding to fail
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodHandlersRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // Expecting ServiceDiscovery NOT to be called
    EXPECT_CALL(service_discovery_mock_, OfferService(_)).Times(0);

    // When offering service
    auto result = skeleton.OfferService();

    // Then it fails with kBindingFailure
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonTest, CreateWithFieldsInitializesFieldBindings)
{
    RecordProperty("Description", "Checks that GenericSkeleton creates bindings for configured fields.");
    RecordProperty("TestType", "Requirements-based test");

    // Given configuration for one field
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField();
    const std::string field_name = "test_field";
    const DataTypeMetaInfo meta_info{16, 8};

    std::vector<FieldInfo> field_storage;
    field_storage.push_back({field_name, meta_info, false, false, true});

    GenericSkeletonServiceElementInfo params;
    params.fields = field_storage;

    // Expect the Event Factory to be called (since Fields use Event bindings for notifiers)
    auto MetaMatcher = AllOf(Property(&score::memory::DataTypeSizeInfo::Size, meta_info.size),
                             Property(&score::memory::DataTypeSizeInfo::Alignment, meta_info.alignment));

    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, field_name, MetaMatcher))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then the skeleton contains the field
    ASSERT_TRUE(result.has_value());
    const auto& fields = result.value().GetFields();
    ASSERT_EQ(fields.size(), 1);

    EXPECT_NE(fields.find(field_name), fields.cend());
}

TEST_F(GenericSkeletonTest, CreateWithDuplicateFieldNamesFails)
{
    RecordProperty("Description", "Checks that creating a skeleton with duplicate field names returns an error.");
    RecordProperty("TestType", "Requirements-based test");

    // Given an identifier and configuration with duplicate field names
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField();
    const std::string field_name = "test_field";

    std::vector<FieldInfo> field_storage;
    field_storage.push_back({field_name, {1, 1}, false, false, true});
    field_storage.push_back({field_name, {2, 2}, false, false, true});  // Duplicate

    GenericSkeletonServiceElementInfo params;
    params.fields = field_storage;

    // Expecting at least one attempt to create an event binding
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, field_name, _))
        .WillRepeatedly(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails with kServiceElementAlreadyExists
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kServiceElementAlreadyExists);
}

TEST_F(GenericSkeletonTest, CreateFailsIfFieldBindingCannotBeCreated)
{
    RecordProperty(
        "Description",
        "Checks that creation fails if the GenericSkeletonEventBindingFactory returns an error for any field.");
    RecordProperty("TestType", "Requirements-based test");

    // Given an identifier and configuration with one valid field
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField();
    const std::string field_name = "test_field";

    std::vector<FieldInfo> field_storage;
    field_storage.push_back({field_name, {16, 8}, false, false, true});

    GenericSkeletonServiceElementInfo params;
    params.fields = field_storage;

    // Expect the Event Binding Factory to be called, but force it to FAIL
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kBindingFailure))));

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails and correctly propagates the kBindingFailure error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonTest, GetFieldsReturnsCorrectMapOfServiceElements)
{
    RecordProperty("Description", "Checks that GetFields provides access to all configured fields by name.");
    RecordProperty("TestType", "Requirements-based test");

    // 1. Prepare a deployment configuration containing three specific field names
    LolaServiceInstanceDeployment::FieldInstanceMapping field_mapping;
    field_mapping["temperature"] = {LolaEventInstanceDeployment{1, 1, 1, true, 0}, false, false};
    field_mapping["pressure"] = {LolaEventInstanceDeployment{2, 1, 1, true, 0}, false, false};
    field_mapping["status"] = {LolaEventInstanceDeployment{3, 1, 1, true, 0}, false, false};

    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(field_mapping);

    // 2. Setup configuration for the three requested fields
    std::vector<FieldInfo> field_storage;
    field_storage.push_back({"temperature", {4, 4}, true, false, true});
    field_storage.push_back({"pressure", {4, 4}, false, true, false});
    field_storage.push_back({"status", {1, 1}, true, true, true});

    GenericSkeletonServiceElementInfo params;
    params.fields = field_storage;

    // 3. Expect the binding factory to be called for each field
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([](auto&, auto&, auto&) {
            return std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
        }));

    // 4. When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);
    ASSERT_TRUE(result.has_value());
    auto& skeleton = result.value();

    // 5. Then GetFields returns all of them correctly
    const auto& fields = skeleton.GetFields();
    EXPECT_EQ(fields.size(), 3);
    EXPECT_NE(fields.find("temperature"), fields.cend());
    EXPECT_NE(fields.find("pressure"), fields.cend());
    EXPECT_NE(fields.find("status"), fields.cend());
}

TEST_F(GenericSkeletonTest, CreateFailsIfFieldNameNotFoundInConfiguration)
{
    RecordProperty("Description",
                   "Checks that creation fails if a field name is not found in the deployment configuration.");
    RecordProperty("TestType", "Requirements-based test");

    // Given an identifier with a default deployment (usually contains "test_field")
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField();
    const std::string unknown_field_name = "unknown_field";

    // When requesting a field that is NOT in that deployment
    std::vector<FieldInfo> field_storage;
    field_storage.push_back({unknown_field_name, {1, 1}, false, false, true});

    GenericSkeletonServiceElementInfo params;
    params.fields = field_storage;

    // The factory should not be called because name resolution fails first
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_, Create(_, _, _)).Times(0);

    // When creating the skeleton
    auto result = GenericSkeleton::Create(identifier, params);

    // Then creation fails with kBindingFailure
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl
