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
#include "score/mw/com/impl/skeleton_base.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_field.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

using TestSampleType = std::uint8_t;

const auto kDummyEventName{"DummyEvent"};
const auto kDummyEventName2{"DummyEvent2"};
const auto kDummyFieldName{"DummyField"};

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();

const TestSampleType kInitialFieldValue(10);

class MyDummySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonEvent<TestSampleType> dummy_event{*this, kDummyEventName};
    SkeletonEvent<TestSampleType> dummy_event2{*this, kDummyEventName2};

    SkeletonField<TestSampleType> dummy_field{*this, kDummyFieldName};
};

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard(RuntimeMock& runtime_mock) noexcept : runtime_mock_{runtime_mock}
    {
        Runtime::InjectMock(&runtime_mock_);
    }

    ~RuntimeMockGuard()
    {
        Runtime::InjectMock(nullptr);
    }

    RuntimeMock& runtime_mock_;
};

mock_binding::Skeleton* GetMockBinding(MyDummySkeleton& skeleton) noexcept
{
    auto* const binding_mock_raw = SkeletonBaseView{skeleton}.GetBinding();
    return dynamic_cast<mock_binding::Skeleton*>(binding_mock_raw);
}

class SkeletonBaseFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(runtime_mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));
    }

    InstanceIdentifier GetInstanceIdentifierWithValidBinding()
    {
        return make_InstanceIdentifier(valid_instance_deployment_, type_deployment_);
    }

    InstanceIdentifier GetInstanceIdentifierWithoutBinding()
    {
        return make_InstanceIdentifier(no_instance_deployment_, type_deployment_);
    }

    InstanceIdentifier GetInstanceIdentifierWithInvalidBinding()
    {
        return make_InstanceIdentifier(invalid_instance_deployment_, type_deployment_);
    }

    void ExpectEventCreation(const InstanceIdentifier& instance_identifier) noexcept
    {
        auto skeleton_event_mock_ptr_1 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
        auto skeleton_event_mock_ptr_2 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
        auto skeleton_field_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();

        event_binding_mock_1_ = skeleton_event_mock_ptr_1.get();
        event_binding_mock_2_ = skeleton_event_mock_ptr_2.get();
        field_binding_mock_ = skeleton_field_mock_ptr.get();

        EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                    Create(instance_identifier, _, kDummyEventName))
            .WillOnce(Return(ByMove(std::move(skeleton_event_mock_ptr_1))));
        EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                    Create(instance_identifier, _, kDummyEventName2))
            .WillOnce(Return(ByMove(std::move(skeleton_event_mock_ptr_2))));
        EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                    CreateEventBinding(instance_identifier, _, kDummyFieldName))
            .WillOnce(Return(ByMove(std::move(skeleton_field_mock_ptr))));

        EXPECT_CALL(*event_binding_mock_1_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
        EXPECT_CALL(*event_binding_mock_2_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
        EXPECT_CALL(*field_binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    }

    void CreateSkeleton(const InstanceIdentifier& instance_identifier) noexcept
    {
        ON_CALL(runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        // Expect that both events and the field are created with mock bindings
        ExpectEventCreation(instance_identifier);

        skeleton_ = std::make_unique<MyDummySkeleton>(std::make_unique<mock_binding::Skeleton>(), instance_identifier);

        binding_mock_ = GetMockBinding(*skeleton_);
        ASSERT_NE(binding_mock_, nullptr);
    }

    void ExpectOfferService() noexcept
    {
        // Expecting that PrepareOffer gets called on the skeleton binding and both events which all return a valid
        // result
        EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
        EXPECT_CALL(*event_binding_mock_1_, PrepareOffer());
        EXPECT_CALL(*event_binding_mock_2_, PrepareOffer());
        EXPECT_CALL(*field_binding_mock_, PrepareOffer());
        EXPECT_CALL(service_discovery_mock_, OfferService(_));
    }

    void ExpectStopOfferService() noexcept
    {
        // PrepareStopOffer is called on the skeleton binding and both events
        EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer());
        EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer());
        EXPECT_CALL(*field_binding_mock_, PrepareStopOffer());
        EXPECT_CALL(*binding_mock_, PrepareStopOffer(_));
        EXPECT_CALL(service_discovery_mock_, StopOfferService(_));
    }

    const ServiceTypeDeployment type_deployment_{score::cpp::blank{}};
    const ServiceIdentifierType service_{make_ServiceIdentifierType("foo")};
    const ServiceInstanceDeployment no_instance_deployment_{
        service_,
        score::cpp::blank{},
        QualityType::kASIL_QM,
        InstanceSpecifier::Create("abc/abc/TirePressurePort/no_instance").value()};
    const ServiceInstanceDeployment valid_instance_deployment_{
        service_,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{0U}},
        QualityType::kASIL_QM,
        InstanceSpecifier::Create("abc/abc/TirePressurePort/valid_instance").value()};
    const ServiceInstanceDeployment invalid_instance_deployment_{
        service_,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{0U}},
        QualityType::kASIL_QM,
        InstanceSpecifier::Create("abc/abc/TirePressurePort/invalid_instance").value()};

    ServiceDiscoveryMock service_discovery_mock_{};
    RuntimeMock runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{runtime_mock_};

    mock_binding::Skeleton* binding_mock_{nullptr};
    mock_binding::SkeletonEvent<TestSampleType>* event_binding_mock_1_{nullptr};
    mock_binding::SkeletonEvent<TestSampleType>* event_binding_mock_2_{nullptr};
    mock_binding::SkeletonEvent<TestSampleType>* field_binding_mock_{nullptr};

    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard_{};
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard_{};

    std::unique_ptr<MyDummySkeleton> skeleton_{nullptr};
};

using SkeletonBaseOfferFixture = SkeletonBaseFixture;
TEST_F(SkeletonBaseOfferFixture, OfferService)
{
    RecordProperty("Verifies", "SCR-5897815, SCR-17434118");  // SWS_CM_00101
    RecordProperty("Description", "Checks whether a service can be offered.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareOffer gets called on the skeleton binding and each event
    ExpectOfferService();

    // and expecting PrepareStopOffer is called on the skeleton binding and each event
    ExpectStopOfferService();

    // and expecting that Send is called on the event binding with the initial value
    EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());
}

TEST_F(SkeletonBaseOfferFixture, CallingPrepareOfferWhenSkeletonBindingPrepareOfferFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-6222081, SCR-21856131, SCR-17434118");
    RecordProperty("Description",
                   "Checks that service offering returns error when binding prepare offer fails as no events will be "
                   "offered, which violates req.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expect that PrepareOffer fails when being called on the binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then the result contains an error that the binding failed
    ASSERT_FALSE(offer_result.has_value());
    ASSERT_EQ(offer_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonBaseOfferFixture, CallingPrepareOfferWhenEventBindingFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-6222081, SCR-21856131, SCR-17434118");
    RecordProperty("Description",
                   "Checks that service offering returns error when event binding prepare offer fails as no events "
                   "will be offered, which violates req.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expect that PrepareOffer fails when being called on the first event binding in the unordered map (we don't know
    // the order of the map so we add possible expecations on both event bindings)
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer())
        .Times(AnyNumber())
        .WillRepeatedly(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer())
        .Times(AnyNumber())
        .WillRepeatedly(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then the result contains an error that the binding failed
    ASSERT_FALSE(offer_result.has_value());
    ASSERT_EQ(offer_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonBaseOfferFixture, CallingPrepareOfferWhenFieldValueNotSetReturnsError)
{
    RecordProperty("Verifies", "SCR-6222081, SCR-21856131, SCR-17434118");
    RecordProperty("Description",
                   "Checks, that service offering leads to termination, when binding prepare offer service fails as "
                   "no events will be offered, which violates req.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expect that PrepareOffer fails when being called on the binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer());

    // and expect that PrepareStopOffer is called on the skeleton binding and both events but not on the field which
    // failed to be offered
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer());
    EXPECT_CALL(*field_binding_mock_, PrepareStopOffer()).Times(0);
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_)).Times(0);

    // When the intitial value of the field is not set

    // and when offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then the result contains an error that the initial value is not valid
    ASSERT_FALSE(offer_result.has_value());
    ASSERT_EQ(offer_result.error(), ComErrc::kFieldValueIsNotValid);
}

TEST_F(SkeletonBaseOfferFixture, CallingPrepareOfferWhenFieldBindingFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-6222081, SCR-21856131, SCR-17434118");
    RecordProperty("Description",
                   "Checks, that service offering leads to termination, when binding prepare offer service fails as "
                   "no events will be offered, which violates req.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expect that PrepareOffer fails when being called on the binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer());
    EXPECT_CALL(*field_binding_mock_, PrepareOffer())
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidBindingInformation)));

    // and expect that PrepareStopOffer is called on the skeleton binding and both events but not on the field which
    // failed to be offered
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer());
    EXPECT_CALL(*field_binding_mock_, PrepareStopOffer()).Times(0);
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_)).Times(0);

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // and when offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then the result contains an error that the binding failed
    ASSERT_FALSE(offer_result.has_value());
    ASSERT_EQ(offer_result.error(), ComErrc::kBindingFailure);
}

using SkeletonBaseOfferDeathTest = SkeletonBaseOfferFixture;
TEST_F(SkeletonBaseOfferDeathTest, TerminateOnOfferWithNoBinding)
{
    RecordProperty("Verifies", "SCR-6222081, SCR-21856131");
    RecordProperty("Description",
                   "Checks, that service offering leads to termination without valid binding as no events will be "
                   "offered, which violates req.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto offer_unit_with_prepare_offering_failure = [this]() {
        // Given a constructed Skeleton with an in-valid identifier
        CreateSkeleton(GetInstanceIdentifierWithInvalidBinding());

        // When offering a Service expect termination
        EXPECT_DEATH({ score::cpp::ignore = skeleton_->OfferService(); }, ".*");
    };

    // Expect to die, when offering a Service without a valid binding
    EXPECT_DEATH(offer_unit_with_prepare_offering_failure(), ".*");
}

TEST_F(SkeletonBaseOfferDeathTest, OfferServiceTerminatesWhenBindingIsNull)
{
    std::unique_ptr<SkeletonBinding> skeleton_binding_null{nullptr};

    // Given a constructed Skeleton with null Binding
    SkeletonBase unit{std::move(skeleton_binding_null),
                      this->GetInstanceIdentifierWithValidBinding(),
                      MethodCallProcessingMode::kEvent};

    // Expect to die, when offering a Service without a valid binding
    EXPECT_DEATH({ score::cpp::ignore = unit.OfferService(); }, ".*");
}

using SkeletonBaseStopOfferFixture = SkeletonBaseFixture;
TEST_F(SkeletonBaseStopOfferFixture, PrepareStopOfferCalledOnSkeletonAndEventsWhenSkeletonIsDestroyed)
{
    RecordProperty("Verifies", "SCR-6093144");
    RecordProperty("Description", "Check whether the service event offering is stopped when the skeleton is destroyed");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareOffer gets called on the skeleton binding and each event
    ExpectOfferService();

    // and expecting PrepareStopOffer is called on the skeleton binding and each event
    ExpectStopOfferService();

    // and expecting that Send is called on the event binding with the initial value
    EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());
}

TEST_F(SkeletonBaseStopOfferFixture, PrepareStopOffer)
{
    RecordProperty("Verifies", "SCR-5897820, SCR-17434265");  // SWS_CM_00111
    RecordProperty("Description", "Checks that PrepareStopOffer() actually stops a offered service");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareOffer gets called on the skeleton binding and each event
    ExpectOfferService();

    // and expecting PrepareStopOffer is called on the skeleton binding and each event
    ExpectStopOfferService();

    // and expecting that Send is called on the event binding with the initial value
    EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // When stop offering a Service
    skeleton_->StopOfferService();
}

TEST_F(SkeletonBaseStopOfferFixture, StopOfferIsNotCalledIfServiceWasNotOffered)
{
    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareStopOffer is not called on any event or field
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer()).Times(0);
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer()).Times(0);
    EXPECT_CALL(*field_binding_mock_, PrepareStopOffer()).Times(0);

    // and that the binding does not get un-initialized (PrepareStopOffer() called)
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_)).Times(0);

    // When stop offering a Service
    skeleton_->StopOfferService();

    // Or when destroying the skeleton
}

TEST_F(SkeletonBaseStopOfferFixture, CallingStopOfferServiceWhenStopOfferServiceFailsOnBindingDoesNotCrash)
{
    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareOffer gets called on the skeleton binding and each event
    ExpectOfferService();

    // and expecting Service Discovery fails on the stop offering
    EXPECT_CALL(service_discovery_mock_, StopOfferService(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // Given Service offering
    const auto offer_result = skeleton_->OfferService();

    // Then the program does not terminate
    ASSERT_TRUE(offer_result.has_value());

    // Then Service Discovery calls StopOfferService on Skeleton destruction
}

using SkeletonBaseMoveFixture = SkeletonBaseFixture;
TEST_F(SkeletonBaseMoveFixture, MovingConstructingSkeletonBaseDoesNotCallPrepareStopOffer)
{
    bool is_skeleton_stop_offer_called{false};
    std::size_t event_stop_offer_called_count{0};
    {
        // Expect that both events and the field are created with mock bindings
        ExpectEventCreation(GetInstanceIdentifierWithValidBinding());

        // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
        MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), GetInstanceIdentifierWithValidBinding()};
        mock_binding::Skeleton* const binding_mock = GetMockBinding(unit);
        ASSERT_NE(binding_mock, nullptr);

        // Expecting that PrepareOffer gets called on the skeleton binding and both events / field
        EXPECT_CALL(*binding_mock, PrepareOffer(_, _, _));
        EXPECT_CALL(*event_binding_mock_1_, PrepareOffer());
        EXPECT_CALL(*event_binding_mock_2_, PrepareOffer());
        EXPECT_CALL(*field_binding_mock_, PrepareOffer());
        EXPECT_CALL(service_discovery_mock_, OfferService(_));

        // and Expecting that PrepareOffer gets called on the skeleton binding and each event / field
        EXPECT_CALL(service_discovery_mock_, StopOfferService(_));
        EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer())
            .WillRepeatedly(Invoke([&event_stop_offer_called_count]() {
                event_stop_offer_called_count++;
            }));
        EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer())
            .WillRepeatedly(Invoke([&event_stop_offer_called_count]() {
                event_stop_offer_called_count++;
            }));
        EXPECT_CALL(*field_binding_mock_, PrepareStopOffer()).WillRepeatedly(Invoke([&event_stop_offer_called_count]() {
            event_stop_offer_called_count++;
        }));
        EXPECT_CALL(*binding_mock, PrepareStopOffer(_)).WillOnce(Invoke([&is_skeleton_stop_offer_called]() {
            is_skeleton_stop_offer_called = true;
        }));

        // and expecting that Send is called on the event binding with the initial value
        EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

        // and the initial field value is set
        unit.dummy_field.Update(kInitialFieldValue);

        // When offering a Service
        const auto offer_result = unit.OfferService();

        // Then no error is returned
        ASSERT_TRUE(offer_result.has_value());

        // not when the move constructor is called
        MyDummySkeleton unit2{std::move(unit)};
        EXPECT_FALSE(is_skeleton_stop_offer_called);
        EXPECT_EQ(event_stop_offer_called_count, 0);
    }
    // but only when the skeleton is destroyed
    EXPECT_TRUE(is_skeleton_stop_offer_called);
    EXPECT_EQ(event_stop_offer_called_count, 3);
}

TEST_F(SkeletonBaseOfferFixture, OfferServiceReturnsErrorWhenServiceDiscoveryOfferServiceFails)
{
    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that when OfferService is called on the ServiceDiscovery binding an error is returned
    EXPECT_CALL(service_discovery_mock_, OfferService(_)).WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then an error is returned
    ASSERT_FALSE(offer_result.has_value());
    EXPECT_EQ(offer_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonBaseMoveFixture, MovingAssigningOfferedSkeletonBaseCallsPrepareStopOffer)
{
    const auto instance_identifier = GetInstanceIdentifierWithValidBinding();
    const ServiceIdentifierType service{make_ServiceIdentifierType("foo2")};
    const ServiceInstanceDeployment valid_instance_deployment2{
        service, LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto valid_instance_identifier2 = make_InstanceIdentifier(valid_instance_deployment2, type_deployment_);

    // Expect that both events and the field are created with mock bindings
    ExpectEventCreation(instance_identifier);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), instance_identifier};
    mock_binding::Skeleton* const binding_mock = GetMockBinding(unit);
    ASSERT_NE(binding_mock, nullptr);

    // Expecting that PrepareOffer gets called on the first skeleton binding
    EXPECT_CALL(*binding_mock, PrepareOffer(_, _, _));

    // and PrepareOffer is called on each event / field
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer());
    EXPECT_CALL(*field_binding_mock_, PrepareOffer());
    EXPECT_CALL(service_discovery_mock_, OfferService(instance_identifier));

    EXPECT_CALL(service_discovery_mock_, StopOfferService(instance_identifier));

    // and Expecting that PrepareStopOffer gets called on the first skeleton binding
    EXPECT_CALL(*binding_mock, PrepareStopOffer(_));

    // and expecting that PrepareStopOffer is called on each skeleton event binding
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer());
    EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer());
    EXPECT_CALL(*field_binding_mock_, PrepareStopOffer());

    // and expecting that Send is called on the event binding with the initial value
    EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

    // and the initial field value is set
    unit.dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    const auto offer_result = unit.OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and given a second constructed Skeleton with a valid identifier with two events and a field registered with
    // the skeleton

    auto skeleton_event_mock_ptr_1 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto skeleton_event_mock_ptr_2 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto skeleton_field_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();

    auto& second_event_binding_mock_1 = *skeleton_event_mock_ptr_1;
    auto& second_event_binding_mock_2 = *skeleton_event_mock_ptr_2;
    auto& second_field_binding_mock = *skeleton_field_mock_ptr;

    // Expect that both events and the field are created with mock bindings for the second skeleton
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(valid_instance_identifier2, _, kDummyEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_mock_ptr_1))));
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(valid_instance_identifier2, _, kDummyEventName2))
        .WillOnce(Return(ByMove(std::move(skeleton_event_mock_ptr_2))));
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(valid_instance_identifier2, _, kDummyFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_mock_ptr))));

    // and GetBindingType is called on each event / field
    EXPECT_CALL(second_event_binding_mock_1, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    EXPECT_CALL(second_event_binding_mock_2, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    EXPECT_CALL(second_field_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    MyDummySkeleton unit2{std::make_unique<mock_binding::Skeleton>(), valid_instance_identifier2};
    mock_binding::Skeleton* const second_binding_mock = GetMockBinding(unit2);
    ASSERT_NE(second_binding_mock, nullptr);

    // Expecting that PrepareOffer gets called on the second skeleton binding
    EXPECT_CALL(*second_binding_mock, PrepareOffer(_, _, _));

    // and PrepareOffer is called on each event / field
    EXPECT_CALL(second_event_binding_mock_1, PrepareOffer());
    EXPECT_CALL(second_event_binding_mock_2, PrepareOffer());
    EXPECT_CALL(second_field_binding_mock, PrepareOffer());
    EXPECT_CALL(service_discovery_mock_, OfferService(valid_instance_identifier2));

    EXPECT_CALL(service_discovery_mock_, StopOfferService(valid_instance_identifier2));

    // and Expecting that PrepareStopOffer gets called on the second skeleton binding
    EXPECT_CALL(*second_binding_mock, PrepareStopOffer(_));

    // and expecting that PrepareStopOffer is called on each skeleton event binding
    EXPECT_CALL(second_event_binding_mock_1, PrepareStopOffer());
    EXPECT_CALL(second_event_binding_mock_2, PrepareStopOffer());
    EXPECT_CALL(second_field_binding_mock, PrepareStopOffer());

    // and expecting that Send is called on the event binding with a new value
    TestSampleType initial_field_value_2{20};
    EXPECT_CALL(second_field_binding_mock, Send(initial_field_value_2, _));

    // and the initial field value is set
    unit2.dummy_field.Update(initial_field_value_2);

    // When offering the second Service
    const auto offer_result_2 = unit2.OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result_2.has_value());

    // When move assigning the second skeleton to the first skeleton
    unit = std::move(unit2);
}

TEST_F(SkeletonBaseMoveFixture, MovingAssigningUnOfferedSkeletonBaseDoesNotCallPrepareStopOffer)
{
    RecordProperty("Verifies", "SCR-17432438");
    RecordProperty("Description", "skeleton is move assignable");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    const auto instance_identifier = GetInstanceIdentifierWithValidBinding();
    const ServiceIdentifierType service{make_ServiceIdentifierType("foo2")};
    const ServiceInstanceDeployment valid_instance_deployment2{
        service, LolaServiceInstanceDeployment{LolaServiceInstanceId{0U}}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto valid_instance_identifier2 = make_InstanceIdentifier(valid_instance_deployment2, type_deployment_);

    // Expect that both events and the field are created with mock bindings
    ExpectEventCreation(instance_identifier);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), instance_identifier};
    mock_binding::Skeleton* const binding_mock = GetMockBinding(unit);
    ASSERT_NE(binding_mock, nullptr);

    // Expecting that PrepareOffer is never called on the first skeleton binding
    EXPECT_CALL(*binding_mock, PrepareOffer(_, _, _)).Times(0);
    EXPECT_CALL(service_discovery_mock_, OfferService(instance_identifier)).Times(0);

    // and PrepareOffer is never called on any event in the first skeleton
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer()).Times(0);
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer()).Times(0);
    EXPECT_CALL(*field_binding_mock_, PrepareOffer()).Times(0);

    EXPECT_CALL(service_discovery_mock_, StopOfferService(instance_identifier)).Times(0);

    // and expecting that PrepareStopOffer never gets called on the first skeleton binding
    EXPECT_CALL(*binding_mock, PrepareStopOffer(_)).Times(0);

    // and given a second constructed Skeleton with a valid identifier with two events and a field registered with
    // the skeleton

    auto second_skeleton_event_mock_ptr_1 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto second_skeleton_event_mock_ptr_2 = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto second_skeleton_field_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();

    auto& second_event_binding_mock_1 = *second_skeleton_event_mock_ptr_1;
    auto& second_event_binding_mock_2 = *second_skeleton_event_mock_ptr_2;
    auto& second_field_binding_mock = *second_skeleton_field_mock_ptr;

    // Expect that both events and the field are created with mock bindings
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(valid_instance_identifier2, _, kDummyEventName))
        .WillOnce(Return(ByMove(std::move(second_skeleton_event_mock_ptr_1))));
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(valid_instance_identifier2, _, kDummyEventName2))
        .WillOnce(Return(ByMove(std::move(second_skeleton_event_mock_ptr_2))));
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(valid_instance_identifier2, _, kDummyFieldName))
        .WillOnce(Return(ByMove(std::move(second_skeleton_field_mock_ptr))));

    // and GetBindingType is called on each event / field
    EXPECT_CALL(second_event_binding_mock_1, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    EXPECT_CALL(second_event_binding_mock_2, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    EXPECT_CALL(second_field_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    MyDummySkeleton unit2{std::make_unique<mock_binding::Skeleton>(), valid_instance_identifier2};
    mock_binding::Skeleton* const second_binding_mock = GetMockBinding(unit2);
    ASSERT_NE(second_binding_mock, nullptr);

    // Expecting that PrepareOffer is called on the second skeleton binding
    EXPECT_CALL(*second_binding_mock, PrepareOffer(_, _, _));

    // and that PrepareOffer is called on the second skeleton event bindings
    EXPECT_CALL(second_event_binding_mock_1, PrepareOffer());
    EXPECT_CALL(second_event_binding_mock_2, PrepareOffer());
    EXPECT_CALL(second_field_binding_mock, PrepareOffer());
    EXPECT_CALL(service_discovery_mock_, OfferService(valid_instance_identifier2));

    EXPECT_CALL(service_discovery_mock_, StopOfferService(valid_instance_identifier2));

    // and that PrepareStopOffer is called on the second skeleton event bindings
    EXPECT_CALL(second_event_binding_mock_1, PrepareStopOffer());
    EXPECT_CALL(second_event_binding_mock_2, PrepareStopOffer());
    EXPECT_CALL(second_field_binding_mock, PrepareStopOffer());

    // and that PrepareStopOffer is called on the second skeleton binding
    EXPECT_CALL(*second_binding_mock, PrepareStopOffer(_));

    // and expecting that Send is called on the event binding with the initial value
    EXPECT_CALL(second_field_binding_mock, Send(kInitialFieldValue, _));

    // and the initial field value is set
    unit2.dummy_field.Update(kInitialFieldValue);

    // When offering the second Service
    const auto offer_result_2 = unit2.OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result_2.has_value());

    // When move assigning the second skeleton to the first skeleton
    unit = std::move(unit2);
}

TEST_F(SkeletonBaseMoveFixture, SelfMovingAssignmentDoesNotCauseIssues)
{
    // To prevent undefined behavior reported by sanitizer tools when accessing this counter (captured by EXPECT_CALL)
    // after its scope ends, encapsulate the counter in a shared_ptr to ensure proper lifetime management and safe
    // shared ownership.
    auto stop_offer_called_count = std::make_shared<size_t>(0U);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // Expecting that PrepareOffer gets called on the skeleton binding and each event
    ExpectOfferService();

    // Expecting that Service Discovery does not perform cleanup operations during self-assignment. However, verify that
    // cleanup operations are correctly invoked when the Skeleton is destroyed.
    EXPECT_CALL(service_discovery_mock_, StopOfferService(_))
        .WillOnce(Invoke([counter = stop_offer_called_count](InstanceIdentifier) -> ResultBlank {
            score::cpp::ignore = ++(*counter);
            return ResultBlank{};
        }));
    // and the initial field value is set
    skeleton_->dummy_field.Update(kInitialFieldValue);

    // When offering a Service
    score::cpp::ignore = skeleton_->OfferService();

    // Then Skeleton self-moving assignment does not perform Cleanup()
    *skeleton_ = std::move(*skeleton_);

    // Expecting that Service Discovery did not call StopOfferService
    EXPECT_EQ(*stop_offer_called_count, 0U);

    // Then no issues occur, and the skeleton remains valid
    ASSERT_NE(binding_mock_, nullptr);
    ASSERT_NE(event_binding_mock_1_, nullptr);
    ASSERT_NE(event_binding_mock_2_, nullptr);
    ASSERT_NE(field_binding_mock_, nullptr);
}

TEST_F(SkeletonBaseOfferFixture, ServiceCanBeReOfferedAfterMoveConstructingService)
{
    RecordProperty("Verifies", "SCR-17432457, SCR-17432438");
    RecordProperty("Description",
                   "If the service provided by the skeleton is currently being offered at the time of the destruction, "
                   "the offering shall be stopped. And skeleton is move constructible");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expect that both events and the field are created with mock bindings
    ExpectEventCreation(GetInstanceIdentifierWithValidBinding());

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    MyDummySkeleton skeleton(std::make_unique<mock_binding::Skeleton>(), GetInstanceIdentifierWithValidBinding());
    mock_binding::Skeleton* const binding_mock = GetMockBinding(skeleton);
    ASSERT_NE(binding_mock, nullptr);

    // Expecting that PrepareOffer gets called on the skeleton binding and each event twice, each time OfferService is
    // called (i.e. total of 6)
    EXPECT_CALL(*binding_mock, PrepareOffer(_, _, _)).Times(2);
    EXPECT_CALL(*event_binding_mock_1_, PrepareOffer()).Times(2);
    EXPECT_CALL(*event_binding_mock_2_, PrepareOffer()).Times(2);
    EXPECT_CALL(*field_binding_mock_, PrepareOffer()).Times(2);
    EXPECT_CALL(service_discovery_mock_, OfferService(_)).Times(2);

    // and expecting PrepareStopOffer is called on the skeleton binding and each event twice every time
    // StopOfferService is called (manually and on destruction of skeleton2)
    EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer()).Times(2);
    EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer()).Times(2);
    EXPECT_CALL(*field_binding_mock_, PrepareStopOffer()).Times(2);
    EXPECT_CALL(*binding_mock, PrepareStopOffer(_)).Times(2);
    EXPECT_CALL(service_discovery_mock_, StopOfferService(_)).Times(2);

    // and expecting that Send is called on the event binding once with the initial value
    EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

    // and the initial field value is set
    skeleton.dummy_field.Update(kInitialFieldValue);

    // When offering the Service
    const auto offer_result = skeleton.OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and when the move constructor is called
    MyDummySkeleton skeleton2{std::move(skeleton)};

    // Then after calling stop offering a Service
    skeleton2.StopOfferService();

    // When re-offering the second Service
    const auto offer_result_2 = skeleton2.OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result_2.has_value());

    // and when the skeleton is destroyed
}

TEST_F(SkeletonBaseOfferFixture, ServiceCanBeReOfferedAfterCallingStopOfferService)
{
    int skeleton_offer_count{0};
    int skeleton_stop_offer_count{0};
    int event_offer_count{0};
    int event_stop_offer_count{0};

    {
        // Expect that both events and the field are created with mock bindings
        ExpectEventCreation(GetInstanceIdentifierWithValidBinding());

        // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
        MyDummySkeleton skeleton(std::make_unique<mock_binding::Skeleton>(), GetInstanceIdentifierWithValidBinding());
        mock_binding::Skeleton* const binding_mock = GetMockBinding(skeleton);
        ASSERT_NE(binding_mock, nullptr);

        // Expecting that PrepareOffer gets called on the skeleton binding and each event twice
        EXPECT_CALL(*binding_mock, PrepareOffer(_, _, _))
            .Times(2)
            .WillRepeatedly(Invoke(
                [&skeleton_offer_count](SkeletonBinding::SkeletonEventBindings&,
                                        SkeletonBinding::SkeletonFieldBindings&,
                                        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback>) -> ResultBlank {
                    skeleton_offer_count++;
                    return {};
                }));
        EXPECT_CALL(*event_binding_mock_1_, PrepareOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_offer_count]() -> ResultBlank {
                event_offer_count++;
                return {};
            }));
        EXPECT_CALL(*event_binding_mock_2_, PrepareOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_offer_count]() -> ResultBlank {
                event_offer_count++;
                return {};
            }));
        EXPECT_CALL(*field_binding_mock_, PrepareOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_offer_count]() -> ResultBlank {
                event_offer_count++;
                return {};
            }));
        EXPECT_CALL(service_discovery_mock_, OfferService(_)).Times(2);

        EXPECT_CALL(service_discovery_mock_, StopOfferService(_)).Times(2);
        // and expecting PrepareStopOffer is called on the skeleton binding and each event twice
        EXPECT_CALL(*event_binding_mock_1_, PrepareStopOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_stop_offer_count]() {
                event_stop_offer_count++;
            }));
        EXPECT_CALL(*event_binding_mock_2_, PrepareStopOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_stop_offer_count]() {
                event_stop_offer_count++;
            }));
        EXPECT_CALL(*field_binding_mock_, PrepareStopOffer())
            .Times(2)
            .WillRepeatedly(Invoke([&event_stop_offer_count]() {
                event_stop_offer_count++;
            }));
        EXPECT_CALL(*binding_mock, PrepareStopOffer(_))
            .Times(2)
            .WillRepeatedly(Invoke([&skeleton_stop_offer_count]() -> void {
                skeleton_stop_offer_count++;
            }));

        // and expecting that Send is called on the event binding with the initial value
        EXPECT_CALL(*field_binding_mock_, Send(kInitialFieldValue, _));

        // and the initial field value is set
        skeleton.dummy_field.Update(kInitialFieldValue);

        // When offering the Service
        const auto offer_result = skeleton.OfferService();

        // Then no error is returned
        ASSERT_TRUE(offer_result.has_value());

        // Then PrepareOffer() is called on the skeleton and events the first time
        EXPECT_EQ(skeleton_offer_count, 1);
        EXPECT_EQ(event_offer_count, 3);

        // When stop offering a Service
        skeleton.StopOfferService();

        // Then PrepareStopOffer() is called on the skeleton and events the first time
        EXPECT_EQ(skeleton_stop_offer_count, 1);
        EXPECT_EQ(event_stop_offer_count, 3);

        // When re-offering the Service
        const auto offer_result_2 = skeleton.OfferService();

        // Then no error is returned
        ASSERT_TRUE(offer_result_2.has_value());

        // Then PrepareOffer() is called on the skeleton and events the second time
        EXPECT_EQ(skeleton_offer_count, 2);
        EXPECT_EQ(event_offer_count, 6);

        // and when the skeleton is destroyed
    }
    // then PrepareStopOffer() is called on the skeleton and events the second time
    EXPECT_EQ(skeleton_stop_offer_count, 2);
    EXPECT_EQ(event_stop_offer_count, 6);
}

TEST_F(SkeletonBaseOfferFixture, NoStopOfferOnErrorIdentifier)
{
    const auto instance_identifier = GetInstanceIdentifierWithoutBinding();

    // Expect that the events and field bindings are never created
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(instance_identifier, _, kDummyEventName))
        .Times(0);
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(instance_identifier, _, kDummyEventName2))
        .Times(0);
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(GetInstanceIdentifierWithoutBinding(), _, kDummyFieldName))
        .Times(0);

    // Given a constructed Skeleton with a invalid identifier
    CreateSkeleton(instance_identifier);

    // When stop offering a Service
    skeleton_->StopOfferService();
}

}  // namespace
}  // namespace score::mw::com::impl
