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
#include "score/mw/com/impl/generic_skeleton_event.h"

#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h" 

#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h" 
#include "score/mw/com/impl/com_error.h"

#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>
#include <string>

namespace score::mw::com::impl {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::Field;
using ::testing::AllOf;

// --- Helper Mocks ---
class IBindingRuntimeMock : public IBindingRuntime {
public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

class GenericSkeletonEventTest : public ::testing::Test {
public:
    GenericSkeletonEventTest() {
        GenericSkeletonEventBindingFactory::mock_ = &generic_event_binding_factory_mock_;

        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_mock_));
        
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
             .WillByDefault(ReturnRef(service_discovery_mock_));

        ON_CALL(binding_runtime_mock_, GetBindingType())
            .WillByDefault(Return(BindingType::kLoLa));
            
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(ReturnRef(service_discovery_client_mock_));

        ON_CALL(service_discovery_mock_, OfferService(_)).WillByDefault(Return(score::Blank{}));

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Invoke([this](const auto&) {
                auto mock = std::make_unique<NiceMock<mock_binding::Skeleton>>();
                this->skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Blank{}));
                return mock;
            }));
    }

    ~GenericSkeletonEventTest() override {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    // Mocks
    NiceMock<GenericSkeletonEventBindingFactoryMock> generic_event_binding_factory_mock_;
    RuntimeMockGuard runtime_mock_guard_{};
    NiceMock<IBindingRuntimeMock> binding_runtime_mock_{};
    NiceMock<ServiceDiscoveryMock> service_discovery_mock_{};
    NiceMock<ServiceDiscoveryClientMock> service_discovery_client_mock_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    
    // Pointers and Helpers
    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_;
};

TEST_F(GenericSkeletonEventTest, AllocateBeforeOfferReturnsError)
{
    RecordProperty("Description", "Checks that calling Allocate() before OfferService() returns kNotOffered.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton created with one event "test_event"
    const DataTypeMetaInfo size_info{16, 8};
    const std::string event_name = "test_event"; 
    
    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, size_info});
    create_params.events = events;

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    
    // And given the event instance
    auto& skeleton = skeleton_result.value();
    const auto& events_map = skeleton.GetEvents();
    auto it = events_map.find(event_name);
    ASSERT_NE(it, events_map.cend());
    
    auto* event = const_cast<GenericSkeletonEvent*>(&it->second); 

    // When calling Allocate() before OfferService()
    auto alloc_result = event->Allocate();

    // Then it fails with kNotOffered
    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kNotOffered);
}

TEST_F(GenericSkeletonEventTest, SendBeforeOfferReturnsError)
{
    RecordProperty("Description", "Checks that calling Send() before OfferService() returns kNotOffered.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton created with one event "test_event"
    const std::string event_name = "test_event";
    
    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, {16, 8}});
    create_params.events = events;

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    
    auto& skeleton = skeleton_result.value();

    auto* event = const_cast<GenericSkeletonEvent*>(&skeleton.GetEvents().find(event_name)->second);

    // And a valid sample to send
    mock_binding::SampleAllocateePtr<void> dummy_sample{nullptr, [](void*){}};
    
    // When calling Send() before OfferService()
    auto send_result = event->Send(MakeSampleAllocateePtr(std::move(dummy_sample)));
    
    // Then it fails with kNotOffered
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kNotOffered);
}

TEST_F(GenericSkeletonEventTest, AllocateAndSendDispatchesToBindingAfterOffer)
{
    RecordProperty("Description", "Checks that Allocate and Send dispatch to the binding when the service is offered.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton configured with an event binding mock
    const std::string event_name = "test_event";
    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));

    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, {16, 8}});
    create_params.events = events;

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    auto& skeleton = skeleton_result.value();
    
    auto* event = const_cast<GenericSkeletonEvent*>(&skeleton.GetEvents().find(event_name)->second);

    // And Given the service is Offered
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_event_binding_ptr, PrepareOffer()).WillOnce(Return(score::Blank{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    // When calling Allocate()
    mock_binding::SampleAllocateePtr<void> dummy_alloc{nullptr, [](void*){}};
    EXPECT_CALL(*mock_event_binding_ptr, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc)))));

    auto alloc_result = event->Allocate();
    ASSERT_TRUE(alloc_result.has_value());

    // And When calling Send() with the allocated sample
    EXPECT_CALL(*mock_event_binding_ptr, Send(_)).WillOnce(Return(score::Blank{}));
  
    auto send_result = event->Send(std::move(alloc_result.value()));
    
    // Then both operations succeed
    ASSERT_TRUE(send_result.has_value());
}

TEST_F(GenericSkeletonEventTest, AllocateReturnsErrorWhenBindingFails)
{
    RecordProperty("Description", "Checks that Allocate returns kSampleAllocationFailure if the binding allocation fails.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton configured with an event binding mock
    const std::string event_name = "test_event";
    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));

    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, {16, 8}});
    create_params.events = events;

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    auto& skeleton = skeleton_result.value();
    auto* event = const_cast<GenericSkeletonEvent*>(&skeleton.GetEvents().find(event_name)->second);

    // And Given the service is Offered
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_event_binding_ptr, PrepareOffer()).WillOnce(Return(score::Blank{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    // Expect the binding to fail allocation
    EXPECT_CALL(*mock_event_binding_ptr, Allocate())
        .WillOnce(Return(MakeUnexpected(ComErrc::kSampleAllocationFailure)));

    // When calling Allocate()
    auto alloc_result = event->Allocate();
    
    // Then it fails with kSampleAllocationFailure
    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kSampleAllocationFailure);
}


TEST_F(GenericSkeletonEventTest, SendReturnsErrorWhenBindingFails)
{
    RecordProperty("Description", "Checks that Send returns kBindingFailure if the binding send fails.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton configured with an event binding mock
    const std::string event_name = "test_event";
    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));

    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, {16, 8}});
    create_params.events = events;

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    auto& skeleton = skeleton_result.value();
    auto* event = const_cast<GenericSkeletonEvent*>(&skeleton.GetEvents().find(event_name)->second);

    // And Given the service is Offered
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_event_binding_ptr, PrepareOffer()).WillOnce(Return(score::Blank{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    // Expect the binding to fail sending
    EXPECT_CALL(*mock_event_binding_ptr, Send(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // When calling Send() with a dummy sample
    mock_binding::SampleAllocateePtr<void> dummy_alloc{nullptr, [](void*){}};
    auto send_result = event->Send(MakeSampleAllocateePtr(std::move(dummy_alloc)));
    
    // Then it fails with kBindingFailure
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonEventTest, GetSizeInfoDispatchesToBinding)
{
    RecordProperty("Description", "Checks that GetSizeInfo returns the correct DataTypeMetaInfo from the binding.");
    RecordProperty("TestType", "Requirements-based test");

    // Given a skeleton configured with an event binding mock
    const std::string event_name = "test_event";
    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, event_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));

    GenericSkeletonCreateParams create_params;
    std::vector<EventInfo> events;
    events.push_back({event_name, {16, 8}}); // Original creation info
    create_params.events = events;

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(),
        create_params
    );
    ASSERT_TRUE(skeleton_result.has_value());
    auto& skeleton = skeleton_result.value();
    auto* event = const_cast<GenericSkeletonEvent*>(&skeleton.GetEvents().find(event_name)->second);

    // Expect the binding to return specific size info
    std::pair<std::size_t, std::uint8_t> expected_size_info{32, 16};
    EXPECT_CALL(*mock_event_binding_ptr, GetSizeInfo())
        .WillOnce(Return(expected_size_info));

    // When calling GetSizeInfo
    auto result_info = event->GetSizeInfo();

    // Then it matches the binding's return values
    EXPECT_EQ(result_info.size, expected_size_info.first);
    EXPECT_EQ(result_info.alignment, expected_size_info.second);
}

} // namespace
} // namespace score::mw::com::impl