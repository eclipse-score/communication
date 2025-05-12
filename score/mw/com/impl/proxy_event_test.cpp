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
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/proxy_resources.h"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestSampleType = std::uint16_t;

const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooservice{make_ServiceIdentifierType("foo")};
const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooservice,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{10U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};

ProxyBase kEmptyProxy(std::make_unique<mock_binding::Proxy>(),
                      make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));
const auto kEventName{"DummyEvent1"};

/// \brief Function that returns the value pointed to by a pointer
template <typename T>
T GetSamplePtrValue(const T* const sample_ptr)
{
    return *sample_ptr;
}

/// \brief Function that casts and returns the value pointed to by a void pointer
///
/// Assumes that the object in memory being pointed to is of type TestSampleType.
TestSampleType GetSamplePtrValue(const void* const void_ptr)
{
    auto* const typed_ptr = static_cast<const TestSampleType*>(void_ptr);
    return *typed_ptr;
}

/// \brief Structs containing types for templated gtests.
///
/// We use a struct instead of a tuple since a tuple cannot contain a void type.
struct ProxyEventStruct
{
    using SampleType = TestSampleType;
    using ProxyEventType = ProxyEvent<TestSampleType>;
    using MockProxyEventType = StrictMock<mock_binding::ProxyEvent<TestSampleType>>;
};
struct GenericProxyEventStruct
{
    using SampleType = void;
    using ProxyEventType = GenericProxyEvent;
    using MockProxyEventType = StrictMock<mock_binding::GenericProxyEvent>;
};
struct ProxyFieldStruct
{
    using SampleType = TestSampleType;
    using ProxyEventType = ProxyField<TestSampleType>;
    using MockProxyEventType = StrictMock<mock_binding::ProxyEvent<TestSampleType>>;
};

/// \brief Templated test fixture for ProxyEvent functionality that works for both ProxyEvent and GenericProxyEvent
///
/// \tparam T A tuple containing:
///     SampleType either a type such as std::uint32_t or void
///     ProxyEventType either ProxyEvent or GenericProxyEvent
///     MockProxyEventType either mock_binding::ProxyEvent<TestSampleType> or mock_binding::GenericProxyEvent
template <typename T>
class ProxyEventFixture : public ::testing::Test
{
  protected:
    using SampleType = typename T::SampleType;
    using ProxyEventType = typename T::ProxyEventType;
    using MockProxyEventType = typename T::MockProxyEventType;

    ProxyEventFixture()
        : mock_proxy_event_ptr_{std::make_unique<MockProxyEventType>()},
          mock_proxy_event_{*mock_proxy_event_ptr_},
          proxy_event_{kEmptyProxy, std::move(mock_proxy_event_ptr_), kEventName}
    {
    }

    std::unique_ptr<MockProxyEventType> mock_proxy_event_ptr_;
    MockProxyEventType& mock_proxy_event_;
    ProxyEventType proxy_event_;
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

// Gtest will run all tests in the LolaProxyEventFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::Types<ProxyEventStruct, GenericProxyEventStruct, ProxyFieldStruct>;
TYPED_TEST_SUITE(ProxyEventFixture, MyTypes, );

template <typename T>
using ProxyEventGetNewSamplesFixture = ProxyEventFixture<T>;

TYPED_TEST_SUITE(ProxyEventGetNewSamplesFixture, MyTypes, );

TYPED_TEST(ProxyEventFixture, ReceiveDataFromProxy)
{
    using Base = ProxyEventFixture<TypeParam>;

    // SWS_CM_00141, SWS_CM_00701, SWS_CM_00702, SWS_CM_00703, SWS_CM_00704, SWS_CM_00705, SWS_CM_00706, SWS_CM_00707
    Base::RecordProperty("Verifies",
                         ""
                         "SCR-7822488, SCR-14035773, SCR-21350367");
    Base::RecordProperty("Description", "Checks event proxy by simulating an skeleton event");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    Base::mock_proxy_event_.PushFakeSample(4242);

    EXPECT_CALL(Base::mock_proxy_event_, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    EXPECT_CALL(Base::mock_proxy_event_, Subscribe(7)).Times(1);
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _)).Times(2);
    EXPECT_CALL(Base::mock_proxy_event_, GetNumNewSamplesAvailable()).WillOnce(Return(1));

    Base::proxy_event_.Subscribe(7U);
    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 7U);
    EXPECT_EQ(*Base::proxy_event_.GetNumNewSamplesAvailable(), 1U);
    const auto num_samples_result = Base::proxy_event_.GetNewSamples(
        [](SamplePtr<typename Base::SampleType> sample) {
            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, 4242);
        },
        1U);
    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 7U);
    ASSERT_TRUE(num_samples_result.has_value());
    EXPECT_EQ(*num_samples_result, 1U);

    const auto num_samples_result_empty = Base::proxy_event_.GetNewSamples(
        [](SamplePtr<typename Base::SampleType>) noexcept {
            FAIL() << "GetNewSamples returned a sample despite being empty.";
        },
        1U);
    ASSERT_TRUE(num_samples_result_empty.has_value());
    EXPECT_EQ(*num_samples_result_empty, 0U);
}

TYPED_TEST(ProxyEventFixture, SamplePtrLimitsAreEnforced)
{
    using Base = ProxyEventFixture<TypeParam>;

    Base::RecordProperty(
        "Verifies",
        "SCR-6367376, SCR-6342893, SCR-6225130, SCR-6338237, SCR-6340966, SCR-14035773, SCR-21350367, SCR-21803116");
    Base::RecordProperty("Description",
                         "Checks event proxy by simulating an skeleton event. Also checks that GetFreeSampleCount "
                         "correctly reflects the number of samples that can be received by the user.");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_num_samples{2};

    EXPECT_CALL(Base::mock_proxy_event_, Subscribe(max_num_samples));
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _)).Times(2);

    Base::mock_proxy_event_.PushFakeSample(1U);
    Base::mock_proxy_event_.PushFakeSample(2U);
    Base::mock_proxy_event_.PushFakeSample(3U);
    EXPECT_CALL(Base::mock_proxy_event_, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    std::vector<SamplePtr<typename Base::SampleType>> samples{};
    unsigned int num_callback_calls{0};
    auto get_new_samples_callback = [&samples, &num_callback_calls](SamplePtr<typename Base::SampleType> ptr) {
        samples.push_back(std::move(ptr));
        ++num_callback_calls;
    };

    Base::proxy_event_.Subscribe(max_num_samples);

    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 2U);
    const auto get_result = Base::proxy_event_.GetNewSamples(get_new_samples_callback, 3U);
    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 0U);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(*get_result, 2U);
    EXPECT_EQ(num_callback_calls, *get_result);
    EXPECT_EQ(samples.size(), 2U);

    for (const auto val : {2U, 3U})
    {
        EXPECT_NE(std::find_if(samples.cbegin(),
                               samples.cend(),
                               [val](const SamplePtr<typename Base::SampleType>& ptr) {
                                   const auto value = GetSamplePtrValue(ptr.get());
                                   return value == val;
                               }),
                  samples.cend());
    }

    Base::mock_proxy_event_.PushFakeSample(4U);
    const auto get_new_samples_max_samples_reached_result =
        Base::proxy_event_.GetNewSamples(get_new_samples_callback, 1U);
    ASSERT_FALSE(get_new_samples_max_samples_reached_result.has_value());
    EXPECT_EQ(get_new_samples_max_samples_reached_result.error(), ComErrc::kMaxSamplesReached);

    samples.clear();
    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 2U);
    num_callback_calls = 0;
    const auto get_result_2 = Base::proxy_event_.GetNewSamples(get_new_samples_callback, 1U);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(*get_result_2, 1U);
    EXPECT_EQ(Base::proxy_event_.GetFreeSampleCount(), 1U);
    ASSERT_EQ(samples.size(), 1U);
    EXPECT_EQ(GetSamplePtrValue(samples[0].get()), 4U);
    EXPECT_EQ(num_callback_calls, *get_result_2);
}

TYPED_TEST(ProxyEventFixture, DieOnUnsubscribingWhileHoldingSamplePtrs)
{
    using Base = ProxyEventFixture<TypeParam>;

    Base::RecordProperty("Verifies", "SCR-5898007");  // SWS_CM_00151
    Base::RecordProperty("Description",
                         "Checks whether if a user unsubscribes an event, which still holds sample ptr, that we die.");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_num_samples{1};

    EXPECT_CALL(Base::mock_proxy_event_, Subscribe(max_num_samples));
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _));
    EXPECT_CALL(Base::mock_proxy_event_, Unsubscribe()).Times(::testing::AtMost(1));

    Base::mock_proxy_event_.PushFakeSample(3U);
    EXPECT_CALL(Base::mock_proxy_event_, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    score::cpp::optional<SamplePtr<typename Base::SampleType>> ptr{};
    Base::proxy_event_.Subscribe(max_num_samples);
    Result<std::size_t> num_samples = Base::proxy_event_.GetNewSamples(
        [&ptr](SamplePtr<typename Base::SampleType> new_sample) {
            ptr = std::move(new_sample);
        },
        1U);
    ASSERT_TRUE(num_samples.has_value());
    ASSERT_TRUE(ptr.has_value());
    EXPECT_EQ(*num_samples, 1U);
    EXPECT_DEATH(Base::proxy_event_.Unsubscribe(), ".*");
}

TYPED_TEST(ProxyEventFixture, UnsubscribeWhileNotHoldingSamplePtrs)
{
    using Base = ProxyEventFixture<TypeParam>;

    Base::mock_proxy_event_.PushFakeSample(3U);
    EXPECT_CALL(Base::mock_proxy_event_, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));
    Base::proxy_event_.Unsubscribe();

    // Then nothing bad happens
}

TYPED_TEST(ProxyEventFixture, CanConstructUnboundProxy)
{
    using Base = ProxyEventFixture<TypeParam>;

    // SWS_CM_00141, SWS_CM_00701, SWS_CM_00702
    Base::RecordProperty("Verifies", "SCR-5898016");
    Base::RecordProperty("Description", "Checks event proxy by simulating an skeleton event");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEvent<typename Base::SampleType> proxy{kEmptyProxy, nullptr, kEventName};
    // Nothing bad happens
}

TYPED_TEST(ProxyEventGetNewSamplesFixture, GetNewSamplesDispatchesToBinding)
{
    using Base = ProxyEventGetNewSamplesFixture<TypeParam>;

    Base::RecordProperty("Verifies", "SCR-14034910, SCR-14137273, SCR-17292401, SCR-14035773, SCR-21350367");
    Base::RecordProperty("Description", "Checks that GetNewSamples dispatches to the binding");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t expected_new_samples_processed{1};
    const std::size_t max_num_samples{1};

    // Given an event proxy that is connected to a mock binding

    // Expect that GetNewSamples is called once on the binding
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _))
        .WillOnce(Return(Result<std::size_t>{expected_new_samples_processed}));

    // and that the underlying sample reference tracker has the same number of free slot as requested
    auto& tracker = ProxyEventBaseAttorney{Base::proxy_event_}.GetSampleReferenceTracker();
    tracker.Reset(max_num_samples);

    // When GetNewSamples is called on the proxy
    const auto new_samples_processed_result =
        Base::proxy_event_.GetNewSamples([](SamplePtr<typename Base::SampleType>) noexcept {}, max_num_samples);

    // Then the result will contain the number of sample processed returned by the binding
    ASSERT_TRUE(new_samples_processed_result.has_value());
    EXPECT_EQ(new_samples_processed_result.value(), expected_new_samples_processed);
}

TYPED_TEST(ProxyEventGetNewSamplesFixture, GetNewSamplesReturnsErrorIfMaxSamplesAlreadyReached)
{
    using Base = ProxyEventGetNewSamplesFixture<TypeParam>;

    Base::RecordProperty("Verifies", "SCR-14034910, SCR-14137273, SCR-17292401, SCR-14035773, SCR-21350367");
    Base::RecordProperty("Description",
                         "Checks that GetNewSamples will return an error if the max samples has already been reached");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_num_samples{1};

    // Given an event proxy that is connected to a mock binding

    // Expect that the underlying sample reference tracker has no free slots
    auto& tracker = ProxyEventBaseAttorney{Base::proxy_event_}.GetSampleReferenceTracker();
    tracker.Reset(0);

    // When GetNewSamples is called on the proxy
    const auto new_samples_processed_result =
        Base::proxy_event_.GetNewSamples([](SamplePtr<typename Base::SampleType>) noexcept {}, max_num_samples);

    // Then the result will contain an error that the max samples has been reached
    ASSERT_FALSE(new_samples_processed_result.has_value());
    EXPECT_EQ(new_samples_processed_result.error(), ComErrc::kMaxSamplesReached);
}

TYPED_TEST(ProxyEventGetNewSamplesFixture, GetNewSamplesReturnsErrorIfNotSubscribed)
{
    using Base = ProxyEventGetNewSamplesFixture<TypeParam>;

    Base::RecordProperty("Verifies", "SCR-14034910, SCR-14137273, SCR-17292401");
    Base::RecordProperty("Description",
                         "Checks that GetNewSamples will forward an error kNotSubscribed from the binding");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_num_samples{1};

    // Given an event proxy that is connected to a mock binding

    // Expect that GetNewSamples is called once on the binding and returns an error code that it's not currently
    // subscribed
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _)).WillOnce(Return(MakeUnexpected(ComErrc::kNotSubscribed)));

    // and that the underlying sample reference tracker has the same number of free slot as requested
    auto& tracker = ProxyEventBaseAttorney{Base::proxy_event_}.GetSampleReferenceTracker();
    tracker.Reset(max_num_samples);

    // When GetNewSamples is called on the proxy
    const auto new_samples_processed_result =
        Base::proxy_event_.GetNewSamples([](SamplePtr<typename Base::SampleType>) noexcept {}, max_num_samples);

    // Then the result will contain an error that it's not currently subscribed
    ASSERT_FALSE(new_samples_processed_result.has_value());
    EXPECT_EQ(new_samples_processed_result.error(), ComErrc::kNotSubscribed);
}

TYPED_TEST(ProxyEventGetNewSamplesFixture, GetNewSamplesReturnsErrorFromBinding)
{
    using Base = ProxyEventGetNewSamplesFixture<TypeParam>;

    Base::RecordProperty("Verifies", "SCR-14034910, SCR-14137273, SCR-17292401");
    Base::RecordProperty(
        "Description",
        "Checks that GetNewSamples will return kBindingFailure for a generic error code from the binding");
    Base::RecordProperty("TestType", "Requirements-based test");
    Base::RecordProperty("Priority", "1");
    Base::RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_num_samples{1};

    // Given an event proxy that is connected to a mock binding

    // Expect that GetNewSamples is called once on the binding and returns an error code
    EXPECT_CALL(Base::mock_proxy_event_, GetNewSamples(_, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kInvalidConfiguration)));

    // and that the underlying sample reference tracker has the same number of free slot as requested
    auto& tracker = ProxyEventBaseAttorney{Base::proxy_event_}.GetSampleReferenceTracker();
    tracker.Reset(max_num_samples);

    // When GetNewSamples is called on the proxy
    const auto new_samples_processed_result =
        Base::proxy_event_.GetNewSamples([](SamplePtr<typename Base::SampleType>) noexcept {}, max_num_samples);

    // Then the result will contain an error that the binding failed
    ASSERT_FALSE(new_samples_processed_result.has_value());
    EXPECT_EQ(new_samples_processed_result.error(), ComErrc::kBindingFailure);
}

TEST(ProxyEventTest, SamplePtrsToSlotDataAreConst)
{
    RecordProperty("Verifies", "SCR-6340729");
    RecordProperty("Description", "Proxy shall interpret slot data as const");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SampleType = std::uint16_t;
    const std::size_t max_num_samples{1};

    auto mock_proxy_ptr = std::make_unique<StrictMock<mock_binding::ProxyEvent<SampleType>>>();
    auto& mock_proxy = *mock_proxy_ptr;
    ProxyEvent<SampleType> proxy{
        kEmptyProxy, std::unique_ptr<ProxyEventBinding<SampleType>>{std::move(mock_proxy_ptr)}, kEventName};

    EXPECT_CALL(mock_proxy, Subscribe(max_num_samples));
    EXPECT_CALL(mock_proxy, GetNewSamples(_, _));

    mock_proxy.PushFakeSample(1U);
    EXPECT_CALL(mock_proxy, GetSubscriptionState()).WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    auto get_new_samples_callback = [](SamplePtr<SampleType> ptr) noexcept {
        using GetSlotType = typename std::remove_pointer<decltype(ptr.get())>::type;
        using DerefSlotType = typename std::remove_reference<decltype(ptr.operator*())>::type;
        using ArrowSlotType = typename std::remove_pointer<decltype(ptr.operator->())>::type;
        static_assert(std::is_const<GetSlotType>::value, "SamplePtr should provide manage pointer to const.");
        static_assert(std::is_const<DerefSlotType>::value, "SamplePtr should provide manage pointer to const.");
        static_assert(std::is_const<ArrowSlotType>::value, "SamplePtr should provide manage pointer to const.");
    };

    proxy.Subscribe(max_num_samples);

    const auto get_result = proxy.GetNewSamples(get_new_samples_callback, max_num_samples);
    EXPECT_EQ(get_result.value(), 1U);
}

TEST(ProxyEventTest, DieOnProxyDestructionWhileHoldingSamplePtrs)
{
    using SampleType = std::uint16_t;
    const std::size_t max_num_samples{1};

    auto mock_proxy_ptr = std::make_unique<StrictMock<mock_binding::ProxyEvent<SampleType>>>();
    auto& mock_proxy = *mock_proxy_ptr;
    auto proxy = std::make_unique<ProxyEvent<SampleType>>(
        kEmptyProxy, std::unique_ptr<ProxyEventBinding<SampleType>>{std::move(mock_proxy_ptr)}, kEventName);

    EXPECT_CALL(mock_proxy, Subscribe(max_num_samples));
    EXPECT_CALL(mock_proxy, GetNewSamples(_, _));

    mock_proxy.PushFakeSample(3U);
    EXPECT_CALL(mock_proxy, GetSubscriptionState()).WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    score::cpp::optional<SamplePtr<SampleType>> ptr{};
    proxy->Subscribe(max_num_samples);
    Result<std::size_t> num_samples = proxy->GetNewSamples(
        [&ptr](SamplePtr<SampleType> new_sample) {
            ptr = std::move(new_sample);
        },
        1U);
    ASSERT_TRUE(num_samples.has_value());
    ASSERT_TRUE(ptr.has_value());
    EXPECT_EQ(*num_samples, 1U);
    EXPECT_DEATH(proxy.reset(), ".*");
}

TEST(ProxyEventTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-5897862, SCR-14137269");  // SWS_CM_00134
    RecordProperty("Description", "Checks copy semantics for ProxyEvents");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SampleType = std::uint16_t;
    static_assert(!std::is_copy_constructible<ProxyEvent<SampleType>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<ProxyEvent<SampleType>>::value, "Is wrongly copyable");
}

TEST(ProxyEventTest, IsMoveable)
{
    RecordProperty("Verifies", "SCR-5897869");  // SWS_CM_00135
    RecordProperty("Description", "Checks move semantics for ProxyEvents");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SampleType = std::uint16_t;
    static_assert(std::is_move_constructible<ProxyEvent<SampleType>>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<ProxyEvent<SampleType>>::value, "Is not move assignable");
}

TEST(ProxyEventTest, ClassTypeDependsOnEventDataType)
{
    RecordProperty("Verifies", "SCR-29235350");
    RecordProperty("Description", "ProxyEvents with different field data types should be different classes.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using FirstProxyEventType = ProxyEvent<bool>;
    using SecondProxyEventType = ProxyEvent<std::uint16_t>;
    static_assert(!std::is_same_v<FirstProxyEventType, SecondProxyEventType>,
                  "Class type does not depend on field data type");
}

TEST(ProxyEventTest, ProxyEventContainsPublicSampleType)
{
    RecordProperty("Verifies", "SCR-14137294");
    RecordProperty("Description",
                   "A ProxyEvent contains a public member type SampleType which denotes the type of the event.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using CustomSampleType = std::uint16_t;
    static_assert(std::is_same<typename ProxyEvent<CustomSampleType>::SampleType, CustomSampleType>::value,
                  "Incorrect SampleType.");
}

}  // namespace
}  // namespace score::mw::com::impl
