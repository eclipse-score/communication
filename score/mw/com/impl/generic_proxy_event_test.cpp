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
/// This file contains unit tests for functionality that is unique to GenericProxyEvents.
/// There is additional test functionality in the following locations:
///     - score/mw/com/impl/proxy_event_base_test.cpp contains unit tests which contain paramaterised tests to
///     re-use the ProxyEventBase tests for testing functionality common between ProxyEventBase and GenericProxyEvent
///     (i.e. type independent functionality).
///     - score/mw/com/impl/proxy_event_test.cpp contains unit tests which contain paramaterised tests to re-use
///     the ProxyEvent tests for testing functionality common between ProxyEvent and GenericProxyEvent (i.e. type
///     dependent functionality).

#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/test/proxy_resources.h"
#include "score/mw/com/types.h"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

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

const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooservice{make_ServiceIdentifierType("foo")};
const auto kInstanceSpecifier = InstanceSpecifier::Create("/dummy_instance_specifier").value();
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooservice,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{10U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};

ProxyBase kEmptyProxy(std::make_unique<mock_binding::Proxy>(),
                      make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));
const auto kEventName{"DummyEvent1"};

TEST(GenericProxyEventTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-14032718");
    RecordProperty("Description", "Checks copy semantics for GenericProxyEvent");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<GenericProxyEvent>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<GenericProxyEvent>::value, "Is wrongly copyable");
}

TEST(GenericProxyEventTest, NotMoveable)
{
    static_assert(!std::is_move_constructible<GenericProxyEvent>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<GenericProxyEvent>::value, "Is wrongly moveable");
}

TEST(GenericProxyEventTest, SamplePtrsToSlotDataAreConst)
{
    RecordProperty("Verifies", "SCR-6340729");
    RecordProperty("Description", "Proxy shall interpret slot data as const");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using SampleType = void;
    const std::size_t max_num_samples{1};

    auto mock_proxy_event_ptr = std::make_unique<StrictMock<mock_binding::GenericProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    GenericProxyEvent proxy_event{
        kEmptyProxy, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr)}, kEventName};

    EXPECT_CALL(mock_proxy_event, Subscribe(max_num_samples));
    EXPECT_CALL(mock_proxy_event, GetNewSamples(_, _));

    mock_proxy_event.PushFakeSample(1U);
    EXPECT_CALL(mock_proxy_event, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    auto get_new_samples_callback = [](SamplePtr<SampleType> ptr) noexcept {
        // A SamplePtr<void> cannot be dereferenced
        using GetSlotType = std::remove_pointer<decltype(ptr.get())>::type;
        using ArrowSlotType = typename std::remove_pointer<decltype(ptr.operator->())>::type;
        static_assert(std::is_const<GetSlotType>::value, "SamplePtr should provide manage pointer to const.");
        static_assert(std::is_const<ArrowSlotType>::value, "SamplePtr should provide manage pointer to const.");
    };

    proxy_event.Subscribe(max_num_samples);

    const auto get_result = proxy_event.GetNewSamples(get_new_samples_callback, max_num_samples);
    EXPECT_EQ(get_result.value(), 1U);
}

TEST(GenericProxyEventTest, DieOnProxyDestructionWhileHoldingSamplePtrs)
{
    using SampleType = void;
    const std::size_t max_num_samples{1};

    auto mock_proxy_event_ptr = std::make_unique<StrictMock<mock_binding::GenericProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    auto proxy_event = std::make_unique<GenericProxyEvent>(
        kEmptyProxy, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr)}, kEventName);

    EXPECT_CALL(mock_proxy_event, Subscribe(max_num_samples));
    EXPECT_CALL(mock_proxy_event, GetNewSamples(_, _));

    mock_proxy_event.PushFakeSample(3U);
    EXPECT_CALL(mock_proxy_event, GetSubscriptionState())
        .WillOnce(::testing::Return(SubscriptionState::kNotSubscribed));

    score::cpp::optional<SamplePtr<SampleType>> ptr{};
    proxy_event->Subscribe(max_num_samples);
    Result<std::size_t> num_samples = proxy_event->GetNewSamples(
        [&ptr](SamplePtr<SampleType> new_sample) {
            ptr = std::move(new_sample);
        },
        1U);
    ASSERT_TRUE(num_samples.has_value());
    ASSERT_TRUE(ptr.has_value());
    EXPECT_EQ(*num_samples, 1U);
    EXPECT_DEATH(proxy_event.reset(), ".*");
}

TEST(GenericProxyEventGetSampleSizeTest, GetSampleSizeDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that GetSampleSize will return the sample size from the binding");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t expected_sample_size{10U};

    // Given a generic proxy event based on a mock binding
    auto mock_proxy_event_ptr = std::make_unique<StrictMock<mock_binding::GenericProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    GenericProxyEvent proxy_event{
        kEmptyProxy, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr)}, kEventName};

    // Expect that GetSampleSize is called once on the binding
    EXPECT_CALL(mock_proxy_event, GetSampleSize()).WillOnce(Return(expected_sample_size));

    // When GetSampleSize is called on the proxy_event
    const auto sample_size = proxy_event.GetSampleSize();

    // Then the sample size will be the same value returned by the binding
    EXPECT_EQ(sample_size, expected_sample_size);
}

TEST(GenericProxyEventHasSerializedFormatTest, HasSerializedFormatDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-14035199");
    RecordProperty("Description", "Checks that HasSerializedFormat will return the sample size from the binding");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool expected_has_serialized_format{true};

    // Given a generic event proxy, that is connected to a mock binding
    auto mock_proxy_event_ptr = std::make_unique<StrictMock<mock_binding::GenericProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    GenericProxyEvent proxy_event{
        kEmptyProxy, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr)}, kEventName};

    // Expect that HasSerializedFormat is called once on the binding
    EXPECT_CALL(mock_proxy_event, HasSerializedFormat()).WillOnce(Return(expected_has_serialized_format));

    // When HasSerializedFormat is called on the proxy
    const auto has_serialized_format = proxy_event.HasSerializedFormat();

    // Then the result will contain the value returned by the binding
    EXPECT_EQ(has_serialized_format, expected_has_serialized_format);
}

TEST(GenericProxyEventGetNewSamplesTest, GetNewSamplesContainsCorrectReceiverSignature)
{
    RecordProperty("Verifies", "SCR-14086929");
    RecordProperty("Description", "Checks that the GetNewSamples receiver signature is correct");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ExpectedReceiverCallableType = void (*)(mw::com::SamplePtr<void>);

    const std::size_t expected_new_samples_processed{1};
    const std::size_t max_num_samples{1};

    // Given a generic event proxy, that is connected to a mock binding
    auto mock_proxy_event_ptr = std::make_unique<StrictMock<mock_binding::GenericProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    GenericProxyEvent proxy_event{
        kEmptyProxy, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr)}, kEventName};

    // Expect that GetNewSamples is called once on the binding
    EXPECT_CALL(mock_proxy_event, GetNewSamples(_, _))
        .WillOnce(Return(Result<std::size_t>{expected_new_samples_processed}));

    // and that the underlying sample reference tracker has the same number of free slot as requested
    auto& tracker = ProxyEventBaseAttorney{proxy_event}.GetSampleReferenceTracker();
    tracker.Reset(max_num_samples);

    // When GetNewSamples is called on the proxy with the expected Callable signature
    ExpectedReceiverCallableType get_new_samples_callable = [](mw::com::SamplePtr<void>) {};
    score::cpp::ignore = proxy_event.GetNewSamples(std::move(get_new_samples_callable), max_num_samples);

    // Then it compiles and doesn't crash
}

}  // namespace
}  // namespace score::mw::com::impl
