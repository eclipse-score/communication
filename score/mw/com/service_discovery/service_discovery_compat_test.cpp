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
#include "score/mw/com/service_discovery/service_discovery_compat.h"
#include "score/mw/com/service_discovery/service_discovery_daemon.h"
#include "score/mw/com/service_discovery/service_discovery_message_passing_server.h"
#include "score/mw/com/service_discovery/service_discovery_registry.h"

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/runtime_mock.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace score::mw::com::impl
{

TEST(ServiceDiscoveryCompatTest, OfferFindAndStartFindServiceRemainCompatible)
{
    struct CallbackState
    {
        std::mutex mutex{};
        std::condition_variable cv{};
        bool called{false};
        ServiceHandleContainer<HandleType> handles{};
        FindServiceHandle handle{make_FindServiceHandle(0U)};
    };

    score::mw::com::service_discovery::ServiceDiscoveryRegistry registry{};
    score::mw::com::service_discovery::ServiceDiscoveryDaemon daemon{registry};

    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingServer::Config server_config{};
    server_config.service_identifier = "mw_com_service_discovery_compat";

    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingServer server{daemon, server_config};
    ASSERT_TRUE(server.Start());

    RuntimeMock runtime_mock{};

    const ServiceIdentifierType service_identifier{make_ServiceIdentifierType("foo")};
    const auto instance_specifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();
    LolaServiceInstanceDeployment service_instance_deployment{};
    service_instance_deployment.instance_id_ = LolaServiceInstanceId{0x42};
    service_instance_deployment.allowed_consumer_ = {{QualityType::kASIL_QM, {42}}};
    ServiceTypeDeployment type_deployment{LolaServiceTypeDeployment{0x31}};
    ServiceInstanceDeployment instance_deployment{
        service_identifier, service_instance_deployment, QualityType::kASIL_QM, instance_specifier};
    const auto instance_identifier = make_InstanceIdentifier(instance_deployment, type_deployment);

    score::mw::com::service_discovery::ServiceDiscoveryMessagePassingClient::Config client_config{};
    client_config.service_identifier = server_config.service_identifier;

    ServiceDiscoveryCompat provider{runtime_mock, client_config};
    ServiceDiscoveryCompat consumer{runtime_mock, client_config};

    ASSERT_TRUE(provider.OfferService(instance_identifier).has_value());

    const auto find_result = consumer.FindService(instance_identifier);
    ASSERT_TRUE(find_result.has_value());
    ASSERT_EQ(find_result->size(), 1U);

    CallbackState callback_state{};
    const auto start_result = consumer.StartFindService(
        [&callback_state](ServiceHandleContainer<HandleType> handles, FindServiceHandle handle) {
            {
                std::scoped_lock lock{callback_state.mutex};
                callback_state.called = true;
                callback_state.handles = std::move(handles);
                callback_state.handle = handle;
            }
            callback_state.cv.notify_all();
        },
        instance_identifier);
    ASSERT_TRUE(start_result.has_value());

    {
        std::unique_lock lock{callback_state.mutex};
        ASSERT_TRUE(callback_state.cv.wait_for(lock, std::chrono::milliseconds{1500}, [&callback_state]() noexcept {
            return callback_state.called;
        }));
        ASSERT_EQ(callback_state.handles.size(), 1U);
    }

    EXPECT_TRUE(consumer.StopFindService(*start_result).has_value());
    EXPECT_TRUE(provider.StopOfferService(instance_identifier).has_value());

    server.Stop();
}

}  // namespace score::mw::com::impl
