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
#include "score/mw/com/doc/tutorial/chapter_12/consumer/proxy_component.h"

#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/test_types.h"

#include <gtest/gtest.h>

using testing::_;
using testing::Invoke;
using testing::Return;

class ProxyComponentTestFixture : public ::testing::Test
{
  protected:
    using ProxyComponent = score::mw::com::tutorial::ProxyComponent;
    using MessageEventMock = score::mw::com::ProxyEventMock<score::mw::com::tutorial::HelloWorldProxy::FixedSizeString>;

    void SetUp() override
    {
        auto proxy =
            score::mw::com::ProxyWrapperClassTestView<score::mw::com::tutorial::HelloWorldProxy>::Create(events_tuple_);
        // Note: This additional (redundant as we handed it over in the Create() already) "inject" of the event mock
        // into the proxy mock is currently needed, but will be removed in the future.
        proxy.message.InjectMock(message_event_mock());

        proxy_component_.emplace(std::move(proxy));
    }

    MessageEventMock& message_event_mock()
    {
        return std::get<0>(events_tuple_).mock;
    }

    std::tuple<score::mw::com::NamedProxyEventMock<score::mw::com::tutorial::HelloWorldProxy::FixedSizeString>>
        events_tuple_{"message"};
    std::optional<ProxyComponent> proxy_component_{};
};

TEST_F(ProxyComponentTestFixture, SubscribeReturnsTrueWhenEventSubscribeSucceeds)
{
    // Given a mock for our message event
    auto& event_mock = message_event_mock();

    // Expect, that Subscribe with max_samples = 1 is called on the message ProxyEvent and it returns success
    EXPECT_CALL(event_mock, Subscribe(1)).WillOnce(Return(score::Result<void>{}));

    // when we call Subscribe on the unit under test
    auto subscribe_result = proxy_component_->Subscribe();
    // and that the result of the Subscribe is true.
    EXPECT_TRUE(subscribe_result);
}

TEST_F(ProxyComponentTestFixture, SubscribeReturnsFalseWhenEventSubscribeFails)
{
    // Given a mock for our message event
    auto& event_mock = message_event_mock();

    // Expect, that Subscribe with max_samples = 1 is called on the message ProxyEvent, and it returns a binding failure
    EXPECT_CALL(event_mock, Subscribe(1))
        .WillOnce(Return(score::MakeUnexpected(score::mw::com::ComErrc::kBindingFailure)));

    // when we call Subscribe on the unit under test
    auto subscribe_result = proxy_component_->Subscribe();
    // and that the result of the Subscribe is false.
    EXPECT_FALSE(subscribe_result);
}

TEST_F(ProxyComponentTestFixture, ReceiveCounterIncrementedWhenGetNewSamplesSucceeds)
{
    // Given a mock for our message event
    auto& event_mock = message_event_mock();

    // expect, that a call to ProxyEvent::GetNewSamples takes place with a max_samples_count being 1
    EXPECT_CALL(event_mock, GetNewSamples(_, 1))
        .WillOnce(Invoke([](auto&& receiver, std::size_t) -> score::Result<std::size_t> {
            // Simulate receiving a sample by invoking the receiver callback with a dummy sample containing "Hello
            // World". This isn't checked by our UuT - but we simply show the complete steps, how to create a SamplePtr
            // in tests and fill it with data.

            // Creating a unique_ptr pointing at the sample-data
            constexpr std::string_view kHelloWorld{"Hello World"};
            auto sample_unique_ptr = std::make_unique<score::mw::com::tutorial::HelloWorldProxy::FixedSizeString>();
            std::memcpy(sample_unique_ptr->data(), kHelloWorld.data(), kHelloWorld.size());

            // Creating a fake SamplePtr from the unique_ptr
            auto sample_ptr = score::mw::com::MakeFakeSamplePtr(std::move(sample_unique_ptr));
            // Calling the user provided (proxy component provided) receiver.
            receiver(std::move(sample_ptr));
            // this is the return of the GetNewSamples call - indicating how many samples have been received (how many
            // times the receiver callback has been called)
            return 1;
        }));

    // when we call GetNewSamples on the unit under test
    proxy_component_->GetNewSamples();
    // and the receive counter is incremented to 1
    EXPECT_EQ(proxy_component_->GetReceiveCounter(), 1U);
}
