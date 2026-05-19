/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/impl/generic_skeleton_field.h"
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include <variant>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;

using ::testing::Return;
using ::testing::ReturnRef;

class IBindingRuntimeMock : public IBindingRuntime
{
  public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

class GenericSkeletonFieldTest : public ::testing::Test
{
  public:
    GenericSkeletonFieldTest()
    {
        GenericSkeletonEventBindingFactory::mock_ = &generic_event_binding_factory_mock_;

        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_mock_));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(ReturnRef(service_discovery_mock_));
        ON_CALL(binding_runtime_mock_, GetBindingType()).WillByDefault(Return(BindingType::kLoLa));
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(ReturnRef(service_discovery_client_mock_));
        ON_CALL(service_discovery_mock_, OfferService(_)).WillByDefault(Return(score::Result<void>{}));

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Invoke([this](const auto&) {
                auto mock = std::make_unique<NiceMock<mock_binding::Skeleton>>();
                this->skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Result<void>{}));
                return mock;
            }));
    }

    ~GenericSkeletonFieldTest() override
    {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    std::string GetConfiguredFieldName()
    {
        auto identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField();
        const auto& type_depl = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
        auto* lola_type = std::get_if<LolaServiceTypeDeployment>(&type_depl.binding_info_);
        if (lola_type && !lola_type->fields_.empty())
        {
            return lola_type->fields_.begin()->first;
        }
        return "test_field";
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

TEST_F(GenericSkeletonFieldTest, AllocateBeforeOfferReturnsError)
{
    RecordProperty("Description", "Checks that calling Allocate() before OfferService() returns an error.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0};
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, true, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);

    auto alloc_result = field->Allocate();

    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonFieldTest, AllocateFailsWithoutNotifier)
{
    RecordProperty("Description", "Checks that calling Allocate() without a notifier configured returns an error.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0};
    // has_notifier set to false
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, false, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);

    auto alloc_result = field->Allocate();

    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonFieldTest, GettersAndSettersReturnError)
{
    RecordProperty("Description", "Checks that Get/Set handlers are currently WIP and correctly return an error.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0};
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, true, true, false, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);

    auto get_result = field->RegisterGetHandler([]() {
        return std::vector<uint8_t>{};
    });
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error(), ComErrc::kCouldNotExecute);

    auto set_result = field->RegisterSetHandler([](auto) {
        return std::vector<uint8_t>{};
    });
    EXPECT_FALSE(set_result.has_value());
    EXPECT_EQ(set_result.error(), ComErrc::kCouldNotExecute);
}

TEST_F(GenericSkeletonFieldTest, UpdateBeforeOfferCachesValue)
{
    RecordProperty("Description",
                   "Checks that updating a field before offering the service caches the value without sending it.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, true, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    auto mock_event = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();

    // Expect NO Send or Allocate to be called since the service is not offered yet.
    EXPECT_CALL(*mock_event, Allocate()).Times(0);
    EXPECT_CALL(*mock_event, Send(_)).Times(0);

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event))));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);

    ASSERT_TRUE(skeleton_result.has_value());

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);

    // Explicitly update the field before calling OfferService
    std::vector<uint8_t> explicit_update_val{0x11, 0x22, 0x33};
    auto update_res = field->Update(explicit_update_val);

    // The update should succeed and cache the value without making any binding calls (Allocate/Send).
    EXPECT_TRUE(update_res.has_value());
}

TEST_F(GenericSkeletonFieldTest, DoDeferredUpdatePushesCachedValueOnOffer)
{
    RecordProperty("Description",
                   "Checks that offering the service triggers DoDeferredUpdate, which allocates and sends the cached "
                   "initial value.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, true, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    auto mock_event = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_ptr = mock_event.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event))));

    // Setup mock event expectations BEFORE the skeleton is even created
    EXPECT_CALL(*mock_event_ptr, PrepareOffer()).WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(*mock_event_ptr, GetSizeInfo()).WillRepeatedly(Return(std::make_pair<size_t, size_t>(16, 8)));

    std::vector<uint8_t> dummy_memory(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc{dummy_memory.data(), [](void*) {}};
    EXPECT_CALL(*mock_event_ptr, Allocate()).WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc)))));
    EXPECT_CALL(*mock_event_ptr, Send(_)).WillOnce(Return(score::Result<void>{}));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    // skeleton_binding_mock_ is initialized inside Create(), so its expectations must come after
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));

    // When offering service, DoDeferredUpdate is executed
    auto offer_res = skeleton_result.value().OfferService();
    EXPECT_TRUE(offer_res.has_value());
}

TEST_F(GenericSkeletonFieldTest, UpdateAfterOfferAllocatesAndSends)
{
    RecordProperty("Description",
                   "Checks that updating a field after offering the service successfully allocates and sends.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, true, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    auto mock_event = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_ptr = mock_event.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event))));

    // Setup expectations for initial OfferService caching BEFORE creation
    EXPECT_CALL(*mock_event_ptr, PrepareOffer()).WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(*mock_event_ptr, GetSizeInfo()).WillRepeatedly(Return(std::make_pair<size_t, size_t>(16, 8)));

    std::vector<uint8_t> dummy_memory1(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc1{dummy_memory1.data(), [](void*) {}};
    EXPECT_CALL(*mock_event_ptr, Allocate()).WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc1)))));
    EXPECT_CALL(*mock_event_ptr, Send(_)).WillOnce(Return(score::Result<void>{}));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));

    // Offer the service (Handle initial value allocation/send)
    ASSERT_TRUE(skeleton_result.value().OfferService().has_value());

    // When calling Update after OfferService
    std::vector<uint8_t> new_val{0xCC, 0xDD};
    std::vector<uint8_t> dummy_memory2(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc2{dummy_memory2.data(), [](void*) {}};

    // Expect a NEW allocation and send
    EXPECT_CALL(*mock_event_ptr, Allocate()).WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc2)))));
    EXPECT_CALL(*mock_event_ptr, Send(_)).WillOnce(Return(score::Result<void>{}));

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);
    auto update_res = field->Update(new_val);

    EXPECT_TRUE(update_res.has_value());
}

TEST_F(GenericSkeletonFieldTest, UpdateWithoutNotifierReturnsSuccessAndDoesNotSend)
{
    RecordProperty(
        "Description",
        "Checks that updating a field without a notifier returns success immediately without allocating or sending.");
    RecordProperty("TestType", "Requirements-based test");

    const std::string field_name = GetConfiguredFieldName();
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    // has_notifier = false
    std::vector<FieldInfo> field_storage{{field_name, {16, 8}, false, false, false, init_val}};
    GenericSkeletonServiceElementInfo create_params;
    create_params.fields = field_storage;

    auto mock_event = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_ptr = mock_event.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
        .WillOnce(Return(ByMove(std::move(mock_event))));

    auto skeleton_result = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    // Offer the service
    EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodsRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_event_ptr, PrepareOffer()).WillOnce(Return(score::Result<void>{}));
    ASSERT_TRUE(skeleton_result.value().OfferService().has_value());

    // When calling Update, expect NO Allocate and NO Send since has_notifier_ is false
    EXPECT_CALL(*mock_event_ptr, Allocate()).Times(0);
    EXPECT_CALL(*mock_event_ptr, Send(_)).Times(0);

    auto& skeleton = skeleton_result.value();
    auto* field = const_cast<GenericSkeletonField*>(&skeleton.GetFields().find(field_name)->second);

    std::vector<uint8_t> new_val{0xCC, 0xDD};
    auto update_res = field->Update(new_val);

    EXPECT_TRUE(update_res.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
