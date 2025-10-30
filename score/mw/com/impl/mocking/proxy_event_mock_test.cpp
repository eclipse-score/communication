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
#include "score/mw/com/impl/mocking/proxy_event_mock.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

auto kDummyServiceElementName = "MyDummyServiceElement";

const std::size_t kDummyMaxSampleCount{5U};
const std::size_t kDummyAvailableSamples{6U};

template <typename T>
class ProxyServiceElementMockFixture : public ::testing::Test
{
  public:
    using ProxyServiceElement = typename T::ProxyServiceElement;
    using ProxyServiceElementMock = typename T::ProxyServiceElementMock;

    ProxyServiceElementMockFixture()
    {
        unit_.InjectMock(proxy_service_element_mock_);
    }

    ProxyServiceElementMock proxy_service_element_mock_{};
    ProxyBase proxy_base_{nullptr, MakeFakeHandle(1U)};
    ProxyServiceElement unit_{proxy_base_, nullptr, kDummyServiceElementName};
};

struct ProxyEventStruct
{
    using ProxyServiceElement = ProxyEvent<TestSampleType>;
    using ProxyServiceElementMock = ProxyEventMock<TestSampleType>;
};

struct ProxyFieldStruct
{
    using ProxyServiceElement = ProxyField<TestSampleType>;
    using ProxyServiceElementMock = ProxyEventMock<TestSampleType>;
};

using MyTypes = ::testing::Types<ProxyEventStruct, ProxyFieldStruct>;
TYPED_TEST_SUITE(ProxyServiceElementMockFixture, MyTypes, );

TYPED_TEST(ProxyServiceElementMockFixture, SubscribeDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that Subscribe will be called on the mock which returns a valid result
    EXPECT_CALL(this->proxy_service_element_mock_, Subscribe(kDummyMaxSampleCount)).WillOnce(Return(ResultBlank{}));

    // When Subscribe is called on the ProxyEvent
    const auto result = this->unit_.Subscribe(kDummyMaxSampleCount);

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TYPED_TEST(ProxyServiceElementMockFixture, SubscribeReturnsErrorWhenMockReturnsError)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that Subscribe will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(this->proxy_service_element_mock_, Subscribe(kDummyMaxSampleCount))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When Subscribe is called on the ProxyEvent
    const auto result = this->unit_.Subscribe(kDummyMaxSampleCount);

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TYPED_TEST(ProxyServiceElementMockFixture, UnsubscribeDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that Unsubscribe will be called on the mock
    EXPECT_CALL(this->proxy_service_element_mock_, Unsubscribe());

    // When Unsubscribe is called on the ProxyEvent
    this->unit_.Unsubscribe();
}

TYPED_TEST(ProxyServiceElementMockFixture, GetSubscriptionStateDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetSubscriptionState will be called on the mock which returns a valid result
    const auto subscription_state = SubscriptionState::kSubscribed;
    EXPECT_CALL(this->proxy_service_element_mock_, GetSubscriptionState()).WillOnce(Return(subscription_state));

    // When GetSubscriptionState is called on the ProxyEvent
    const auto result = this->unit_.GetSubscriptionState();

    // Then the result is the same as the value returned by the mock
    EXPECT_EQ(result, subscription_state);
}

TYPED_TEST(ProxyServiceElementMockFixture, GetFreeSampleCountDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetFreeSampleCount will be called on the mock which returns a valid result
    const std::size_t free_sample_count = 3U;
    EXPECT_CALL(this->proxy_service_element_mock_, GetFreeSampleCount()).WillOnce(Return(free_sample_count));

    // When GetFreeSampleCount is called on the ProxyEvent
    const auto result = this->unit_.GetFreeSampleCount();

    // Then the result is the same as the value returned by the mock
    EXPECT_EQ(result, free_sample_count);
}

TYPED_TEST(ProxyServiceElementMockFixture, GetNumNewSamplesAvailableDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetNumNewSamplesAvailable will be called on the mock which returns a valid result

    EXPECT_CALL(this->proxy_service_element_mock_, GetNumNewSamplesAvailable())
        .WillOnce(Return(kDummyAvailableSamples));

    // When GetNumNewSamplesAvailable is called on the ProxyEvent
    const auto result = this->unit_.GetNumNewSamplesAvailable();

    // Then the result is the same as the value returned by the mock
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), kDummyAvailableSamples);
}

TYPED_TEST(ProxyServiceElementMockFixture, GetNumNewSamplesAvailableReturnsErrorWhenMockReturnsError)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetNumNewSamplesAvailable will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(this->proxy_service_element_mock_, GetNumNewSamplesAvailable())
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When GetNumNewSamplesAvailable is called on the ProxyEvent
    const auto result = this->unit_.GetNumNewSamplesAvailable();

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TYPED_TEST(ProxyServiceElementMockFixture, SetReceiveHandlerDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that SetReceiveHandler will be called on the mock which returns a valid result

    EXPECT_CALL(this->proxy_service_element_mock_, SetReceiveHandler(_)).WillOnce(Return(ResultBlank{}));

    // When SetReceiveHandler is called on the ProxyEvent
    const auto result = this->unit_.SetReceiveHandler([]() {});

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TYPED_TEST(ProxyServiceElementMockFixture, SetReceiveHandlerReturnsErrorWhenMockReturnsError)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that SetReceiveHandler will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(this->proxy_service_element_mock_, SetReceiveHandler(_))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When SetReceiveHandler is called on the ProxyEvent
    const auto result = this->unit_.SetReceiveHandler([]() {});

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TYPED_TEST(ProxyServiceElementMockFixture, UnsetReceiveHandlerDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that UnsetReceiveHandler will be called on the mock which returns a valid result

    EXPECT_CALL(this->proxy_service_element_mock_, UnsetReceiveHandler()).WillOnce(Return(ResultBlank{}));

    // When UnsetReceiveHandler is called on the ProxyEvent
    const auto result = this->unit_.UnsetReceiveHandler();

    // Then the result is valid
    ASSERT_TRUE(result.has_value());
}

TYPED_TEST(ProxyServiceElementMockFixture, UnsetReceiveHandlerReturnsErrorWhenMockReturnsError)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that UnsetReceiveHandler will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(this->proxy_service_element_mock_, UnsetReceiveHandler())
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When UnsetReceiveHandler is called on the ProxyEvent
    const auto result = this->unit_.UnsetReceiveHandler();

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TYPED_TEST(ProxyServiceElementMockFixture, GetNewSamplesDispatchesToMockAfterInjectingMock)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetNewSamples will be called on the mock which returns a valid result
    const std::size_t number_of_receiver_calls{10U};
    EXPECT_CALL(this->proxy_service_element_mock_, GetNewSamples(_, kDummyMaxSampleCount))
        .WillOnce(Return(number_of_receiver_calls));

    // When GetNewSamples is called on the ProxyEvent
    auto result = this->unit_.GetNewSamples([](SamplePtr<TestSampleType>) {}, kDummyMaxSampleCount);

    // Then the result contains the return value that was returned by the mock
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), number_of_receiver_calls);
}

TYPED_TEST(ProxyServiceElementMockFixture, GetNewSamplesReturnsErrorWhenMockReturnsError)
{
    // Given a ProxyEvent constructed with an empty binding and an injected mock

    // Expecting that GetNewSamples will be called on the mock which returns an error
    const auto error_code = ComErrc::kNotOffered;
    EXPECT_CALL(this->proxy_service_element_mock_, GetNewSamples(_, _)).WillOnce(Return(MakeUnexpected(error_code)));

    // When GetNewSamples is called on the ProxyEvent
    auto result = this->unit_.GetNewSamples([](SamplePtr<TestSampleType>) {}, kDummyMaxSampleCount);

    // Then the result contains the same error code that was returned by the mock
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

}  // namespace
}  // namespace score::mw::com::impl
