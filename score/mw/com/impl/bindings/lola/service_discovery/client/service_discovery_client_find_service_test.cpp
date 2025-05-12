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
#include "score/filesystem/file_status.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"

#include "score/filesystem/error.h"
#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <string>

namespace score::mw::com::impl::lola::test
{
namespace
{

using namespace ::testing;

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create("/bla/blub/specifier").value();
ConfigurationStore kConfigStoreQm1{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};
ConfigurationStore kConfigStoreQm2{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{2U},
};
ConfigurationStore kConfigStoreFindAny{kInstanceSpecifierString,
                                       make_ServiceIdentifierType("foo"),
                                       QualityType::kASIL_QM,
                                       kServiceId,
                                       score::cpp::optional<LolaServiceInstanceId>{}};

HandleType kHandleFindAnyQm1{kConfigStoreFindAny.GetHandle(kConfigStoreQm1.lola_instance_id_.value())};
HandleType kHandleFindAnyQm2{kConfigStoreFindAny.GetHandle(kConfigStoreQm2.lola_instance_id_.value())};

using ServiceDiscoveryClientFindServiceFixture = ServiceDiscoveryClientFixture;
TEST_F(ServiceDiscoveryClientFindServiceFixture, AddsNoWatchOnFindService)
{
    // Expecting that _no_ watches are added
    EXPECT_CALL(inotify_instance_mock_, AddWatch(_, _)).Times(0);

    // Given a ServiceDiscovery client which offers a service
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // When finding a services as one shot
    const auto find_service_result =
        service_discovery_client_->FindService(kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    // Then still a service is found
    ASSERT_TRUE(find_service_result.has_value());
    ASSERT_EQ(find_service_result.value().size(), 1);
    EXPECT_EQ(find_service_result.value()[0], kConfigStoreQm1.GetHandle());
}

TEST_F(ServiceDiscoveryClientFindServiceFixture, FindServiceReturnHandleIfServiceFound)
{
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    const auto find_service_result =
        service_discovery_client_->FindService(kConfigStoreQm1.GetEnrichedInstanceIdentifier());
    EXPECT_TRUE(find_service_result.has_value());
    EXPECT_EQ(find_service_result.value().size(), 1);
    EXPECT_EQ(find_service_result.value()[0], kConfigStoreQm1.GetHandle());
}

TEST_F(ServiceDiscoveryClientFindServiceFixture, FindServiceReturnHandlesForAny)
{
    // Given that two services are offered
    WhichContainsAServiceDiscoveryClient()
        .WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier())
        .WithAnOfferedService(kConfigStoreQm2.GetInstanceIdentifier());

    // When finding services one shot with ANY
    const auto service_handle_container_result =
        service_discovery_client_->FindService(kConfigStoreFindAny.GetEnrichedInstanceIdentifier());

    // Then two services are found
    ASSERT_TRUE(service_handle_container_result.has_value());
    EXPECT_EQ(service_handle_container_result.value().size(), 2);
    auto& service_handle_container = service_handle_container_result.value();

    // whose handles contain InstanceIdentifiers without an InstanceId and explitily set InstanceIds in the handles
    // themselves
    const auto expected_handle_0 =
        make_HandleType(kConfigStoreFindAny.GetInstanceIdentifier(), kConfigStoreQm1.lola_instance_id_.value());
    const auto expected_handle_1 =
        make_HandleType(kConfigStoreFindAny.GetInstanceIdentifier(), kConfigStoreQm2.lola_instance_id_.value());
    EXPECT_THAT(service_handle_container, Contains(expected_handle_0));
    EXPECT_THAT(service_handle_container, Contains(expected_handle_1));
}

TEST_F(ServiceDiscoveryClientFindServiceFixture, FindServiceReturnNoHandleIfServiceNotFound)
{
    WhichContainsAServiceDiscoveryClient();

    const auto find_service_result =
        service_discovery_client_->FindService(kConfigStoreQm1.GetEnrichedInstanceIdentifier());
    EXPECT_TRUE(find_service_result.has_value());
    EXPECT_EQ(find_service_result.value().size(), 0);
}

using ServiceDiscoveryClientWithFakeFileSystemFindServiceFixture = ServiceDiscoveryClientWithFakeFileSystemFixture;
TEST_F(ServiceDiscoveryClientWithFakeFileSystemFindServiceFixture,
       FindServiceReturnsErrorWhenFailingToGetTheStatusOfInstanceDirectory)
{
    // Expecting that all calls to Status will use the fake filesystem
    EXPECT_CALL(*standard_filesystem_fake_, Status(_)).Times(AnyNumber()).WillRepeatedly(DoDefault());

    // and that Status is called on the instance directory search path once during FindService which returns an error
    const auto instance_directory_search_path = GenerateExpectedInstanceDirectoryPath(
        kConfigStoreQm1.lola_service_type_deployment_.service_id_, kConfigStoreQm1.lola_instance_id_.value().GetId());
    EXPECT_CALL(*standard_filesystem_fake_, Status(instance_directory_search_path))
        .WillOnce(Return(MakeUnexpected<filesystem::FileStatus>(
            filesystem::MakeError(filesystem::ErrorCode::kCorruptedFileSystem))));

    // and that Status() is called once with the instance directory search path when making the flag file which uses the
    // fake filesystem
    EXPECT_CALL(*standard_filesystem_fake_, Status(instance_directory_search_path)).Times(1).RetiresOnSaturation();

    // Given a ServiceDiscovery client which offers a service
    WhichContainsAServiceDiscoveryClient().WithAnOfferedService(kConfigStoreQm1.GetInstanceIdentifier());

    // When calling FindService with a find any search
    const auto find_service_result =
        service_discovery_client_->FindService(kConfigStoreFindAny.GetEnrichedInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(find_service_result.has_value());
    EXPECT_EQ(find_service_result.error(), ComErrc::kBindingFailure);
}

using ServiceDiscoveryClientFindServiceDeathTest = ServiceDiscoveryClientFindServiceFixture;
TEST_F(ServiceDiscoveryClientFindServiceDeathTest, CallingFindServiceWithOfferedServiceWithInvalidQualityTypeTerminates)
{
    const ConfigurationStore config_store_invalid_quality_type{
        kInstanceSpecifierString,
        make_ServiceIdentifierType("foo"),
        QualityType::kInvalid,
        kServiceId,
        LolaServiceInstanceId{1U},
    };

    // Since CreateAServiceDiscoveryClient() spawns a thread, it should be called within the EXPECT_DEATH context.
    const auto test_function = [this, &config_store_invalid_quality_type] {
        // Given a ServiceDiscoveryClient
        WhichContainsAServiceDiscoveryClient();

        // When calling StartFindService with an InstanceIdentifier with an invalid quality type
        // Then the program terminates
        score::cpp::ignore =
            service_discovery_client_->FindService(config_store_invalid_quality_type.GetEnrichedInstanceIdentifier());
    };
    EXPECT_DEATH(test_function(), ".*");
}

TEST_F(ServiceDiscoveryClientFindServiceDeathTest, CallingFindServiceWithUnknownQualityTypeTerminates)
{
    const auto unknown_quality_type = static_cast<QualityType>(100U);
    const ConfigurationStore config_store_unknown_quality_type{
        kInstanceSpecifierString,
        make_ServiceIdentifierType("foo"),
        unknown_quality_type,
        kServiceId,
        LolaServiceInstanceId{1U},
    };

    // Since CreateAServiceDiscoveryClient() spawns a thread, it should be called within the EXPECT_DEATH context.
    const auto test_function = [this, &config_store_unknown_quality_type] {
        // Given a ServiceDiscoveryClient
        WhichContainsAServiceDiscoveryClient();

        // When calling StartFindService with an InstanceIdentifier with an unknown quality type
        // Then the program terminates
        score::cpp::ignore =
            service_discovery_client_->FindService(config_store_unknown_quality_type.GetEnrichedInstanceIdentifier());
    };
    EXPECT_DEATH(test_function(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
