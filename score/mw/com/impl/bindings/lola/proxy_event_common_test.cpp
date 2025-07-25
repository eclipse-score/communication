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
/// This file contains tests for the functionality within ProxyEventCommon which is the type-independent functionality
/// that is used by both ProxyEvent and GenericProxyEvent. Since ProxyEvent and GenericProxyEvent dispatch directly to
/// ProxyEventCommon for these functions, we can test the functionality for both in this file. However, to ensure that
/// we have correct code coverage for ProxyEvent and GenericProxyEvent, we use templated tests and test the calls on
/// ProxyEvent and GenericProxyEvent rather than directly on ProxyEventCommon.

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using TestSampleType = std::uint16_t;

using namespace ::score::memory::shared;
using ::testing::Return;

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::ReturnRef;

template <typename R, typename... Args>
std::shared_ptr<ScopedEventReceiveHandler> FromMockFunction(safecpp::Scope<>& event_receive_handler_scope,
                                                            ::testing::MockFunction<R(Args...)>& mock_function)
{
    return std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope, [&mock_function](Args&&... args) {
        mock_function.Call(std::forward<Args>(args)...);
    });
}

const std::string kEventName{"dummy_event"};
const ElementFqId kElementFqId{0xcdef, 0x5, 0x10, ServiceElementType::EVENT};
const InstanceSpecifier kInstanceSpecifier{InstanceSpecifier::Create("/my_dummy_instance_specifier").value()};
const std::size_t kMaxNumSlots{5U};
const std::uint8_t kMaxSubscribers{10U};

LolaServiceInstanceDeployment kShmBinding{LolaServiceInstanceId{kElementFqId.instance_id_}};
ServiceIdentifierType kServiceIdentifier = make_ServiceIdentifierType("foo");
LolaServiceTypeDeployment kLolaServiceTypeDeployment{0xcdef};
ServiceTypeDeployment kServiceTypeDeployment = ServiceTypeDeployment{kLolaServiceTypeDeployment};
ServiceInstanceDeployment kServiceInstanceDeployment =
    ServiceInstanceDeployment{kServiceIdentifier, kShmBinding, QualityType::kASIL_QM, kInstanceSpecifier};
InstanceIdentifier kInstanceIdentifier = make_InstanceIdentifier(kServiceInstanceDeployment, kServiceTypeDeployment);

template <typename T>
class LolaProxyEventCommonFixture : public ProxyMockedMemoryFixture
{
  public:
    LolaProxyEventCommonFixture()
    {
        EXPECT_CALL(event_handler_, Call()).Times(0);

        InitialiseDummySkeletonEvent(kElementFqId, SkeletonEventProperties{kMaxNumSlots, kMaxSubscribers, true});
    }

    void InitialiseProxyAndEvent() noexcept
    {
        InitialiseProxyWithConstructor(kInstanceIdentifier);
        proxy_event_ = std::make_unique<T>(*proxy_, kElementFqId, kEventName);
    }

    void ExpectCallbackRegistration()
    {
        constexpr const IMessagePassingService::HandlerRegistrationNoType my_handler_no = 37U;

        EXPECT_CALL(event_handler_, Call());

        EXPECT_CALL(*mock_service_, RegisterEventNotification(QualityType::kASIL_QM, kElementFqId, _, kDummyPid))
            .WillOnce(
                ::testing::Invoke([&](auto, auto, std::weak_ptr<ScopedEventReceiveHandler> handler_weak_ptr, auto) {
                    auto handler_shared_ptr = handler_weak_ptr.lock();
                    EXPECT_TRUE(handler_shared_ptr);
                    if (handler_shared_ptr)
                    {
                        (*handler_shared_ptr)();
                    }
                    return my_handler_no;
                }));
        EXPECT_CALL(*mock_service_,
                    UnregisterEventNotification(QualityType::kASIL_QM, kElementFqId, my_handler_no, kDummyPid));
    }

  protected:
    ::testing::MockFunction<void()> event_handler_{};
    std::unique_ptr<T> proxy_event_{nullptr};
    TransactionLogId transaction_log_id_{kDummyUid};
};

// Gtest will run all tests in the LolaProxyEventFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::Types<ProxyEvent<TestSampleType>, GenericProxyEvent>;
TYPED_TEST_SUITE(LolaProxyEventCommonFixture, MyTypes, );

TYPED_TEST(LolaProxyEventCommonFixture, RegisterEventHandlerBeforeSubscription)
{
    safecpp::Scope<> event_receive_handler_scope{};

    this->InitialiseProxyAndEvent();
    const std::size_t max_sample_count{1U};
    this->ExpectCallbackRegistration();
    auto mocked_receive_handler = FromMockFunction(event_receive_handler_scope, this->event_handler_);
    this->proxy_event_->SetReceiveHandler(mocked_receive_handler);
    this->proxy_event_->Subscribe(max_sample_count);
}

TYPED_TEST(LolaProxyEventCommonFixture, RegisterEventHandlerAfterSubscription)
{
    safecpp::Scope<> event_receive_handler_scope{};

    this->InitialiseProxyAndEvent();
    const std::size_t max_sample_count{1U};
    this->ExpectCallbackRegistration();
    this->proxy_event_->Subscribe(max_sample_count);
    this->proxy_event_->SetReceiveHandler(FromMockFunction(event_receive_handler_scope, this->event_handler_));
}

TYPED_TEST(LolaProxyEventCommonFixture, DoNotRegisterEventHandler)
{
    this->InitialiseProxyAndEvent();
    this->proxy_event_->Subscribe(1U);

    EXPECT_EQ(this->proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
}

TYPED_TEST(LolaProxyEventCommonFixture, SubscriptionFailsWhenProviderRejectsSubscription)
{
    this->RecordProperty("Verifies", "SCR-21269964, SCR-14137270, SCR-17292398, SCR-14033248");
    this->RecordProperty("Description",
                         "Checks that a subscription will fail when the provider rejects the subscription due to "
                         "overflowed max sample count.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->InitialiseProxyAndEvent();

    // When we subscribe requresting too many samples
    const auto subscribe_result = this->proxy_event_->Subscribe(kMaxNumSlots + 1U);

    // Then the subscribe call should return an error
    ASSERT_FALSE(subscribe_result.has_value());
    EXPECT_EQ(subscribe_result.error(), ComErrc::kMaxSampleCountNotRealizable);

    // And we stay in not subscribed state
    EXPECT_EQ(this->proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventCommonFixture, UnsubscribeImmediatelyAfterSubscribing)
{
    this->RecordProperty("Verifies", "SCR-14033377, SCR-17292399, SCR-14137271, SCR-21286218");
    this->RecordProperty("Description",
                         "Unsubscribe will be succesfully processed if a user unsubscribes from an event immediately "
                         "after subscribing.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_sample_count{1U};

    // Given a proxy that unsubscribes while waiting for being subscribed correctly
    this->InitialiseProxyAndEvent();

    // When we subscribe (sending a subscribe message to the producer)
    this->proxy_event_->Subscribe(max_sample_count);

    // and we unsubscribe before the producer sends a response that it has changed state
    this->proxy_event_->Unsubscribe();

    // And we stay in not subscribed state
    EXPECT_EQ(this->proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventCommonFixture, UnsubscribingWillUnregisterEventHandler)
{
    this->RecordProperty("Verifies", "SCR-21293524");
    this->RecordProperty(
        "Description",
        "Checks that calling Unsubscribe while currently subscribed will unregister a registered event "
        "receive handler.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_sample_count{1U};
    constexpr const IMessagePassingService::HandlerRegistrationNoType my_handler_no = 37U;
    bool handler_unregistered{false};

    // Expecting that a receive handler will be registered
    EXPECT_CALL(*this->mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, kElementFqId, _, this->kDummyPid))
        .WillOnce(Return(my_handler_no));

    // and the same receive handler will be unregistered
    EXPECT_CALL(*this->mock_service_,
                UnregisterEventNotification(QualityType::kASIL_QM, kElementFqId, my_handler_no, this->kDummyPid))
        .WillOnce(InvokeWithoutArgs([&handler_unregistered]() {
            handler_unregistered = true;
        }));

    // Given a proxy that unsubscribes while waiting for being subscribed correctly
    this->InitialiseProxyAndEvent();

    // When we subscribe (sending a subscribe message to the producer)
    this->proxy_event_->Subscribe(max_sample_count);

    // and Register a receive handler
    safecpp::Scope<> event_receive_handler_scope{};
    this->proxy_event_->SetReceiveHandler(FromMockFunction(event_receive_handler_scope, this->event_handler_));

    // Then the receive handler should not be unregistered
    EXPECT_FALSE(handler_unregistered);

    // and when the ProxyEvent unsubscribes
    this->proxy_event_->Unsubscribe();

    // Then the receive handler should be unregistered
    EXPECT_TRUE(handler_unregistered);
}

TYPED_TEST(LolaProxyEventCommonFixture, DestroyingTheProxyEventWillUnregisterEventHandler)
{
    this->RecordProperty("Verifies", "SCR-20236391, SCR-20237033");
    this->RecordProperty(
        "Description",
        "Checks that a registered event handler will be unregistered when the (generic) proxy event is destroyed.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_sample_count{1U};
    constexpr const IMessagePassingService::HandlerRegistrationNoType my_handler_no = 37U;
    bool handler_unregistered{false};

    // Expecting that a receive handler will be registered
    EXPECT_CALL(*this->mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, kElementFqId, _, this->kDummyPid))
        .WillOnce(Return(my_handler_no));

    // and the same receive handler will be unregistered
    EXPECT_CALL(*this->mock_service_,
                UnregisterEventNotification(QualityType::kASIL_QM, kElementFqId, my_handler_no, this->kDummyPid))
        .WillOnce(InvokeWithoutArgs([&handler_unregistered]() {
            handler_unregistered = true;
        }));

    // Given a proxy that unsubscribes while waiting for being subscribed correctly
    this->InitialiseProxyAndEvent();

    // When we subscribe (sending a subscribe message to the producer)
    this->proxy_event_->Subscribe(max_sample_count);

    // and Register a receive handler
    safecpp::Scope<> event_receive_handler_scope{};
    this->proxy_event_->SetReceiveHandler(FromMockFunction(event_receive_handler_scope, this->event_handler_));

    // Then the receive handler should not be unregistered
    EXPECT_FALSE(handler_unregistered);

    // and when the ProxyEvent is destroyed
    this->proxy_event_.reset();

    // Then the receive handler should be unregistered
    EXPECT_TRUE(handler_unregistered);
}

TYPED_TEST(LolaProxyEventCommonFixture, DoubleSubscribe)
{
    const std::size_t max_sample_count{kMaxNumSlots / 2U};

    // Given a valid proxy that is already subscribed
    this->InitialiseProxyAndEvent();
    const auto subscribe_result = this->proxy_event_->Subscribe(max_sample_count);
    EXPECT_TRUE(subscribe_result.has_value());

    // When subscribing again
    const auto subscribe_result_2 = this->proxy_event_->Subscribe(max_sample_count);
    EXPECT_TRUE(subscribe_result_2.has_value());

    // We don't crash
}

TYPED_TEST(LolaProxyEventCommonFixture, DoubleSubscribeWithDifferentMaxSampleCount)
{
    // Given a valid proxy that is already subscribed
    this->InitialiseProxyAndEvent();
    const auto subscribe_result = this->proxy_event_->Subscribe(kMaxNumSlots - 1U);
    EXPECT_TRUE(subscribe_result.has_value());

    // When subscribing again with a different sample count
    const auto subscribe_result_2 = this->proxy_event_->Subscribe(1U);
    ASSERT_FALSE(subscribe_result_2.has_value());
    EXPECT_EQ(subscribe_result_2.error(), ComErrc::kMaxSampleCountNotRealizable);

    // We don't crash
}

TYPED_TEST(LolaProxyEventCommonFixture, UnsetReceiveHandlerWhileSubscribed)
{
    // Given a valid proxy where we are only subscribed
    this->InitialiseProxyAndEvent();
    this->proxy_event_->Subscribe(1U);

    // When removing an receive handler
    this->proxy_event_->UnsetReceiveHandler();

    // Then we don't crash
}

TYPED_TEST(LolaProxyEventCommonFixture, UnsetReceiveHandlerWithoutBeingSubscribed)
{
    // Given a valid proxy that is not subscribed
    this->InitialiseProxyAndEvent();

    // When removing an receive handler
    this->proxy_event_->UnsetReceiveHandler();

    // Then we don't crash
}

}  // namespace
}  // namespace score::mw::com::impl::lola
