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
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/com_error.h" 

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
using ::testing::Return;
using ::testing::ReturnRef;

// --- Helper Mocks ---
class IBindingRuntimeMock : public IBindingRuntime
{
  public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
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
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Blank{}));
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
    GenericSkeletonCreateParams params;
    auto result = GenericSkeleton::Create(instance_specifier, params);

    // Then creation succeeds
    ASSERT_TRUE(result.has_value());
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
    
    GenericSkeletonCreateParams params;
    params.events = event_storage; 

    // Expect the Event Factory to be called
    auto MetaMatcher = AllOf(
        Field(&DataTypeMetaInfo::size, meta_info.size),
        Field(&DataTypeMetaInfo::alignment, meta_info.alignment)
    );

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


TEST_F(GenericSkeletonTest, CreateWithDuplicateEventNamesFails)
{
    RecordProperty("Description", "Checks that creating a skeleton with duplicate event names returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    
    // Given an identifier and configuration with duplicate event names
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent();
    const std::string event_name = "test_event"; 
    
    std::vector<EventInfo> event_storage;
    event_storage.push_back({event_name, {1, 1}});
    event_storage.push_back({event_name, {2, 2}}); // Duplicate

    GenericSkeletonCreateParams params;
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
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
        .WillOnce(Return(ByMove(nullptr)));

    GenericSkeletonCreateParams params;

    // When creating the skeleton
    auto result = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier(), params);

    // Then creation fails with kBindingFailure
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}


TEST_F(GenericSkeletonTest, OfferServicePropagatesToBindingAndDiscovery)
{
    RecordProperty("Description", "Checks that OfferService calls PrepareOffer on the binding and notifies ServiceDiscovery.");
    RecordProperty("TestType", "Requirements-based test");
    
    // Given a created skeleton
    auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();
    auto skeleton = GenericSkeleton::Create(identifier, {}).value();

    // Expecting OfferService to trigger binding and discovery
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Blank{}));
    EXPECT_CALL(service_discovery_mock_, OfferService(identifier)).WillOnce(Return(score::Blank{}));

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
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Blank{}));
    EXPECT_CALL(service_discovery_mock_, OfferService(identifier)).WillOnce(Return(score::Blank{}));
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

} // namespace
} // namespace score::mw::com::impl