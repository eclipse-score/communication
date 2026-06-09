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
 *******************************************************************************/
#include "score/mw/com/gateway/gateway_application/gateway_application.h"

#include "score/mw/com/gateway/gateway_application/configuration/gateway_configuration.h"
#include "score/mw/com/gateway/gateway_application/gateway_error.h"
#include "score/mw/com/gateway/transport_layer/transport_mock.h"
#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/subscription_state.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::gateway
{
namespace
{

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

GatewayConfiguration MakeConfig(std::vector<std::string> forwarded = {}, std::vector<std::string> received = {})
{
    return GatewayConfiguration{
        std::move(forwarded), std::move(received), "sample_hypervisor", "/etc/gateway/transport.json"};
}

// Creates an InstanceSpecifier from a string literal — asserts on failure.
impl::InstanceSpecifier MakeSpecifier(const std::string& path)
{
    auto result = impl::InstanceSpecifier::Create(std::string{path});
    EXPECT_TRUE(result.has_value()) << "Failed to create InstanceSpecifier for: " << path;
    return std::move(result).value();
}

// Creates a GatewayApplication with an injected NiceMock<TransportMock>.
// Returns both the app and a raw pointer to the mock for EXPECT_CALL setup.
std::pair<std::unique_ptr<GatewayApplication>, TransportMock*> MakeAppWithMockTransport(GatewayConfiguration config)
{
    auto mock_owned = std::make_unique<NiceMock<TransportMock>>();
    TransportMock* mock_ptr = mock_owned.get();
    auto app = std::make_unique<GatewayApplication>(std::move(config), std::move(mock_owned));
    return {std::move(app), mock_ptr};
}

// Start / service discovery tests
TEST(GatewayApplicationStartTest, InvalidForwardedSpecifierIsSkipped)
{
    // Given a forwarded service whose specifier string is invalid ("//" is not allowed)
    auto [app, transport] = MakeAppWithMockTransport(MakeConfig({"in//valid"}, {}));

    // When Start() runs service discovery
    const auto result = app->Start();

    // Then the invalid specifier is skipped (no find-service started) and Start still succeeds.
    EXPECT_TRUE(result.has_value());
}

// ProvideService tests
TEST(GatewayApplicationProvideServiceTest, NonWhitelistedServiceReturnsError)
{
    // Given a GatewayApplication with "svc/a" and "svc/b" in its whitelist
    GatewayApplication app{MakeConfig({"svc/a"}, {"svc/b"})};

    // When ProvideService is called with a specifier that is not on the whitelist
    const auto result = app.ProvideService(MakeSpecifier("svc/not_whitelisted"), {});

    // Then the call must fail with kNonWhitelistedService
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kNonWhitelistedService);
}

// OfferService tests
TEST(GatewayApplicationOfferServiceTest, NoSkeletonReturnsError)
{
    // Given a GatewayApplication with no skeleton registered for the specifier
    GatewayApplication app{MakeConfig({}, {"svc/b"})};

    // When OfferService is called for that specifier
    const auto result = app.OfferService(MakeSpecifier("svc/b"));

    // Then the call must fail with kSkeletonOfferFailed
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kSkeletonOfferFailed);
}

// StopOfferService tests
TEST(GatewayApplicationStopOfferServiceTest, NoSkeletonDoesNotCrash)
{
    // Given a GatewayApplication with no skeleton registered for the specifier
    GatewayApplication app{MakeConfig({}, {"svc/b"})};

    // When StopOfferService is called for that specifier
    // Then it must not crash (returns void)
    EXPECT_NO_FATAL_FAILURE(app.StopOfferService(MakeSpecifier("svc/b")));
}

// RegisterUpdateNotification tests
TEST(GatewayApplicationRegisterUpdateNotificationTest, NoProxyReturnsUnknownServiceInstance)
{
    // Given a GatewayApplication with no proxy registered for the specifier
    GatewayApplication app{MakeConfig({"svc/a"}, {})};

    // When RegisterUpdateNotification is called for that specifier
    const auto result =
        app.RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");

    // Then the call must fail with kUnknownServiceInstance
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceInstance);
}

// UnregisterUpdateNotification tests
TEST(GatewayApplicationUnregisterUpdateNotificationTest, NoProxyReturnsUnknownServiceInstance)
{
    // Given a GatewayApplication with no proxy registered for the specifier
    GatewayApplication app{MakeConfig({"svc/a"}, {})};

    // When UnregisterUpdateNotification is called for that specifier
    const auto result =
        app.UnregisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");

    // Then the call must fail with kUnknownServiceInstance
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceInstance);
}

// NotifyUpdate tests
TEST(GatewayApplicationNotifyUpdateTest, NoSkeletonReturnsUnknownServiceInstance)
{
    // Given a GatewayApplication with no skeleton registered for the specifier
    GatewayApplication app{MakeConfig({}, {"svc/b"})};

    // When NotifyUpdate is called for that specifier
    const auto result = app.NotifyUpdate(MakeSpecifier("svc/b"), impl::ServiceElementType::EVENT, "EventA");

    // Then the call must fail with kUnknownServiceInstance
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceInstance);
}

// Setup / Start tests
TEST(GatewayApplicationSetupTest, SetupDelegatesToTransport)
{
    // Given a GatewayApplication with an injected mock transport that returns success on Setup
    auto [app, mock] = MakeAppWithMockTransport(MakeConfig());
    EXPECT_CALL(*mock, Setup()).WillOnce(Return(score::Result<void>{}));

    // When Setup is called
    const auto result = app->Setup();

    // Then the call must succeed
    EXPECT_TRUE(result.has_value());
}

TEST(GatewayApplicationSetupTest, SetupPropagatesTransportError)
{
    // Given a GatewayApplication with an injected mock transport that returns an error on Setup
    auto [app, mock] = MakeAppWithMockTransport(MakeConfig());
    EXPECT_CALL(*mock, Setup()).WillOnce(Return(MakeUnexpected(GatewayErrorc::kSkeletonCreationFailed)));

    // When Setup is called
    const auto result = app->Setup();

    // Then the transport error must be propagated
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kSkeletonCreationFailed);
}

TEST(GatewayApplicationSetupTest, StartWithNoForwardedServicesSucceeds)
{
    // Given a GatewayApplication with no forwarded services and an injected mock transport
    // (Start calls StartServiceDiscovery which loops over forwarded_services)
    auto [app, mock] = MakeAppWithMockTransport(MakeConfig({}, {"svc/b"}));

    // When Start is called
    const auto result = app->Start();

    // Then the call must succeed (empty forwarded list — nothing to discover)
    EXPECT_TRUE(result.has_value());
}

TEST(GatewayApplicationDestructorTest, ShutdownCalledOnDestruction)
{
    // Given a GatewayApplication with an injected mock transport
    auto mock_owned = std::make_unique<StrictMock<TransportMock>>();
    TransportMock* mock_ptr = mock_owned.get();
    EXPECT_CALL(*mock_ptr, Shutdown()).Times(1);

    // When the GatewayApplication goes out of scope
    {
        GatewayApplication app{MakeConfig(), std::move(mock_owned)};
        (void)app;
    }

    // Then Transport::Shutdown must have been called exactly once
}

TEST(GatewayApplicationDestructorTest, NoTransportDoesNotCrash)
{
    // Given a GatewayApplication with no transport injected and Setup() never called

    // When the GatewayApplication goes out of scope
    // Then the destructor must not crash
    GatewayApplication app{MakeConfig()};
    (void)app;
}

TEST(GatewayApplicationSetupTest, SetupWithoutInjectedTransportCallsTransportFactory)
{
    // Given a GatewayApplication with no transport injected
    // (TransportFactory::Create currently calls std::terminate() — transport layer not yet implemented)
    GatewayApplication app{MakeConfig()};

    // When Setup() is called without a pre-injected transport
    // Then TransportFactory::Create is invoked and the process terminates
    EXPECT_DEATH(app.Setup(), ".*");
}

// ---------------------------------------------------------------------------
// OnSubscriptionStateChanged tests
// ---------------------------------------------------------------------------

}  // namespace

class GatewayApplicationSubscriptionTest : public ::testing::Test
{
  protected:
    GatewayApplicationSubscriptionTest()
    {
        auto mock_owned = std::make_unique<::testing::NiceMock<TransportMock>>();
        mock_ = mock_owned.get();
        app_ = std::make_unique<GatewayApplication>(
            GatewayConfiguration{{"svc/a"}, {"svc/b"}, "sample_hypervisor", "/etc/gateway/transport.json"},
            std::move(mock_owned));
    }

    // Friend-access wrappers: the fixture is a friend of GatewayApplication, but TEST_F bodies derive from
    // it and do not inherit friendship, so they need these to reach the private API.
    void CallOnSubscriptionStateChanged(const std::string& specifier, const std::string& event, bool has_subscribers)
    {
        app_->OnSubscriptionStateChanged(specifier, event, has_subscribers);
    }

    void CallReRegisterActiveEventSubscriptions(const std::string& specifier)
    {
        app_->ReRegisterActiveEventSubscriptions(specifier);
    }

    // Directly seeds the private active-subscription map so a non-empty entry can be stored under a
    // specifier string that InstanceSpecifier::Create would reject (unreachable via the public API).
    void InsertActiveSubscription(const std::string& specifier, const std::string& event)
    {
        app_->active_event_subscriptions_[specifier].insert(event);
    }

    std::unique_ptr<GatewayApplication> app_;
    TransportMock* mock_{nullptr};
};

TEST_F(GatewayApplicationSubscriptionTest, FirstSubscriberCallsRegisterUpdateNotification)
{
    // Given no previous subscriptions for "svc/a" / "EventA"
    auto specifier = impl::InstanceSpecifier::Create("svc/a").value();
    EXPECT_CALL(*mock_, RegisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(Return(score::Result<void>{}));

    // When OnSubscriptionStateChanged is called with has_subscribers = true
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);
}

TEST_F(GatewayApplicationSubscriptionTest, DuplicateSubscriberIsIgnored)
{
    // Given "EventA" is already in the active subscriptions
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).WillOnce(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);

    // When OnSubscriptionStateChanged fires again for the same event
    // Then RegisterUpdateNotification must NOT be called a second time
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).Times(0);
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);
}

TEST_F(GatewayApplicationSubscriptionTest, LastUnsubscriberCallsUnregisterUpdateNotification)
{
    // Given "EventA" is active
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).WillOnce(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);

    // When all consumers unsubscribe
    // Then UnregisterUpdateNotification is called for the event
    auto specifier = impl::InstanceSpecifier::Create("svc/a").value();
    EXPECT_CALL(*mock_, UnregisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", false);
}

TEST_F(GatewayApplicationSubscriptionTest, UnsubscribeWithNoActiveSubscriptionIsIgnored)
{
    // Given no active subscription for "EventA"
    // When OnSubscriptionStateChanged fires with has_subscribers = false
    // Then UnregisterUpdateNotification must NOT be called
    EXPECT_CALL(*mock_, UnregisterUpdateNotification(_, _, _)).Times(0);
    CallOnSubscriptionStateChanged("svc/a", "EventA", false);
}

TEST_F(GatewayApplicationSubscriptionTest, InvalidSpecifierIsIgnored)
{
    // Given an invalid specifier string (ends with '/', so InstanceSpecifier::Create fails)
    // When OnSubscriptionStateChanged fires
    // Then neither RegisterUpdateNotification nor UnregisterUpdateNotification must be called
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).Times(0);
    EXPECT_CALL(*mock_, UnregisterUpdateNotification(_, _, _)).Times(0);
    CallOnSubscriptionStateChanged("svc/", "EventA", true);
}

TEST_F(GatewayApplicationSubscriptionTest, RegisterUpdateNotificationFailureDoesNotCrash)
{
    // Given the transport returns an error for RegisterUpdateNotification
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _))
        .WillOnce(Return(MakeUnexpected(GatewayErrorc::kUnknownServiceInstance)));

    // When OnSubscriptionStateChanged fires with has_subscribers = true
    // Then no crash — the error is only logged
    EXPECT_NO_FATAL_FAILURE(CallOnSubscriptionStateChanged("svc/a", "EventA", true));
}

TEST_F(GatewayApplicationSubscriptionTest, UnregisterUpdateNotificationFailureDoesNotCrash)
{
    // Given "EventA" is active
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).WillOnce(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);

    // Given the transport returns an error for UnregisterUpdateNotification
    EXPECT_CALL(*mock_, UnregisterUpdateNotification(_, _, _))
        .WillOnce(Return(MakeUnexpected(GatewayErrorc::kUnknownServiceInstance)));

    // When OnSubscriptionStateChanged fires with has_subscribers = false
    // Then no crash — the error is only logged
    EXPECT_NO_FATAL_FAILURE(CallOnSubscriptionStateChanged("svc/a", "EventA", false));
}

TEST_F(GatewayApplicationSubscriptionTest, ReRegisterActiveEventSubscriptionsNoSubscriptionsNoTransportCall)
{
    // Given no active subscriptions for "svc/a"
    // When ReRegisterActiveEventSubscriptions is called
    // Then RegisterUpdateNotification must NOT be called
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).Times(0);
    CallReRegisterActiveEventSubscriptions("svc/a");
}

TEST_F(GatewayApplicationSubscriptionTest, ReRegisterActiveEventSubscriptionsWithActiveSubscriptionsCallsTransport)
{
    // Given "EventA" and "EventB" are active subscriptions for "svc/a"
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).WillRepeatedly(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);
    CallOnSubscriptionStateChanged("svc/a", "EventB", true);

    // When ReRegisterActiveEventSubscriptions is called
    // Then RegisterUpdateNotification must be called once for each active event
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, impl::ServiceElementType::EVENT, std::string{"EventB"}))
        .WillOnce(Return(score::Result<void>{}));
    CallReRegisterActiveEventSubscriptions("svc/a");
}

TEST_F(GatewayApplicationSubscriptionTest, ReRegisterActiveEventSubscriptionsEmptyEntryNoTransportCall)
{
    // Given "EventA" was subscribed then unsubscribed, which leaves an empty (but present) entry
    // under "svc/a" in the active-subscriptions map.
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).WillOnce(Return(score::Result<void>{}));
    EXPECT_CALL(*mock_, UnregisterUpdateNotification(_, _, _)).WillOnce(Return(score::Result<void>{}));
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);
    CallOnSubscriptionStateChanged("svc/a", "EventA", false);

    // When ReRegisterActiveEventSubscriptions is called
    // Then the empty-set branch returns early and no RegisterUpdateNotification is issued.
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).Times(0);
    CallReRegisterActiveEventSubscriptions("svc/a");
}

TEST_F(GatewayApplicationSubscriptionTest, ReRegisterActiveEventSubscriptionsInvalidSpecifierKeySkipsEvent)
{
    // Given a non-empty active subscription stored under an invalid specifier key (ends with '/',
    // so InstanceSpecifier::Create fails). This state is only reachable by seeding the map directly.
    InsertActiveSubscription("svc/", "EventA");

    // When ReRegisterActiveEventSubscriptions is called
    // Then InstanceSpecifier::Create fails for the event, it is skipped, and no transport call occurs.
    EXPECT_CALL(*mock_, RegisterUpdateNotification(_, _, _)).Times(0);
    EXPECT_NO_FATAL_FAILURE(CallReRegisterActiveEventSubscriptions("svc/"));
}

// ---------------------------------------------------------------------------
// RegisterEventReceiveHandlerCallback tests
// ---------------------------------------------------------------------------

namespace
{

// Local mock for IBindingRuntime — mirrors the one used in generic_skeleton_event_test.cpp.
class IBindingRuntimeMock : public impl::IBindingRuntime
{
  public:
    MOCK_METHOD(impl::IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (ref(&), noexcept, override));
    MOCK_METHOD(impl::BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(impl::tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

}  // namespace

class GatewayApplicationRegisterCallbackTest : public ::testing::Test
{
  protected:
    GatewayApplicationRegisterCallbackTest()
    {
        impl::GenericSkeletonEventBindingFactory::mock_ = &event_binding_factory_mock_;

        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(impl::BindingType::kLoLa))
            .WillByDefault(::testing::Return(&binding_runtime_mock_));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(::testing::ReturnRef(service_discovery_mock_));
        ON_CALL(binding_runtime_mock_, GetBindingType()).WillByDefault(::testing::Return(impl::BindingType::kLoLa));
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(::testing::ReturnRef(service_discovery_client_mock_));
        ON_CALL(service_discovery_mock_, OfferService(::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
            .WillByDefault(::testing::Invoke([this](const auto&) {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::Skeleton>>();
                skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(::testing::_, ::testing::_, ::testing::_))
                    .WillByDefault(::testing::Return(score::Result<void>{}));
                return mock;
            }));

        auto transport_owned = std::make_unique<::testing::NiceMock<TransportMock>>();
        transport_mock_ = transport_owned.get();
        app_ = std::make_unique<GatewayApplication>(
            GatewayConfiguration{{"svc/a"}, {"svc/b"}, "sample_hypervisor", "/etc/gateway/transport.json"},
            std::move(transport_owned));
    }

    ~GatewayApplicationRegisterCallbackTest() override
    {
        impl::GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    void CallRegisterEventReceiveHandlerCallback(impl::GenericSkeleton& skeleton,
                                                 const std::string& specifier_str,
                                                 const std::string& event_name)
    {
        app_->RegisterEventReceiveHandlerCallback(skeleton, specifier_str, event_name);
    }

    // Creates a GenericSkeleton with one mock event binding for the given event name.
    // Returns the skeleton (by move) and a raw pointer to the mock event binding.
    // IMPORTANT: Set EXPECT_CALL on the returned mock_event BEFORE calling
    // CallRegisterEventReceiveHandlerCallback to capture the stored callback.
    std::pair<impl::GenericSkeleton, impl::mock_binding::GenericSkeletonEvent*> CreateSkeletonWithMockEvent(
        const std::string& event_name)
    {
        auto mock_event = std::make_unique<::testing::NiceMock<impl::mock_binding::GenericSkeletonEvent>>();
        auto* mock_event_ptr = mock_event.get();

        EXPECT_CALL(event_binding_factory_mock_, Create(::testing::_, event_name, ::testing::_))
            .WillOnce(::testing::Return(::testing::ByMove(std::move(mock_event))));

        std::vector<impl::EventInfo> event_storage{{event_name, impl::DataTypeMetaInfo{16, 8}}};
        impl::GenericSkeletonServiceElementInfo info;
        info.events = event_storage;

        auto skeleton_result =
            impl::GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifierWithEvent(
                                              {{event_name, impl::LolaEventInstanceDeployment{1, 1, 1, true, 0}}}),
                                          info);
        EXPECT_TRUE(skeleton_result.has_value());
        return {std::move(skeleton_result).value(), mock_event_ptr};
    }

    std::unique_ptr<GatewayApplication> app_;
    TransportMock* transport_mock_{nullptr};

    ::testing::NiceMock<impl::GenericSkeletonEventBindingFactoryMock> event_binding_factory_mock_;
    impl::RuntimeMockGuard runtime_mock_guard_{};
    ::testing::NiceMock<IBindingRuntimeMock> binding_runtime_mock_{};
    ::testing::NiceMock<impl::ServiceDiscoveryMock> service_discovery_mock_{};
    ::testing::NiceMock<impl::ServiceDiscoveryClientMock> service_discovery_client_mock_{};
    impl::SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    impl::mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    impl::DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GatewayApplicationRegisterCallbackTest, CallbackFiredWithSubscriberCallsRegisterUpdateNotification)
{
    // Given a skeleton with "EventA" whose binding captures the registration callback
    auto [skeleton, mock_event] = CreateSkeletonWithMockEvent("EventA");

    bool callback_captured = false;
    impl::ReceiveHandlerRegistrationChangedCallback captured_callback;
    EXPECT_CALL(*mock_event, SetReceiveHandlerRegistrationChangedHandler(::testing::_))
        .WillOnce(::testing::Invoke([&captured_callback, &callback_captured](
                                        impl::ReceiveHandlerRegistrationChangedCallback cb) -> score::Result<void> {
            captured_callback = std::move(cb);
            callback_captured = true;
            return {};
        }));

    // When RegisterEventReceiveHandlerCallback is called
    CallRegisterEventReceiveHandlerCallback(skeleton, "svc/a", "EventA");
    ASSERT_TRUE(callback_captured);

    // And the captured callback fires with has_subscribers = true
    EXPECT_CALL(*transport_mock_,
                RegisterUpdateNotification(::testing::_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(::testing::Return(score::Result<void>{}));
    captured_callback(true);
}

TEST_F(GatewayApplicationRegisterCallbackTest, CallbackFiredWithoutSubscriberDoesNotCallUnregister)
{
    // Given a skeleton with "EventA" whose binding captures the registration callback
    auto [skeleton, mock_event] = CreateSkeletonWithMockEvent("EventA");

    bool callback_captured = false;
    impl::ReceiveHandlerRegistrationChangedCallback captured_callback;
    EXPECT_CALL(*mock_event, SetReceiveHandlerRegistrationChangedHandler(::testing::_))
        .WillOnce(::testing::Invoke([&captured_callback, &callback_captured](
                                        impl::ReceiveHandlerRegistrationChangedCallback cb) -> score::Result<void> {
            captured_callback = std::move(cb);
            callback_captured = true;
            return {};
        }));

    CallRegisterEventReceiveHandlerCallback(skeleton, "svc/a", "EventA");
    ASSERT_TRUE(callback_captured);

    // When the callback fires with has_subscribers = false and no prior subscription
    // Then UnregisterUpdateNotification must NOT be called (nothing to deregister)
    EXPECT_CALL(*transport_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_)).Times(0);
    captured_callback(false);
}

TEST_F(GatewayApplicationRegisterCallbackTest, UnknownEventDoesNotRegisterCallback)
{
    // Given a skeleton that only knows about "EventA"
    auto [skeleton, mock_event] = CreateSkeletonWithMockEvent("EventA");

    // When RegisterEventReceiveHandlerCallback targets an event the skeleton does not expose
    // Then no registration-changed handler is set on any event binding (event-not-found path).
    EXPECT_CALL(*mock_event, SetReceiveHandlerRegistrationChangedHandler(::testing::_)).Times(0);
    EXPECT_NO_FATAL_FAILURE(CallRegisterEventReceiveHandlerCallback(skeleton, "svc/a", "EventX"));
}

// ---------------------------------------------------------------------------
// End-to-end flow tests (proxy + skeleton paths)
// ---------------------------------------------------------------------------
// This fixture wires up the complete impl-layer mock infrastructure so the
// proxy-discovery and skeleton-offer code paths of GatewayApplication can be
// exercised without a real LoLa runtime. It lives outside the anonymous
// namespace so the friend declaration GatewayApplicationFlowTest resolves.

namespace
{

class FlowBindingRuntimeMock : public impl::IBindingRuntime
{
  public:
    MOCK_METHOD(impl::IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (ref(&), noexcept, override));
    MOCK_METHOD(impl::BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(impl::tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

}  // namespace

class GatewayApplicationFlowTest : public ::testing::Test
{
  protected:
    GatewayApplicationFlowTest()
    {
        impl::GenericSkeletonEventBindingFactory::mock_ = &generic_skeleton_event_binding_factory_mock_;

        // --- Runtime / service discovery wiring -------------------------------------------------
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(::testing::ReturnRef(service_discovery_mock_));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(impl::BindingType::kLoLa))
            .WillByDefault(::testing::Return(&binding_runtime_mock_));
        ON_CALL(binding_runtime_mock_, GetBindingType()).WillByDefault(::testing::Return(impl::BindingType::kLoLa));
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(::testing::ReturnRef(service_discovery_client_mock_));
        ON_CALL(service_discovery_mock_, OfferService(::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));

        // GenericSkeleton::Create(specifier, ...) resolves the specifier to a concrete identifier
        // and then looks up each requested event in that identifier's type deployment. A fresh
        // builder is used per call (so each returned identifier's backing deployment stays alive)
        // and the events the skeleton tests provide are synced into the type deployment.
        ON_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
            .WillByDefault(::testing::Invoke([this](const auto&) {
                auto& builder = handle_builders_.emplace_back();
                impl::LolaServiceInstanceDeployment::EventInstanceMapping events{};
                for (const auto& name : skeleton_event_names_)
                {
                    events.emplace(name, impl::LolaEventInstanceDeployment{1U, 1U, 1U, true, 0U});
                }
                return std::vector<impl::InstanceIdentifier>{
                    builder.CreateValidLolaInstanceIdentifierWithEvent(events)};
            }));

        // --- Proxy binding factory: yields a fresh mock proxy per GenericProxy::Create ----------
        ON_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
            .WillByDefault(::testing::Invoke([this](const auto&) {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::Proxy>>();
                proxy_binding_mock_ = mock.get();
                ON_CALL(*mock, IsEventProvided(::testing::_)).WillByDefault(::testing::Return(true));
                return mock;
            }));

        // --- Generic proxy event binding factory: yields a fresh mock per event -----------------
        // The gateway drives the real ProxyEventBase state machine, so the mock binding has to
        // report a consistent subscription state: not-subscribed until Subscribe() succeeds, then
        // subscribed until Unsubscribe(). Otherwise ProxyEventBase::Subscribe takes the
        // already-subscribed branch and asserts on the (unset) max sample count.
        ON_CALL(generic_proxy_event_binding_factory_mock_guard_.factory_mock_,
                Create(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([this](impl::HandleType,
                                                    impl::ProxyBinding&,
                                                    const std::string_view event_name,
                                                    impl::ServiceElementType) {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::GenericProxyEvent>>();
                auto state = std::make_shared<impl::SubscriptionState>(impl::SubscriptionState::kNotSubscribed);
                ON_CALL(*mock, GetSubscriptionState()).WillByDefault(::testing::Invoke([state]() {
                    return *state;
                }));
                ON_CALL(*mock, Subscribe(::testing::_)).WillByDefault(::testing::Invoke([state](std::size_t) {
                    *state = impl::SubscriptionState::kSubscribed;
                    return score::Result<void>{};
                }));
                ON_CALL(*mock, Unsubscribe()).WillByDefault(::testing::Invoke([state]() noexcept {
                    *state = impl::SubscriptionState::kNotSubscribed;
                }));
                ON_CALL(*mock, SetReceiveHandler(::testing::_)).WillByDefault(::testing::Return(score::Result<void>{}));
                ON_CALL(*mock, UnsetReceiveHandler()).WillByDefault(::testing::Return(score::Result<void>{}));
                proxy_event_mocks_[std::string{event_name}] = mock.get();
                return mock;
            }));

        // --- Skeleton binding factory: yields a fresh mock skeleton per GenericSkeleton::Create --
        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
            .WillByDefault(::testing::Invoke([this](const auto&) {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::Skeleton>>();
                skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(::testing::_, ::testing::_, ::testing::_))
                    .WillByDefault(::testing::Return(score::Result<void>{}));
                ON_CALL(*mock, VerifyAllMethodHandlersRegistered()).WillByDefault(::testing::Return(true));
                return mock;
            }));

        // --- Generic skeleton event binding factory: yields a fresh mock per event --------------
        ON_CALL(generic_skeleton_event_binding_factory_mock_, Create(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [this](impl::SkeletonBase&, std::string_view event_name, const impl::DataTypeMetaInfo&)
                    -> score::Result<std::unique_ptr<impl::GenericSkeletonEventBinding>> {
                    auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::GenericSkeletonEvent>>();
                    ON_CALL(*mock, SetReceiveHandlerRegistrationChangedHandler(::testing::_))
                        .WillByDefault(::testing::Return(score::Result<void>{}));
                    ON_CALL(*mock, PrepareOffer()).WillByDefault(::testing::Return(score::Result<void>{}));
                    ON_CALL(*mock, Notify()).WillByDefault(::testing::Return(score::Result<void>{}));
                    skeleton_event_mocks_[std::string{event_name}] = mock.get();
                    return mock;
                }));

        // --- Transport ---------------------------------------------------------------------------
        auto transport_owned = std::make_unique<::testing::NiceMock<TransportMock>>();
        transport_mock_ = transport_owned.get();
        ON_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));
        ON_CALL(*transport_mock_, StopOfferService(::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));
        ON_CALL(*transport_mock_, RegisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));
        ON_CALL(*transport_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(score::Result<void>{}));

        app_ = std::make_unique<GatewayApplication>(
            GatewayConfiguration{{"svc/a"}, {"svc/a"}, "sample_hypervisor", "/etc/gateway/transport.json"},
            std::move(transport_owned));
    }

    ~GatewayApplicationFlowTest() override
    {
        // Destroy the application (and any skeletons it owns) while the runtime/service-discovery
        // mocks are still alive. GenericSkeleton::StopOfferService reaches into
        // Runtime::getInstance().GetServiceDiscovery() during teardown; if the RuntimeMockGuard were
        // destroyed first (members destruct in reverse declaration order), the real Runtime would be
        // lazily initialised and abort trying to parse a non-existent config file.
        app_.reset();
        impl::GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    // Builds a HandleType carrying the given proxy events. A fresh builder is stored per handle so
    // the deployment storage backing the InstanceIdentifier stays valid for the test's lifetime.
    impl::HandleType MakeProxyHandle(const std::vector<std::string>& event_names)
    {
        auto& builder = handle_builders_.emplace_back();
        impl::LolaServiceInstanceDeployment::EventInstanceMapping events{};
        for (const auto& name : event_names)
        {
            events.emplace(name, impl::LolaEventInstanceDeployment{1U, 1U, 1U, true, 0U});
        }
        const auto identifier = builder.CreateValidLolaInstanceIdentifierWithEvent(events);
        return impl::make_HandleType(identifier);
    }

    // Calls Start() while capturing the find-service handler the gateway registers for "svc/a".
    void StartAndCaptureFindHandler()
    {
        EXPECT_CALL(service_discovery_mock_, StartFindService(::testing::_, ::testing::An<impl::InstanceSpecifier>()))
            .WillOnce(
                ::testing::Invoke([this](impl::FindServiceHandler<impl::HandleType> handler, impl::InstanceSpecifier) {
                    captured_find_handler_ = std::move(handler);
                    find_handler_captured_ = true;
                    return score::Result<impl::FindServiceHandle>{impl::make_FindServiceHandle(1U)};
                }));
        ASSERT_TRUE(app_->Start().has_value());
    }

    // Drives the captured find-service handler, simulating a discovery callback.
    void FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType> handles)
    {
        ASSERT_TRUE(find_handler_captured_);
        captured_find_handler_(std::move(handles), impl::make_FindServiceHandle(1U));
    }

    // Friend-access wrapper: TEST_F bodies derive from the fixture but do not inherit friendship.
    void CallOnSubscriptionStateChanged(const std::string& specifier, const std::string& event, bool subscribed)
    {
        app_->OnSubscriptionStateChanged(specifier, event, subscribed);
    }
    void CallPropagateService(const std::string& specifier)
    {
        app_->PropagateService(specifier);
    }

    // Moves the proxy stored under `from` to the key `to`, keeping the same GenericProxy instance.
    // Used to place a valid proxy under a specifier string that InstanceSpecifier::Create rejects.
    void RekeyProxy(const std::string& from, const std::string& to)
    {
        auto node = app_->proxies_.extract(from);
        node.key() = to;
        app_->proxies_.insert(std::move(node));
    }
    std::unique_ptr<GatewayApplication> app_;
    TransportMock* transport_mock_{nullptr};

    impl::RuntimeMockGuard runtime_mock_guard_{};
    ::testing::NiceMock<impl::ServiceDiscoveryMock> service_discovery_mock_{};
    ::testing::NiceMock<impl::ServiceDiscoveryClientMock> service_discovery_client_mock_{};
    ::testing::NiceMock<FlowBindingRuntimeMock> binding_runtime_mock_{};

    impl::ProxyBindingFactoryMockGuard proxy_binding_factory_mock_guard_{};
    impl::GenericProxyEventBindingFactoryMockGuard generic_proxy_event_binding_factory_mock_guard_{};
    impl::SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    ::testing::NiceMock<impl::GenericSkeletonEventBindingFactoryMock> generic_skeleton_event_binding_factory_mock_{};

    impl::mock_binding::Proxy* proxy_binding_mock_{nullptr};
    impl::mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    std::map<std::string, impl::mock_binding::GenericProxyEvent*> proxy_event_mocks_{};
    std::map<std::string, impl::mock_binding::GenericSkeletonEvent*> skeleton_event_mocks_{};

    impl::FindServiceHandler<impl::HandleType> captured_find_handler_{};
    bool find_handler_captured_{false};

    // Owns the deployment storage backing every InstanceIdentifier created during the test.
    std::deque<impl::DummyInstanceIdentifierBuilder> handle_builders_{};

    // Event names synced into the type deployment returned by the runtime resolve() mock, so that
    // GenericSkeleton::Create can resolve the stable event names the skeleton tests request.
    std::vector<std::string> skeleton_event_names_{"EventA"};
};

// --- StartServiceDiscovery -----------------------------------------------------------------------

TEST_F(GatewayApplicationFlowTest, StartRegistersFindServiceForForwardedService)
{
    // Given a gateway configured to forward "svc/a"
    // When Start() is called
    // Then StartFindService is invoked once for that specifier and a find handle is stored.
    EXPECT_CALL(service_discovery_mock_, StartFindService(::testing::_, ::testing::An<impl::InstanceSpecifier>()))
        .WillOnce(::testing::Return(
            ::testing::ByMove(score::Result<impl::FindServiceHandle>{impl::make_FindServiceHandle(1U)})));

    EXPECT_TRUE(app_->Start().has_value());
}

TEST_F(GatewayApplicationFlowTest, StartHandlesFindServiceFailure)
{
    // Given StartFindService fails for the forwarded service
    EXPECT_CALL(service_discovery_mock_, StartFindService(::testing::_, ::testing::An<impl::InstanceSpecifier>()))
        .WillOnce(::testing::Return(::testing::ByMove(
            score::Result<impl::FindServiceHandle>{score::MakeUnexpected(GatewayErrorc::kUnknownServiceInstance)})));

    // When Start() is called, then it still completes successfully (failure is logged and skipped).
    EXPECT_TRUE(app_->Start().has_value());
}

// --- OnServiceAvailabilityChanged / PropagateService ---------------------------------------------

TEST_F(GatewayApplicationFlowTest, ServiceFoundCreatesProxyAndPropagates)
{
    // Given discovery is started and a service with one event becomes available
    StartAndCaptureFindHandler();

    // Then the gateway creates a proxy and forwards ProvideService to the transport.
    EXPECT_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::Result<void>{}));

    impl::ServiceHandleContainer<impl::HandleType> handles{MakeProxyHandle({"EventA"})};
    FireServiceDiscovery(std::move(handles));
}

TEST_F(GatewayApplicationFlowTest, ServiceFoundProxyCreationFailureIsHandled)
{
    // Given the proxy binding factory cannot create a binding (returns null)
    ON_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
        .WillByDefault(::testing::Invoke([](const auto&) {
            return std::unique_ptr<::testing::NiceMock<impl::mock_binding::Proxy>>{};
        }));

    StartAndCaptureFindHandler();

    // When a service is discovered
    // Then GenericProxy::Create fails, nothing is propagated, and no crash occurs.
    EXPECT_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_)).Times(0);
    EXPECT_NO_FATAL_FAILURE(
        FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})}));
}

TEST_F(GatewayApplicationFlowTest, ServiceDisappearedForwardsStopOffer)
{
    // Given a service was first found
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});

    // When the same service later disappears (empty handle container)
    // Then StopOfferService is forwarded to the transport.
    EXPECT_CALL(*transport_mock_, StopOfferService(::testing::_)).WillOnce(::testing::Return(score::Result<void>{}));
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{});
}

TEST_F(GatewayApplicationFlowTest, ServiceReFoundRePropagates)
{
    // Given a service was found then disappeared (proxy kept alive)
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{});

    // When the service is found again
    // Then it is re-propagated (ProvideService called again) without creating a new proxy.
    EXPECT_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::Result<void>{}));
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
}

TEST_F(GatewayApplicationFlowTest, PropagateServiceFailureErasesProxy)
{
    // Given discovery started and the transport rejects ProvideService
    StartAndCaptureFindHandler();
    EXPECT_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::MakeUnexpected(GatewayErrorc::kSkeletonCreationFailed)))
        .WillOnce(::testing::Return(score::Result<void>{}));

    // When the service is found (ProvideService fails so proxy is erased) and then found again
    // Then a brand new proxy is created and ProvideService retried.
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
}

TEST_F(GatewayApplicationFlowTest, PropagateServiceInvalidSpecifierKeyReturnsEarly)
{
    // Given a service was found, so a valid proxy lives under "svc/a"
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});

    // Re-key that proxy under an invalid specifier string (ends with '/', so
    // InstanceSpecifier::Create fails). This state is only reachable by mutating the map directly.
    RekeyProxy("svc/a", "svc/");

    // When PropagateService runs for the invalid key
    // Then InstanceSpecifier::Create fails, it logs and returns before touching the transport.
    EXPECT_CALL(*transport_mock_, ProvideService(::testing::_, ::testing::_)).Times(0);
    EXPECT_NO_FATAL_FAILURE(CallPropagateService("svc/"));
}

// --- RegisterUpdateNotification / UnregisterUpdateNotification ------------------------------------

TEST_F(GatewayApplicationFlowTest, RegisterUpdateNotificationSubscribesAndSetsHandler)
{
    // Given a discovered service whose proxy carries "EventA"
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    ASSERT_NE(proxy_event_mocks_.find("EventA"), proxy_event_mocks_.cend());

    // When RegisterUpdateNotification is requested for that event
    // Then the proxy event is subscribed and a receive handler is set.
    EXPECT_CALL(*proxy_event_mocks_["EventA"], Subscribe(::testing::_))
        .WillOnce(::testing::Return(score::Result<void>{}));
    EXPECT_CALL(*proxy_event_mocks_["EventA"], SetReceiveHandler(::testing::_))
        .WillOnce(::testing::Return(score::Result<void>{}));

    const auto result =
        app_->RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, RegisterUpdateNotificationUnknownEventReturnsError)
{
    // Given a discovered service whose proxy carries only "EventA"
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});

    // When RegisterUpdateNotification is requested for an unknown event
    // Then it fails with kUnknownServiceElement.
    const auto result =
        app_->RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventX");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceElement);
}

TEST_F(GatewayApplicationFlowTest, RegisterUpdateNotificationHandlerFailureReturnsError)
{
    // Given a discovered service and a proxy event that rejects SetReceiveHandler
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    ASSERT_NE(proxy_event_mocks_.find("EventA"), proxy_event_mocks_.cend());

    EXPECT_CALL(*proxy_event_mocks_["EventA"], SetReceiveHandler(::testing::_))
        .WillOnce(::testing::Return(score::MakeUnexpected(GatewayErrorc::kReceiveHandlerRegistrationFailed)));

    // When RegisterUpdateNotification is requested
    // Then it fails with kReceiveHandlerRegistrationFailed.
    const auto result =
        app_->RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kReceiveHandlerRegistrationFailed);
}

TEST_F(GatewayApplicationFlowTest, ReceiveHandlerFiringForwardsNotifyUpdateOverTransport)
{
    // Given a discovered service whose proxy carries "EventA"
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    ASSERT_NE(proxy_event_mocks_.find("EventA"), proxy_event_mocks_.cend());

    // and the binding fires the stored receive handler immediately when it is set
    ON_CALL(*proxy_event_mocks_["EventA"], SetReceiveHandler(::testing::_))
        .WillByDefault(::testing::Invoke([](std::weak_ptr<impl::ScopedEventReceiveHandler> handler) {
            if (auto locked = handler.lock())
            {
                (*locked)();
            }
            return score::Result<void>{};
        }));

    // When RegisterUpdateNotification is requested (which sets and thus fires the handler)
    // Then the gateway forwards a NotifyUpdate for that event over the transport.
    EXPECT_CALL(*transport_mock_, NotifyUpdate(::testing::_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(::testing::Return(score::Result<void>{}));

    const auto result =
        app_->RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, UnregisterUpdateNotificationUnsetsHandlerAndUnsubscribes)
{
    // Given a discovered service whose proxy carries "EventA" and an active registration
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});
    ASSERT_NE(proxy_event_mocks_.find("EventA"), proxy_event_mocks_.cend());
    ASSERT_TRUE(app_->RegisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA")
                    .has_value());

    // When UnregisterUpdateNotification is requested
    // Then the proxy event handler is unset and the event is unsubscribed.
    EXPECT_CALL(*proxy_event_mocks_["EventA"], UnsetReceiveHandler())
        .WillOnce(::testing::Return(score::Result<void>{}));
    EXPECT_CALL(*proxy_event_mocks_["EventA"], Unsubscribe());

    const auto result =
        app_->UnregisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, UnregisterUpdateNotificationUnknownEventReturnsError)
{
    // Given a discovered service whose proxy carries only "EventA"
    StartAndCaptureFindHandler();
    FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{MakeProxyHandle({"EventA"})});

    // When UnregisterUpdateNotification targets an unknown event
    // Then it fails with kUnknownServiceElement.
    const auto result =
        app_->UnregisterUpdateNotification(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventX");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceElement);
}

// --- ProvideService / OfferService / StopOfferService / NotifyUpdate ------------------------------

std::vector<impl::EventInfo> MakeElements(const std::vector<std::string>& names)
{
    std::vector<impl::EventInfo> elements;
    for (const auto& name : names)
    {
        elements.push_back(impl::EventInfo{name, impl::DataTypeMetaInfo{16U, 8U}});
    }
    return elements;
}

TEST_F(GatewayApplicationFlowTest, ProvideServiceCreatesSkeletonRegistersCallbacksAndOffers)
{
    // Given a whitelisted ("svc/a") provide request with one event whose binding expects exactly one
    // subscription-callback registration.
    ON_CALL(generic_skeleton_event_binding_factory_mock_, Create(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Invoke([this](impl::SkeletonBase&, std::string_view event_name, const impl::DataTypeMetaInfo&)
                                  -> score::Result<std::unique_ptr<impl::GenericSkeletonEventBinding>> {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::GenericSkeletonEvent>>();
                EXPECT_CALL(*mock, SetReceiveHandlerRegistrationChangedHandler(::testing::_))
                    .WillOnce(::testing::Return(score::Result<void>{}));
                ON_CALL(*mock, PrepareOffer()).WillByDefault(::testing::Return(score::Result<void>{}));
                ON_CALL(*mock, Notify()).WillByDefault(::testing::Return(score::Result<void>{}));
                skeleton_event_mocks_[std::string{event_name}] = mock.get();
                return mock;
            }));

    // and the service must be offered exactly once via service discovery.
    EXPECT_CALL(service_discovery_mock_, OfferService(::testing::_)).WillOnce(::testing::Return(score::Result<void>{}));

    // When ProvideService is called
    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));

    // Then a skeleton is created, the subscription callback is registered (verified by the EXPECT_CALL
    // above), and the service is offered (verified by the OfferService expectation above).
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(skeleton_binding_mock_, nullptr);
    EXPECT_NE(skeleton_event_mocks_.find("EventA"), skeleton_event_mocks_.cend());
}

TEST_F(GatewayApplicationFlowTest, ProvideServiceReuseExistingSkeletonReRegistersSubscriptions)
{
    // Given a service was already provided and has an active subscription
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);

    // When ProvideService is called again for the same specifier (skeleton reuse path)
    // Then the active subscription is re-registered via the transport.
    EXPECT_CALL(*transport_mock_,
                RegisterUpdateNotification(::testing::_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(::testing::Return(score::Result<void>{}));

    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, OfferServiceWithExistingSkeletonSucceeds)
{
    // Given a service has been provided (skeleton exists)
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());

    // When OfferService is called for it
    // Then the call succeeds.
    const auto result = app_->OfferService(MakeSpecifier("svc/a"));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, StopOfferServiceWithExistingSkeletonStopsOffer)
{
    // Given a service has been provided (skeleton exists)
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());

    // When StopOfferService is called
    // Then it does not crash and keeps the skeleton for reuse.
    EXPECT_NO_FATAL_FAILURE(app_->StopOfferService(MakeSpecifier("svc/a")));
}

TEST_F(GatewayApplicationFlowTest, NotifyUpdateWithExistingSkeletonNotifiesEvent)
{
    // Given a service has been provided with "EventA"
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());
    ASSERT_NE(skeleton_event_mocks_.find("EventA"), skeleton_event_mocks_.cend());

    // When NotifyUpdate is called for that event
    // Then the skeleton event is notified.
    EXPECT_CALL(*skeleton_event_mocks_["EventA"], Notify()).WillOnce(::testing::Return(score::Result<void>{}));

    const auto result = app_->NotifyUpdate(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventA");
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, NotifyUpdateUnknownEventReturnsError)
{
    // Given a service provided with only "EventA"
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());

    // When NotifyUpdate targets an unknown event
    // Then it fails with kUnknownServiceElement.
    const auto result = app_->NotifyUpdate(MakeSpecifier("svc/a"), impl::ServiceElementType::EVENT, "EventX");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kUnknownServiceElement);
}

TEST_F(GatewayApplicationFlowTest, ProvideServiceOfferFailureErasesSkeleton)
{
    // Given the skeleton binding will reject PrepareOffer (OfferService fails)
    ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
        .WillByDefault(::testing::Invoke([this](const auto&) {
            auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::Skeleton>>();
            skeleton_binding_mock_ = mock.get();
            ON_CALL(*mock, PrepareOffer(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(::testing::Return(score::MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed)));
            return mock;
        }));

    // When ProvideService is called
    // Then it fails with kSkeletonCreationFailed and the skeleton is erased.
    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kSkeletonCreationFailed);
}

// --- RegisterEventReceiveHandlerCallback error paths ---------------------------------------------

TEST_F(GatewayApplicationFlowTest, ProvideServiceTearsDownSkeletonsOnDestruction)
{
    // Given a service has been provided (skeleton exists at destruction)
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());

    // When the application is destroyed
    // Then StopOfferService runs on the held skeleton without crashing (destructor loop coverage).
    EXPECT_NO_FATAL_FAILURE(app_.reset());
}

TEST_F(GatewayApplicationFlowTest, ProvideServiceSkeletonCreationFailureReturnsError)
{
    // Given the skeleton binding factory cannot create a binding (returns null)
    ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(::testing::_))
        .WillByDefault(::testing::Invoke([](const auto&) {
            return std::unique_ptr<::testing::NiceMock<impl::mock_binding::Skeleton>>{};
        }));

    // When ProvideService is called
    // Then GenericSkeleton::Create fails and ProvideService reports kSkeletonCreationFailed.
    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kSkeletonCreationFailed);
}

TEST_F(GatewayApplicationFlowTest, ProvideServiceSetReceiveHandlerRegistrationFailureStillSucceeds)
{
    // Given the skeleton event rejects the receive-handler-registration-changed handler
    ON_CALL(generic_skeleton_event_binding_factory_mock_, Create(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Invoke([this](impl::SkeletonBase&, std::string_view event_name, const impl::DataTypeMetaInfo&)
                                  -> score::Result<std::unique_ptr<impl::GenericSkeletonEventBinding>> {
                auto mock = std::make_unique<::testing::NiceMock<impl::mock_binding::GenericSkeletonEvent>>();
                ON_CALL(*mock, SetReceiveHandlerRegistrationChangedHandler(::testing::_))
                    .WillByDefault(
                        ::testing::Return(score::MakeUnexpected(GatewayErrorc::kReceiveHandlerRegistrationFailed)));
                ON_CALL(*mock, PrepareOffer()).WillByDefault(::testing::Return(score::Result<void>{}));
                skeleton_event_mocks_[std::string{event_name}] = mock.get();
                return mock;
            }));

    // When ProvideService is called
    // Then the registration failure is only logged and ProvideService still succeeds.
    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, OfferServiceFailureReturnsError)
{
    // Given a service has been provided (skeleton exists and offered once)
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());
    ASSERT_NE(skeleton_binding_mock_, nullptr);

    // and the binding will now reject any further offer
    ON_CALL(*skeleton_binding_mock_, PrepareOffer(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(score::MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed)));

    // When OfferService is called explicitly
    // Then it fails with kSkeletonOfferFailed.
    const auto result = app_->OfferService(MakeSpecifier("svc/a"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kSkeletonOfferFailed);
}

TEST_F(GatewayApplicationFlowTest, ReusedSkeletonOfferFailureIsToleratedAndResubscribes)
{
    // Given a service was provided with an active subscription
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);
    ASSERT_NE(skeleton_binding_mock_, nullptr);

    // and re-offering the reused skeleton fails (only a warning is expected)
    ON_CALL(*skeleton_binding_mock_, PrepareOffer(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(score::MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed)));

    // When ProvideService is called again (skeleton reuse path)
    // Then it still succeeds and re-registers the active subscription.
    EXPECT_CALL(*transport_mock_,
                RegisterUpdateNotification(::testing::_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(::testing::Return(score::Result<void>{}));

    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, ReusedSkeletonReRegisterFailureIsTolerated)
{
    // Given a service was provided with an active subscription
    ASSERT_TRUE(app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"})).has_value());
    CallOnSubscriptionStateChanged("svc/a", "EventA", true);

    // and re-registering the active subscription via the transport fails
    EXPECT_CALL(*transport_mock_,
                RegisterUpdateNotification(::testing::_, impl::ServiceElementType::EVENT, std::string{"EventA"}))
        .WillOnce(::testing::Return(score::MakeUnexpected(GatewayErrorc::kReceiveHandlerRegistrationFailed)));

    // When ProvideService is called again (skeleton reuse path)
    // Then the re-registration failure is only logged and ProvideService still succeeds.
    const auto result = app_->ProvideService(MakeSpecifier("svc/a"), MakeElements({"EventA"}));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GatewayApplicationFlowTest, ServiceDisappearedToleratesStopOfferForwardFailure)
{
    // Given a forwarded service was found (a proxy exists)
    StartAndCaptureFindHandler();
    impl::ServiceHandleContainer<impl::HandleType> handles{};
    handles.push_back(MakeProxyHandle({"EventA"}));
    FireServiceDiscovery(std::move(handles));

    // and forwarding StopOfferService to the transport fails
    ON_CALL(*transport_mock_, StopOfferService(::testing::_))
        .WillByDefault(::testing::Return(score::MakeUnexpected(GatewayErrorc::kSkeletonOfferFailed)));

    // When the service disappears (empty discovery)
    // Then the failure is only logged and no crash occurs.
    EXPECT_NO_FATAL_FAILURE(FireServiceDiscovery(impl::ServiceHandleContainer<impl::HandleType>{}));
}

}  // namespace score::mw::com::gateway
