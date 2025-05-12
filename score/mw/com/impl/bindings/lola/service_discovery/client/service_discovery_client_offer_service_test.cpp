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
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/mw/com/impl/i_service_discovery.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <memory>
#include <string>

namespace score::mw::com::impl::lola::test
{
namespace
{

using namespace ::testing;

const auto kOldFlagFileDirectory = GetServiceDiscoveryPath() / "1/1";
const auto kOldFlagFile = kOldFlagFileDirectory / "123456_asil-qm_1234";

constexpr auto kQmPathLabel{"asil-qm"};
constexpr auto kAsilBPathLabel{"asil-b"};

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create("/bla/blub/specifier").value();
ConfigurationStore kConfigStoreQm{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};
ConfigurationStore kConfigStoreAsilB{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_B,
    kServiceId,
    LolaServiceInstanceId{3U},
};

using ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture = ServiceDiscoveryClientWithFakeFileSystemFixture;
TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, CreatesFlagFileOnAsilQmServiceOffer)
{
    // Given a ServiceDiscoveryClient which saves the generated flag file path
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    // When offering a QM service
    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier()).has_value());

    // Then an ASIL QM flag file will be created
    ASSERT_EQ(flag_file_path_.size(), 1);
    EXPECT_NE(flag_file_path_.front().Native().find(kQmPathLabel), std::string::npos);
    EXPECT_TRUE(filesystem_mock_.standard->Exists(flag_file_path_.front()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, OfferingAnAlreadyOfferedServiceReturnsError)
{
    // Given a ServiceDiscoveryClient which saves the generated flag file path and an already offered service
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();
    const auto qm_instance_identifier{kConfigStoreQm.GetInstanceIdentifier()};
    score::cpp::ignore = service_discovery_client_->OfferService(qm_instance_identifier);

    // When offering the same service again
    const auto offer_service_result = service_discovery_client_->OfferService(qm_instance_identifier);

    // Then an error is returned
    ASSERT_FALSE(offer_service_result.has_value());
    EXPECT_EQ(offer_service_result.error(), ComErrc::kBindingFailure);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, CreatesFlagFilesOnAsilBServiceOffer)
{
    // Given a ServiceDiscoveryClient which saves the generated flag file path
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    // When offering an ASIL B service
    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier()).has_value());

    // Then ASIL QM and ASIL B flag files will be created
    ASSERT_EQ(flag_file_path_.size(), 2);
    EXPECT_NE(flag_file_path_.front().Native().find(kAsilBPathLabel), std::string::npos);
    EXPECT_TRUE(filesystem_mock_.standard->Exists(flag_file_path_.front()).value());
    EXPECT_NE(flag_file_path_.back().Native().find(kQmPathLabel), std::string::npos);
    EXPECT_TRUE(filesystem_mock_.standard->Exists(flag_file_path_.back()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, QMFlagFilePathIsMappedFromQMInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-32157630");
    RecordProperty("Description",
                   "Checks that the QM flag file path is derived from the corresponding QM instance identifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient which saves the generated flag file path
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    // When offering the service
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier());

    // Then the generated QM flag file path should match the expected pattern
    const auto expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreQm.lola_instance_id_->GetId()).Native();

    ASSERT_EQ(flag_file_path_.size(), 1);
    EXPECT_EQ(flag_file_path_.front().Native().find(expected_instance_directory_path), 0);
    EXPECT_NE(flag_file_path_.front().Native().find(kQmPathLabel), std::string::npos);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture,
       AsilBFlagFilePathIsMappedFromAsilBInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-32157630");
    RecordProperty(
        "Description",
        "Checks that the ASIL B flag file path is derived from the corresponding ASIL B instance identifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient which saves the generated flag file path
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    // When offering the service
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier());

    // Then the generated ASIL-B flag file path should match the expected pattern
    const std::string expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreAsilB.lola_instance_id_->GetId());

    ASSERT_EQ(flag_file_path_.size(), 2);
    EXPECT_EQ(flag_file_path_.front().Native().find(expected_instance_directory_path), 0);
    EXPECT_NE(flag_file_path_.front().Native().find(kAsilBPathLabel), std::string::npos);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, QMFlagFilePathIsMappedFromAsilBInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-32157630");
    RecordProperty("Description",
                   "Checks that the QM flag file path is derived from the corresponding ASIL B instance identifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient which saves the generated flag file path
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    // When offering the service
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier());

    // Then the generated QM flag file path should match the expected pattern
    const std::string expected_instance_directory_path =
        GenerateExpectedInstanceDirectoryPath(kServiceId, kConfigStoreAsilB.lola_instance_id_->GetId());

    ASSERT_EQ(flag_file_path_.size(), 2);
    EXPECT_EQ(flag_file_path_.back().Native().find(expected_instance_directory_path), 0);
    EXPECT_NE(flag_file_path_.back().Native().find(kQmPathLabel), std::string::npos);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, TwoConsecutiveFlagFilesHaveDifferentName)
{
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier()).has_value());
    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());
    const auto first_flag_file_path = flag_file_path_;

    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier()).has_value());

    ASSERT_EQ(flag_file_path_.size(), 2);
    EXPECT_NE(flag_file_path_.front(), flag_file_path_.back());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, OfferServiceReturnsErrorIfQMFlagFileCannotBeCreated)
{
    const auto flag_file_qm_string_regex = GetServiceDiscoveryPath() / ".*asil-qm.*";
    ON_CALL(*file_factory_mock_, Open(::testing::MatchesRegex(flag_file_qm_string_regex), _))
        .WillByDefault(Return(ByMove(
            score::MakeUnexpected<std::unique_ptr<std::iostream>>(filesystem::ErrorCode::kCouldNotOpenFileStream))));

    WhichContainsAServiceDiscoveryClient();

    const auto result = service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kServiceNotOffered);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture,
       OfferServiceReturnsErrorIfAsilBFlagFileCannotBeCreated)
{
    const auto flag_file_asil_b_string_regex = GetServiceDiscoveryPath() / ".*asil-b.*";
    ON_CALL(*file_factory_mock_, Open(::testing::MatchesRegex(flag_file_asil_b_string_regex), _))
        .WillByDefault(Return(ByMove(
            score::MakeUnexpected<std::unique_ptr<std::iostream>>(filesystem::ErrorCode::kCouldNotOpenFileStream))));

    WhichContainsAServiceDiscoveryClient();

    const auto result = service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kServiceNotOffered);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, OfferServiceRemovesOldFlagFilesInTheSearchPath)
{
    CreateRegularFile(filesystem_mock_, kOldFlagFile);

    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    EXPECT_TRUE(standard_filesystem_fake_->Exists(kOldFlagFile).value());
    EXPECT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier()).has_value());
    EXPECT_FALSE(standard_filesystem_fake_->Exists(kOldFlagFile).value());
    EXPECT_TRUE(standard_filesystem_fake_->Exists(flag_file_path_.front()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, RemovesFlagFileOnStopServiceOffer)
{
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm.GetInstanceIdentifier()).has_value());
    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());

    EXPECT_FALSE(filesystem_mock_.standard->Exists(flag_file_path_.front()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemOfferServiceFixture, OfferingServiceWithInvalidQualityTypeReturnsError)
{
    // Given a ServiceDiscoveryClient and a service configuration with an invalid quality type
    const ConfigurationStore config_store_invalid{
        kInstanceSpecifierString,
        make_ServiceIdentifierType("foo"),
        QualityType::kInvalid,
        kServiceId,
        LolaServiceInstanceId{1U},
    };
    WhichContainsAServiceDiscoveryClient();

    // When offering the service
    const auto offer_service_result =
        service_discovery_client_->OfferService(config_store_invalid.GetInstanceIdentifier());

    // Then an error should be returned
    ASSERT_FALSE(offer_service_result.has_value());
    EXPECT_EQ(offer_service_result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
