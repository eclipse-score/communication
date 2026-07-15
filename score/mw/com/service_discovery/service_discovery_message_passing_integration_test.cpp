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
#include "score/mw/com/service_discovery/service_discovery_daemon.h"
#include "score/mw/com/service_discovery/service_discovery_message_passing_client.h"
#include "score/mw/com/service_discovery/service_discovery_message_passing_server.h"
#include "score/mw/com/service_discovery/service_discovery_registry.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <unistd.h>

namespace score::mw::com::service_discovery
{

TEST(ServiceDiscoveryMessagePassingIntegrationTest, RegisterAndResolveViaMessagePassing)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_integration";
    server_config.max_send_size = 4096U;
    server_config.max_reply_size = 4096U;

    ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;
    client_config.max_send_size = 4096U;
    client_config.max_reply_size = 4096U;

    ServiceDiscoveryMessagePassingClient client{client_config};
    ASSERT_TRUE(client.Start(std::chrono::milliseconds{1500}));

    ProtocolRequest register_request{};
    register_request.operation = ProtocolOperation::kRegister;
    register_request.registration.key.service_id = 10U;
    register_request.registration.key.instance_id = 1U;
    register_request.registration.offered_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.provider_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.provider_uid = 100U;
    register_request.registration.provider_pid = 200U;
    register_request.registration.endpoint.address = "/tmp/service_discovery_provider";

    const auto register_response = client.Request(register_request);
    ASSERT_TRUE(register_response.has_value());
    EXPECT_EQ(register_response->status_code, 0U);

    ProtocolRequest resolve_request{};
    resolve_request.operation = ProtocolOperation::kResolve;
    resolve_request.registration.key.service_id = 10U;
    resolve_request.registration.key.instance_id = 1U;

    const auto resolve_response = client.Request(resolve_request);
    ASSERT_TRUE(resolve_response.has_value());
    EXPECT_EQ(resolve_response->status_code, 0U);
    ASSERT_EQ(resolve_response->registrations.size, 1U);
    EXPECT_EQ(resolve_response->registrations.values[0].provider_pid, static_cast<std::uint32_t>(::getpid()));
    EXPECT_EQ(resolve_response->registrations.values[0].provider_uid, static_cast<std::uint32_t>(::getuid()));

    client.Stop();

    // Allow the server thread to process disconnect cleanup before the next lookup.
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    ServiceDiscoveryMessagePassingClient observer{client_config};
    ASSERT_TRUE(observer.Start(std::chrono::milliseconds{1500}));

    const auto after_disconnect_response = observer.Request(resolve_request);
    ASSERT_TRUE(after_disconnect_response.has_value());
    EXPECT_EQ(after_disconnect_response->status_code, 0U);
    EXPECT_TRUE(after_disconnect_response->registrations.Empty());

    observer.Stop();
    server.Stop();
}

TEST(ServiceDiscoveryMessagePassingIntegrationTest, SubscribeReceivesAppearanceAndDisconnectDisappearanceUpdates)
{
    struct NotificationState
    {
        std::mutex mutex{};
        std::condition_variable cv{};
        std::size_t count{0U};
        ProtocolNotification last_notification{};
    };

    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_subscription";

    ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;

    ServiceDiscoveryMessagePassingClient watcher{client_config};
    ASSERT_TRUE(watcher.Start(std::chrono::milliseconds{1500}));

    ServiceKey key{};
    key.service_id = 21U;
    key.instance_id = 2U;

    NotificationState notification_state{};

    const auto initial_response =
        watcher.StartFindService(key, [&notification_state](const ProtocolNotification& notification) {
            {
                std::scoped_lock lock{notification_state.mutex};
                notification_state.last_notification = notification;
                ++notification_state.count;
            }
            notification_state.cv.notify_all();
        });
    ASSERT_TRUE(initial_response.has_value());
    EXPECT_NE(initial_response->search_handle, 0U);
    EXPECT_TRUE(initial_response->registrations.Empty());

    ServiceDiscoveryMessagePassingClient provider{client_config};
    ASSERT_TRUE(provider.Start(std::chrono::milliseconds{1500}));

    ProtocolRequest register_request{};
    register_request.operation = ProtocolOperation::kRegister;
    register_request.registration.key = key;
    register_request.registration.offered_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.provider_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.endpoint.address = "/tmp/subscribed_provider";

    const auto register_response = provider.Request(register_request);
    ASSERT_TRUE(register_response.has_value());
    EXPECT_EQ(register_response->status_code, 0U);

    {
        std::unique_lock lock{notification_state.mutex};
        ASSERT_TRUE(
            notification_state.cv.wait_for(lock, std::chrono::milliseconds{1500}, [&notification_state]() noexcept {
                return notification_state.count >= 1U;
            }));
        EXPECT_EQ(notification_state.last_notification.key.service_id, key.service_id);
        EXPECT_EQ(notification_state.last_notification.response.registrations.size, 1U);
    }

    provider.Stop();

    {
        std::unique_lock lock{notification_state.mutex};
        ASSERT_TRUE(
            notification_state.cv.wait_for(lock, std::chrono::milliseconds{1500}, [&notification_state]() noexcept {
                return notification_state.count >= 2U;
            }));
        EXPECT_TRUE(notification_state.last_notification.response.registrations.Empty());
    }

    EXPECT_TRUE(watcher.StopFindService(initial_response->search_handle));
    watcher.Stop();
    server.Stop();
}

TEST(ServiceDiscoveryMessagePassingIntegrationTest, StopFindServiceCanBeCalledFromNotificationCallback)
{
    struct CallbackState
    {
        std::mutex mutex{};
        std::condition_variable cv{};
        std::size_t callback_count{0U};
        bool stop_requested{false};
        bool stop_result{false};
    };

    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_callback_stop";

    ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;

    ServiceDiscoveryMessagePassingClient watcher{client_config};
    ServiceDiscoveryMessagePassingClient provider{client_config};
    ASSERT_TRUE(watcher.Start(std::chrono::milliseconds{1500}));
    ASSERT_TRUE(provider.Start(std::chrono::milliseconds{1500}));

    ServiceKey key{};
    key.service_id = 42U;
    key.instance_id = 7U;

    CallbackState callback_state{};
    const auto initial_response =
        watcher.StartFindService(key, [&watcher, &callback_state](const ProtocolNotification& notification) {
            bool should_stop{false};
            {
                std::scoped_lock lock{callback_state.mutex};
                ++callback_state.callback_count;
                if (!callback_state.stop_requested)
                {
                    callback_state.stop_requested = true;
                    should_stop = true;
                }
            }

            if (should_stop)
            {
                const auto stop_result = watcher.StopFindService(notification.search_handle);
                std::scoped_lock lock{callback_state.mutex};
                callback_state.stop_result = stop_result;
            }

            callback_state.cv.notify_all();
        });
    ASSERT_TRUE(initial_response.has_value());
    EXPECT_NE(initial_response->search_handle, 0U);

    ProtocolRequest register_request{};
    register_request.operation = ProtocolOperation::kRegister;
    register_request.registration.key = key;
    register_request.registration.offered_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.provider_integrity = IntegrityLevel::kAsilQm;
    register_request.registration.endpoint.address = "/tmp/callback_stop_provider";

    const auto register_response = provider.Request(register_request);
    ASSERT_TRUE(register_response.has_value());
    EXPECT_EQ(register_response->status_code, 0U);

    {
        std::unique_lock lock{callback_state.mutex};
        ASSERT_TRUE(callback_state.cv.wait_for(lock, std::chrono::milliseconds{1500}, [&callback_state] {
            return callback_state.stop_requested;
        }));
        EXPECT_EQ(callback_state.callback_count, 1U);
        EXPECT_TRUE(callback_state.stop_result);
    }

    provider.Stop();
    watcher.Stop();
    server.Stop();
}

TEST(ServiceDiscoveryMessagePassingIntegrationTest, ResolveAnyInstanceReturnsAllRegisteredInstances)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_any_instance";

    ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;

    ServiceDiscoveryMessagePassingClient provider_a{client_config};
    ServiceDiscoveryMessagePassingClient provider_b{client_config};
    ServiceDiscoveryMessagePassingClient observer{client_config};

    ASSERT_TRUE(provider_a.Start(std::chrono::milliseconds{1500}));
    ASSERT_TRUE(provider_b.Start(std::chrono::milliseconds{1500}));
    ASSERT_TRUE(observer.Start(std::chrono::milliseconds{1500}));

    ProtocolRequest register_request_a{};
    register_request_a.operation = ProtocolOperation::kRegister;
    register_request_a.registration.key.service_id = 33U;
    register_request_a.registration.key.instance_id = 1U;
    register_request_a.registration.offered_integrity = IntegrityLevel::kAsilQm;
    register_request_a.registration.provider_integrity = IntegrityLevel::kAsilQm;
    register_request_a.registration.endpoint.address = "/tmp/provider_a";
    ASSERT_TRUE(provider_a.Request(register_request_a).has_value());

    ProtocolRequest register_request_b{};
    register_request_b.operation = ProtocolOperation::kRegister;
    register_request_b.registration.key.service_id = 33U;
    register_request_b.registration.key.instance_id = 2U;
    register_request_b.registration.offered_integrity = IntegrityLevel::kAsilQm;
    register_request_b.registration.provider_integrity = IntegrityLevel::kAsilQm;
    register_request_b.registration.endpoint.address = "/tmp/provider_b";
    ASSERT_TRUE(provider_b.Request(register_request_b).has_value());

    ProtocolRequest resolve_any_request{};
    resolve_any_request.operation = ProtocolOperation::kResolve;
    resolve_any_request.registration.key.service_id = 33U;
    resolve_any_request.registration.key.instance_id = kAnyInstanceId;

    const auto resolve_response = observer.Request(resolve_any_request);
    ASSERT_TRUE(resolve_response.has_value());
    EXPECT_EQ(resolve_response->status_code, 0U);
    EXPECT_EQ(resolve_response->registrations.size, 2U);

    observer.Stop();
    provider_b.Stop();
    provider_a.Stop();
    server.Stop();
}

TEST(ServiceDiscoveryMessagePassingIntegrationTest, LockOperationsEnforceOwnershipAndDisconnectCleanup)
{
    ServiceDiscoveryRegistry registry{};
    ServiceDiscoveryDaemon daemon{registry};

    ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_locks";

    ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;

    ServiceDiscoveryMessagePassingClient owner{client_config};
    ServiceDiscoveryMessagePassingClient contender{client_config};
    ASSERT_TRUE(owner.Start(std::chrono::milliseconds{1500}));
    ASSERT_TRUE(contender.Start(std::chrono::milliseconds{1500}));

    ServiceKey key{};
    key.service_id = 88U;
    key.instance_id = 5U;

    EXPECT_TRUE(owner.AcquireCreationLock(key));
    EXPECT_FALSE(contender.AcquireCreationLock(key));
    EXPECT_TRUE(owner.AcquireUsageExclusiveLock(key));
    EXPECT_FALSE(contender.AcquireUsageSharedLock(key));

    owner.Stop();

    EXPECT_TRUE(contender.AcquireCreationLock(key));
    EXPECT_TRUE(contender.AcquireUsageExclusiveLock(key));

    contender.Stop();
    server.Stop();
}

}  // namespace score::mw::com::service_discovery
