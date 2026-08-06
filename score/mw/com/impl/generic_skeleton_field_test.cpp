/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0
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
#include "score/mw/com/impl/i_binding_runtime.h"
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
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
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
                ON_CALL(*mock, VerifyAllMethodHandlersRegistered()).WillByDefault(Return(true));
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Result<void>{}));
                return mock;
            }));
    }

    ~GenericSkeletonFieldTest() override
    {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    /// \brief Creates a GenericSkeleton with one field
    GenericSkeletonFieldTest& GivenAGenericSkeletonWithOneField(const std::string& field_name = "test_field",
                                                                DataTypeMetaInfo size_info = {16, 8},
                                                                bool has_getter = false,
                                                                bool has_setter = false,
                                                                bool has_notifier = true)
    {
        auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
        mock_event_binding_ptr_ = mock_event_binding.get();

        EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, field_name, _))
            .WillOnce(Return(ByMove(std::move(mock_event_binding))));

        GenericSkeletonServiceElementInfo create_params;
        std::vector<FieldInfo> field_storage{{field_name, size_info, has_getter, has_setter, has_notifier}};
        create_params.fields = field_storage;

        auto skeleton_result = GenericSkeleton::Create(
            dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithField(), create_params);
        EXPECT_TRUE(skeleton_result.has_value());

        skeleton_ = std::make_unique<GenericSkeleton>(std::move(skeleton_result.value()));
        auto it = skeleton_->GetFields().find(field_name);
        EXPECT_NE(it, skeleton_->GetFields().cend());
        field_ = &it->second;

        return *this;
    }

    /// \brief Offers the skeleton service
    GenericSkeletonFieldTest& OfferSkeletonService()
    {
        EXPECT_CALL(*skeleton_binding_mock_, VerifyAllMethodHandlersRegistered()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_event_binding_ptr_, PrepareOffer()).WillOnce(Return(score::Result<void>{}));
        EXPECT_CALL(*mock_event_binding_ptr_, GetSizeInfo())
            .WillRepeatedly(Return(std::make_pair<size_t, size_t>(16, 8)));

        const auto offer_result = skeleton_->OfferService();
        EXPECT_TRUE(offer_result.has_value());
        return *this;
    }

  protected:
    std::unique_ptr<GenericSkeleton> skeleton_;
    GenericSkeletonField* field_{nullptr};
    mock_binding::GenericSkeletonEvent* mock_event_binding_ptr_{nullptr};

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

    // GIVEN: A skeleton created with one field "test_field"
    this->GivenAGenericSkeletonWithOneField();

    // WHEN: Calling Allocate() before OfferService()
    auto alloc_result = field_->Allocate();

    // THEN: It fails with kBindingFailure
    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonFieldTest, GettersAndSettersReturnError)
{
    RecordProperty("Description", "Checks that Get/Set handlers are currently WIP and correctly return an error.");
    RecordProperty("TestType", "Requirements-based test");

    // GIVEN: A skeleton created with one field that has getter and setter enabled
    this->GivenAGenericSkeletonWithOneField("test_field", {16, 8}, true, true, false);

    // WHEN: Attempting to register a set handler
    auto set_result = field_->RegisterSetHandler([](score::cpp::span<uint8_t>) {});

    // THEN: It fails with kCouldNotExecute (WIP)
    ASSERT_FALSE(set_result.has_value());
    EXPECT_EQ(set_result.error(), ComErrc::kCouldNotExecute);
}

TEST_F(GenericSkeletonFieldTest, UpdateBeforeOfferCachesValue)
{
    RecordProperty("Description",
                   "Checks that updating a field before offering the service caches the value without sending it.");
    RecordProperty("TestType", "Requirements-based test");

    // GIVEN: A skeleton created with one field
    this->GivenAGenericSkeletonWithOneField();

    // WHEN: Updating the field before calling OfferService
    std::vector<uint8_t> update_val{0x11, 0x22, 0x33};
    auto update_res = field_->Update(update_val);

    // THEN: The update succeeds (value is cached internally)
    EXPECT_TRUE(update_res.has_value());
}

TEST_F(GenericSkeletonFieldTest, DoDeferredUpdatePushesCachedValueOnOffer)
{
    RecordProperty("Description",
                   "Checks that offering the service triggers DoDeferredUpdate, which allocates and sends the cached "
                   "initial value.");
    RecordProperty("TestType", "Requirements-based test");

    // GIVEN: A skeleton created with one field
    this->GivenAGenericSkeletonWithOneField();

    // AND: An initial value is set before offering
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    static_cast<void>(field_->Update(init_val));

    // EXPECT: Allocation and Send to be triggered during OfferService()
    std::vector<uint8_t> dummy_memory(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc{dummy_memory.data(), [](void*) {}};
    EXPECT_CALL(*mock_event_binding_ptr_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc)))));
    EXPECT_CALL(*mock_event_binding_ptr_, Send(_)).WillOnce(Return(score::Result<void>{}));

    // WHEN: Offering the service
    this->OfferSkeletonService();

    // THEN: The deferred update is executed (verified by mock expectations)
}

TEST_F(GenericSkeletonFieldTest, UpdateAfterOfferAllocatesAndSends)
{
    RecordProperty("Description",
                   "Checks that updating a field after offering the service successfully allocates and sends.");
    RecordProperty("TestType", "Requirements-based test");

    // GIVEN: An offered skeleton service
    GivenAGenericSkeletonWithOneField();

    // Set initial value manually before offering
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    static_cast<void>(field_->Update(init_val));

    // EXPECT: Initial value allocation and send to the binding during OfferService
    std::vector<uint8_t> dummy_memory1(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc1{dummy_memory1.data(), [](void*) {}};
    EXPECT_CALL(*mock_event_binding_ptr_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc1)))));
    EXPECT_CALL(*mock_event_binding_ptr_, Send(_)).WillOnce(Return(score::Result<void>{}));

    OfferSkeletonService();

    // WHEN: Calling Update after OfferService
    std::vector<uint8_t> new_val{0xCC, 0xDD};
    std::vector<uint8_t> dummy_memory(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc{dummy_memory.data(), [](void*) {}};

    // EXPECT: A new allocation and send to the binding
    EXPECT_CALL(*mock_event_binding_ptr_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc)))));
    EXPECT_CALL(*mock_event_binding_ptr_, Send(_)).WillOnce(Return(score::Result<void>{}));

    auto update_res = field_->Update(new_val);

    // THEN: The update succeeds
    EXPECT_TRUE(update_res.has_value());
}

TEST_F(GenericSkeletonFieldTest, UpdateWithoutNotifierSendsToBinding)
{
    RecordProperty("Description",
                   "Checks that updating a field without a notifier successfully forwards the value to the binding.");
    RecordProperty("TestType", "Requirements-based test");

    // GIVEN: An offered skeleton service for a field with no notifier
    GivenAGenericSkeletonWithOneField("test_field", {16, 8}, false, false, false);

    // Set initial value manually before offering
    std::vector<uint8_t> init_val{0xAA, 0xBB};
    static_cast<void>(field_->Update(init_val));

    // EXPECT: Initial value allocation and send to the binding during OfferService
    std::vector<uint8_t> dummy_memory1(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc1{dummy_memory1.data(), [](void*) {}};
    EXPECT_CALL(*mock_event_binding_ptr_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc1)))));
    EXPECT_CALL(*mock_event_binding_ptr_, Send(_)).WillOnce(Return(score::Result<void>{}));

    OfferSkeletonService();

    // WHEN: Calling Update
    std::vector<uint8_t> new_val{0xCC, 0xDD};
    std::vector<uint8_t> dummy_memory(16, 0);
    mock_binding::SampleAllocateePtr<void> dummy_alloc{dummy_memory.data(), [](void*) {}};

    // EXPECT: An allocation and send to the binding (to update shared memory for Getters)
    EXPECT_CALL(*mock_event_binding_ptr_, Allocate(_))
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::move(dummy_alloc)))));
    EXPECT_CALL(*mock_event_binding_ptr_, Send(_)).WillOnce(Return(score::Result<void>{}));

    auto update_res = field_->Update(new_val);

    // THEN: The update succeeds
    EXPECT_TRUE(update_res.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
