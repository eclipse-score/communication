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
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::gateway
{
namespace
{

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

TEST(GatewayApplicationProvideServiceTest, WhitelistedServiceIsAccepted)
{
    // GenericSkeleton::Create fatally terminates when mw_com_config.json is absent.
    // This test is only executable with a full LoLa runtime providing a valid config.
    GTEST_SKIP() << "Requires mw_com_config.json with 'svc/b' configured — not available in unit test environment.";

    // Given a GatewayApplication with "svc/b" in its received whitelist
    GatewayApplication app{MakeConfig({"svc/a"}, {"svc/b"})};

    // When ProvideService is called with a specifier that is on the whitelist
    // (Since there is no runtime, GenericSkeleton::Create will fail — but the
    // non-whitelisted guard must have been passed.)
    const auto result = app.ProvideService(MakeSpecifier("svc/b"), {});

    // Then the error must not be kNonWhitelistedService (the guard was passed)
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error(), GatewayErrorc::kNonWhitelistedService);
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

}  // namespace
}  // namespace score::mw::com::gateway
