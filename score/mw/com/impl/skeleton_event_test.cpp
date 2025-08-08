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
#include "score/mw/com/impl/skeleton_event.h"

#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using TestSampleType = std::uint8_t;

const std::string_view kEventName{"Event1"};

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
std::uint16_t kInstanceId{23U};
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class MyDummySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonEvent<TestSampleType> my_dummy_event_{*this, kEventName};
};

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        Runtime::InjectMock(&runtime_mock_);
    }
    ~RuntimeMockGuard()
    {
        Runtime::InjectMock(nullptr);
    }

    RuntimeMock runtime_mock_;
};

TEST(SkeletonEventTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-21840365");
    RecordProperty("Description", "Checks that class is neither copy-constructable nor copy-assignable.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    static_assert(!std::is_copy_constructible<SkeletonEvent<TestSampleType>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonEvent<TestSampleType>>::value, "Is wrongly copyable");
}

TEST(SkeletonEventTest, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonEvent<TestSampleType>>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonEvent<TestSampleType>>::value, "Is not move assignable");
}

TEST(SkeletonEventTest, SkeletonEventContainsPublicSampleType)
{
    RecordProperty("Verifies", "SCR-21840366");
    RecordProperty("Description",
                   "A SkeletonEvent contains a public member type EventType which denotes the type of the event.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<typename SkeletonEvent<TestSampleType>::EventType, TestSampleType>::value,
                  "Incorrect EventType.");
}

TEST(SkeletonEventTest, ClassTypeDependsOnEventDataType)
{
    RecordProperty("Verifies", "SCR-29235002");
    RecordProperty("Description", "SkeletonEvents with different event data types should be different classes.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using FirstSkeletonEventType = SkeletonEvent<bool>;
    using SecondSkeletonEventType = SkeletonEvent<std::uint16_t>;
    static_assert(!std::is_same_v<FirstSkeletonEventType, SecondSkeletonEventType>,
                  "Class type does not depend on event data type");
}

TEST(SkeletonEventAllocateTest, CallingAllocateAfterPrepareOfferDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-21470600");
    RecordProperty("Description", "Checks that calling allocate after offer service dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that PrepareStopOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    const auto allocated_slot_result = unit.my_dummy_event_.Allocate();

    // Then the allocated slot is valid
    ASSERT_TRUE(allocated_slot_result.has_value());
}

TEST(SkeletonEventAllocateTest, CallingAllocateBeforePrepareOfferReturnsError)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description", "Checks that allocate before offer service returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that Allocate() is never called on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Allocate()).Times(0);

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // and Allocate is called on the event before PrepareOffer
    const auto allocated_slot_result = unit.my_dummy_event_.Allocate();

    // Then the result contains an error the service has not been offered
    ASSERT_FALSE(allocated_slot_result.has_value());
    EXPECT_EQ(allocated_slot_result.error(), ComErrc::kNotOffered);
}

TEST(SkeletonEventAllocateTest, CallingAllocateAfterPrepareOfferWhenBindingFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description",
                   "Checks that calling allocate after offer service propagates an error from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Allocate() is called once on the event binding which returns an error
    EXPECT_CALL(skeleton_event_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kInvalidConfiguration))));

    // and that PrepareStopOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    const auto allocated_slot_result = unit.my_dummy_event_.Allocate();

    // Then the result contains an error the binding failed
    ASSERT_FALSE(allocated_slot_result.has_value());
    EXPECT_EQ(allocated_slot_result.error(), ComErrc::kBindingFailure);
}

TEST(SkeletonEventSendZeroCopyTest, CallingSendDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-21470600, SCR-21840371, SCR-21840368, SCR-21553623");
    RecordProperty("Description", "Checks that calling zero copy Send dispatches to the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send(SampleAllocateePtr) is called on the event binding with the expected value
    EXPECT_CALL(skeleton_event_binding_mock, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([](SampleAllocateePtr<TestSampleType> sample_ptr) -> ResultBlank {
            EXPECT_EQ(*sample_ptr, 42);
            return {};
        })));

    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto allocated_slot_result = unit.my_dummy_event_.Allocate();

    // Then the result is valid
    ASSERT_TRUE(allocated_slot_result.has_value());
    auto allocated_slot = std::move(allocated_slot_result).value();

    // and when assigning a value to the slot
    *allocated_slot = 42;

    // When calling Send() on the event
    const auto send_result = unit.my_dummy_event_.Send(std::move(allocated_slot));

    // Then no error is returned
    ASSERT_TRUE(send_result.has_value());
}

TEST(SkeletonEventSendZeroCopyTest, CallingSendWhenBindingFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-21840371");
    RecordProperty("Description", "Checks that calling zero copy Send propagates an error from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send(SampleAllocateePtr) is called on the event binding with the expected value
    EXPECT_CALL(skeleton_event_binding_mock, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArg<0>(Invoke([](SampleAllocateePtr<TestSampleType> sample_ptr) -> ResultBlank {
            EXPECT_EQ(*sample_ptr, 42);
            return MakeUnexpected(ComErrc::kInvalidConfiguration);
        })));

    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto allocated_slot_result = unit.my_dummy_event_.Allocate();

    // Then the result is valid
    ASSERT_TRUE(allocated_slot_result.has_value());
    auto allocated_slot = std::move(allocated_slot_result).value();

    // and when assigning a value to the slot
    *allocated_slot = 42;

    // When calling Send() on the event
    const auto send_result = unit.my_dummy_event_.Send(std::move(allocated_slot));

    // Then the result contains an error that the binding failed
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kBindingFailure);
}

TEST(SkeletonEventTest, CallingSendAfterPrepareOfferDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-21553375, SCR-21840370");
    RecordProperty("Description", "Checks that calling Send after offer service dispatches to the binding.");
    RecordProperty("Description", "Checks whether allocated data is sent correctly");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType test_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Send() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Send(test_value, _)).WillOnce(Return(ResultBlank{}));

    // and that PrepareStopOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and when calling Send() on the event
    const auto send_result = unit.my_dummy_event_.Send(test_value);

    // Then no error is returned
    ASSERT_TRUE(send_result.has_value());
}

TEST(SkeletonEventSendWithCopyTest, CallingSendBeforePrepareOfferReturnsError)
{
    RecordProperty("Verifies", "SCR-21840370");
    RecordProperty("Description", "Checks that calling Send before offer service returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType test_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is never called on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer()).Times(0);

    // and that Send() is never called on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, Send(test_value, _)).Times(0);

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // When calling Send() on the event before PrepareOffer
    const auto send_result = unit.my_dummy_event_.Send(test_value);

    // Then the result contains an error that the event has not been offered
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kNotOffered);
}

TEST(SkeletonEventSendWithCopyTest, CallingSendAfterPrepareOfferWhenBindingFailsReturnsError)
{
    RecordProperty("Verifies", "SCR-21840370");
    RecordProperty("Description", "Checks that calling Send after offer service propagates an error from the binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType test_value{42};

    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

    // and that PrepareOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareOffer());

    // and that Send() is called once on the event binding which returns an error
    EXPECT_CALL(skeleton_event_binding_mock, Send(test_value, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidConfiguration)));

    // and that PrepareStopOffer() is called once on the event binding
    EXPECT_CALL(skeleton_event_binding_mock, PrepareStopOffer());

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // when PrepareOffer() is called on the event
    unit.my_dummy_event_.PrepareOffer();

    // and when calling Send() on the event
    const auto send_result = unit.my_dummy_event_.Send(test_value);

    // Then the result contains an error that the binding failed
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kBindingFailure);
}

TEST(SkeletonEventTest, SkeletonEventsRegisterThemselvesWithSkeleton)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // Expecting that the SkeletonEventBindingFactory returns a valid binding
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // Expect that the event map stored in the skeleton
    const auto& events = SkeletonBaseView{unit}.GetEvents();

    // contains a single event
    ASSERT_EQ(events.size(), 1);

    const auto event_name = events.begin()->first;
    const auto& event = events.begin()->second.get();

    // the name corresponds to the correct event name
    EXPECT_EQ(event_name, kEventName);

    // and the event in the map corresponds to the correct skeleton event address
    EXPECT_EQ(&event, &unit.my_dummy_event_);
}

TEST(SkeletonEventTest, MovingConstructingSkeletonUpdatesEventMapReference)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // Expecting that the SkeletonEventBindingFactory returns a valid binding
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    MyDummySkeleton unit2{std::move(unit)};

    // Expect that the event map stored in the skeleton
    const auto& events = SkeletonBaseView{unit2}.GetEvents();

    // contains a single event
    ASSERT_EQ(events.size(), 1);

    const auto event_name = events.begin()->first;
    const auto& event = events.begin()->second.get();

    // the name corresponds to the correct event name
    EXPECT_EQ(event_name, kEventName);

    // and the event in the map corresponds to the new skeleton event address
    EXPECT_EQ(&event, &unit2.my_dummy_event_);
}

TEST(SkeletonEventTest, MovingAssigningSkeletonUpdatesEventMapReference)
{
    RuntimeMockGuard runtime_mock_guard{};
    ON_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    ServiceIdentifierType service{make_ServiceIdentifierType("foo2", 1U, 0U)};
    const ServiceInstanceDeployment instance_deployment{
        service,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    InstanceIdentifier identifier2{make_InstanceIdentifier(instance_deployment, kTypeDeployment)};

    // Expecting that the SkeletonEventBindingFactory returns a valid binding for both Skeletons
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_,
                Create(kInstanceIdWithLolaBinding, _, kEventName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_, Create(identifier2, _, kEventName))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>())));

    // Given a skeleton which has a mock skeleton-binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    MyDummySkeleton unit2{std::make_unique<mock_binding::Skeleton>(), identifier2};

    unit2 = std::move(unit);

    // Expect that the event map stored in the skeleton
    const auto& events = SkeletonBaseView{unit2}.GetEvents();

    // contains a single event
    ASSERT_EQ(events.size(), 1);

    const auto event_name = events.begin()->first;
    const auto& event = events.begin()->second.get();

    // the name corresponds to the correct event name
    EXPECT_EQ(event_name, kEventName);

    // and the event in the map corresponds to the new skeleton event address
    EXPECT_EQ(&event, &unit2.my_dummy_event_);
}

}  // namespace
}  // namespace score::mw::com::impl
