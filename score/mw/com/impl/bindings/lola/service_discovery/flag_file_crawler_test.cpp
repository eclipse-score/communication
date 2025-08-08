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
#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file_crawler.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/filesystem/factory/filesystem_factory_fake.h"
#include "score/os/utils/inotify/inotify_instance_mock.h"

#include <score/expected.hpp>

#include <gtest/gtest.h>
#include <cerrno>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace score::mw::com::impl::lola
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::DoDefault;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

InstanceSpecifier kInstanceSpecifier{InstanceSpecifier::Create("/bla/blub/specifier").value()};
LolaServiceId kServiceId{1U};

LolaServiceInstanceId kInstanceId1{1U};
LolaServiceInstanceId kInstanceId2{2U};

const auto kServiceIdentifier = make_ServiceIdentifierType("/bla/blub/service1");
const ConfigurationStore kConfigStoreQm1{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_QM,
    kServiceId,
    kInstanceId1,
};

const ConfigurationStore kConfigStoreQm2{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_QM,
    kServiceId,
    kInstanceId2,
};

const ConfigurationStore kConfigStoreAsilB1{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_B,
    kServiceId,
    kInstanceId1,
};

const ConfigurationStore kConfigStoreAsilB2{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_B,
    kServiceId,
    kInstanceId2,
};

const ConfigurationStore kConfigStoreQmAny{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_QM,
    kServiceId,
    score::cpp::optional<LolaServiceInstanceId>{},
};

const ConfigurationStore kConfigStoreAsilBAny{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kASIL_B,
    kServiceId,
    score::cpp::optional<LolaServiceInstanceId>{},
};

const ConfigurationStore kConfigStoreInvalidQualityAny{
    kInstanceSpecifier,
    kServiceIdentifier,
    QualityType::kInvalid,
    kServiceId,
    score::cpp::optional<LolaServiceInstanceId>{},
};

EnrichedInstanceIdentifier kEnrichedInstanceIdentifier1Invalid{kConfigStoreQm1.GetEnrichedInstanceIdentifier(),
                                                               QualityType::kInvalid};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifierAnyInvalid{kConfigStoreQmAny.GetEnrichedInstanceIdentifier(),
                                                                 QualityType::kInvalid};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifierAnyInvalid1{
    kConfigStoreQmAny.GetEnrichedInstanceIdentifier(ServiceInstanceId{kInstanceId1}),
    QualityType::kInvalid};

constexpr std::uint8_t kMaxNumberOfWatchAndCrawlRetries{3U};
constexpr pid_t kPid1{42};
constexpr pid_t kPid2{43};
os::InotifyWatchDescriptor kExpectedDescriptor1{2};
os::InotifyWatchDescriptor kExpectedDescriptor2{3};

filesystem::Perms kAllPerms{filesystem::Perms::kReadWriteExecUser | filesystem::Perms::kReadWriteExecGroup |
                            filesystem::Perms::kReadWriteExecOthers};
filesystem::Perms kUserWriteRestRead{filesystem::Perms::kReadUser | filesystem::Perms::kWriteUser |
                                     filesystem::Perms::kReadGroup | filesystem::Perms::kReadOthers};

class FlagFileCrawlerFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        filesystem::StandardFilesystem::set_testing_instance(*(filesystem_.standard));
        ON_CALL(inotify_instance_, AddWatch(_, _)).WillByDefault(Return(os::InotifyWatchDescriptor{0}));
    }

    void TearDown() override
    {
        filesystem::StandardFilesystem::restore_instance();
    }

    FlagFileCrawlerFixture& GivenAFlagFileCrawler()
    {
        flag_file_crawler_ = std::make_unique<FlagFileCrawler>(inotify_instance_, filesystem_);
        return *this;
    }

    filesystem::Path GetServiceIdSearchPath(const ConfigurationStore& configuration_store)
    {
        return test::GetServiceDiscoveryPath() / std::to_string(static_cast<std::uint32_t>(
                                                     configuration_store.lola_service_type_deployment_.service_id_));
    }

    filesystem::Path GetInstanceIdSearchPath(const ConfigurationStore& configuration_store)
    {
        return GetServiceIdSearchPath(configuration_store) /
               std::to_string(static_cast<std::uint32_t>(configuration_store.lola_instance_id_.value().GetId()));
    }

    void CreateFlagFile(const pid_t pid, const ConfigurationStore& configuration_store)
    {
        const auto instance_id_search_path = GetInstanceIdSearchPath(configuration_store);

        const auto disambiguator{std::chrono::steady_clock::now()};
        const auto disambiguator_string = std::to_string(disambiguator.time_since_epoch().count());

        const auto flag_file_name = std::to_string(pid) + "_" +
                                    std::string{GetQualityTypeString(configuration_store.quality_type_)} + "_" +
                                    disambiguator_string;
        const filesystem::Path flag_file_path = instance_id_search_path / flag_file_name;

        filesystem_factory_fake_.GetUtils().CreateDirectories(instance_id_search_path, kAllPerms);
        filesystem_factory_fake_.GetStandard().CreateRegularFile(flag_file_path, kUserWriteRestRead);
    }

    filesystem::FilesystemFactoryFake filesystem_factory_fake_{};
    filesystem::Filesystem filesystem_{filesystem_factory_fake_.CreateInstance()};
    os::InotifyInstanceMock inotify_instance_{};
    std::unique_ptr<FlagFileCrawler> flag_file_crawler_{nullptr};
};

// Note. We assume that the Crawl functionality is same in Crawl and CrawlAndWatch. Therefore, we only test the crawl
// functionality in the fixtures testing Crawl()
using FlagFileCrawlerCrawlSpecificInstanceFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerCrawlSpecificInstanceFixture,
       ReturnsEmptyInstanceContainersIfNoFlagFilesAreFoundInInstanceDirectory)
{
    GivenAFlagFileCrawler();

    // Given that that no flag files exist in the instance directory of a service

    // When calling Crawl for a specific instance ID
    const auto existing_instances_result = flag_file_crawler_->Crawl(kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    // Then the returned instance containers will be empty
    ASSERT_TRUE(existing_instances_result.has_value());
    EXPECT_TRUE(existing_instances_result.value().asil_b.Empty());
    EXPECT_TRUE(existing_instances_result.value().asil_qm.Empty());
}

using FlagFileCrawlerCrawlAnyInstanceFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture, ReturnsEmptyInstanceContainersIfNoFlagFilesAreFoundInInstanceDirectory)
{
    GivenAFlagFileCrawler();

    // Given that that no flag files exist in the instance directory of a service

    // When calling Crawl for any instance ID
    const auto existing_instances_result = flag_file_crawler_->Crawl(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then the returned instance containers will be empty
    ASSERT_TRUE(existing_instances_result.has_value());
    EXPECT_TRUE(existing_instances_result.value().asil_b.Empty());
    EXPECT_TRUE(existing_instances_result.value().asil_qm.Empty());
}

TEST_F(FlagFileCrawlerCrawlSpecificInstanceFixture, ReturnsHandlesForFoundInstancesInCorrectContainersForQmInstanceId)
{
    // Given multiple flag files corresponding to different instances / quality types of the same service exist
    CreateFlagFile(kPid1, kConfigStoreQm1);
    CreateFlagFile(kPid2, kConfigStoreQm2);
    CreateFlagFile(kPid1, kConfigStoreAsilB1);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    // When calling Crawl with an InstanceIdentifier containing a specific instance ID and of QM quality
    const auto& searched_instance = kConfigStoreQm2;
    const auto existing_instances_result = flag_file_crawler_->Crawl(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_TRUE(existing_instances_result.has_value());

    // Then the instances container will contain a single handle for the existing instance corresponding to the searched
    // instance with quality type QM
    const auto asil_qm_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_qm_handles.size(), 1U);
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle()));

    // and the instances container will contain a single handle for the existing instance corresponding to the searched
    // instance with quality type ASIL-B
    const auto asil_b_handles =
        existing_instances_result.value().asil_b.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_b_handles.size(), 1U);
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle()));
}

TEST_F(FlagFileCrawlerCrawlSpecificInstanceFixture,
       ReturnsHandlesForFoundInstancesInCorrectContainersForAsilBInstanceId)
{
    // Given multiple flag files corresponding to different instances / quality types of the same service exist
    CreateFlagFile(kPid1, kConfigStoreQm1);
    CreateFlagFile(kPid2, kConfigStoreQm2);
    CreateFlagFile(kPid1, kConfigStoreAsilB1);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    // When calling Crawl with an InstanceIdentifier containing a specific instance ID and of ASIL-B quality
    const auto& searched_instance = kConfigStoreAsilB2;
    const auto existing_instances_result = flag_file_crawler_->Crawl(searched_instance.GetEnrichedInstanceIdentifier());

    // Then the instances container will contain a single handle for the existing instance corresponding to the searched
    // instance with quality type QM
    const auto asil_qm_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_qm_handles.size(), 1U);
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle()));

    // and the instances container will contain a single handle for the existing instance corresponding to the searched
    // instance with quality type ASIL-B
    const auto asil_b_handles =
        existing_instances_result.value().asil_b.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_b_handles.size(), 1U);
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle()));
}

TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture,
       ReturnsAllCorrespondingInstancesWithExistingFlagFilesInCorrectContainersForQmInstanceId)
{
    // Given multiple flag files corresponding to different instances / quality types of the same service exist
    CreateFlagFile(kPid1, kConfigStoreQm1);
    CreateFlagFile(kPid2, kConfigStoreQm2);
    CreateFlagFile(kPid1, kConfigStoreAsilB1);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    // When calling Crawl with an InstanceIdentifier containing no specific instance ID of QM quality
    const auto& searched_instance = kConfigStoreQmAny;
    const auto existing_instances_result = flag_file_crawler_->Crawl(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_TRUE(existing_instances_result.has_value());

    // Then the instances container will contain all the handles for instances corresponding to the searched service
    // with quality type QM
    const auto asil_qm_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_qm_handles.size(), 2U);
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle(kInstanceId1)));
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle(kInstanceId2)));

    // and the instances container will contain all the handles for instances corresponding to the searched service
    // with quality type ASIL-B
    const auto asil_b_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_b_handles.size(), 2U);
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle(kInstanceId1)));
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle(kInstanceId2)));
}

TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture,
       ReturnsAllCorrespondingInstancesWithExistingFlagFilesInCorrectContainersForAsilBInstanceId)
{
    // Given multiple flag files corresponding to different instances / quality types of the same service exist
    CreateFlagFile(kPid1, kConfigStoreQm1);
    CreateFlagFile(kPid2, kConfigStoreQm2);
    CreateFlagFile(kPid1, kConfigStoreAsilB1);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    // When calling Crawl with an InstanceIdentifier containing no specific instance ID of ASIL-B quality
    const auto& searched_instance = kConfigStoreAsilBAny;
    const auto existing_instances_result = flag_file_crawler_->Crawl(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_TRUE(existing_instances_result.has_value());

    // Then the instances container will contain all the handles for instances corresponding to the searched service
    // with quality type QM
    const auto asil_qm_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_qm_handles.size(), 2U);
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle(kInstanceId1)));
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle(kInstanceId2)));

    // and the instances container will contain all the handles for instances corresponding to the searched service
    // with quality type ASIL-B
    const auto asil_b_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_b_handles.size(), 2U);
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle(kInstanceId1)));
    EXPECT_THAT(asil_b_handles, Contains(searched_instance.GetHandle(kInstanceId2)));
}

TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture, IgnoresInvalidInstanceDirectories)
{
    // Given a flag file corresponding to a specific instance of a service exists
    CreateFlagFile(kPid1, kConfigStoreQm1);

    // and that the service ID directory contains a directory with an invalid name (whose name is not a stringified
    // instance id)
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQmAny);
    const auto invalid_instance_directory = service_search_path / "invalid_directory_name";
    filesystem_factory_fake_.GetUtils().CreateDirectories(invalid_instance_directory, kAllPerms);

    GivenAFlagFileCrawler();

    // When calling Crawl for any instance ID
    const auto& searched_instance = kConfigStoreAsilBAny;
    const auto existing_instances_result = flag_file_crawler_->Crawl(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_TRUE(existing_instances_result.has_value());

    // Then the instances container will only contain the handle for the instance corresponding to the searched service
    // with quality type QM
    const auto asil_qm_handles =
        existing_instances_result.value().asil_qm.GetKnownHandles(searched_instance.GetEnrichedInstanceIdentifier());
    ASSERT_EQ(asil_qm_handles.size(), 1U);
    EXPECT_THAT(asil_qm_handles, Contains(searched_instance.GetHandle(kInstanceId1)));
}

TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture, ReturnsErrorWhenGettingDirectoryStatusReturnsError)
{
    GivenAFlagFileCrawler();

    // Given that a flag file exists in the instance directory of a service
    CreateFlagFile(kPid1, kConfigStoreQm1);

    // and given that calling Status on the instance directory returns an error
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1);
    ON_CALL(filesystem_factory_fake_.GetStandard(), Status(instance_search_path))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kNotImplemented)));

    // When calling Crawl for any instance ID
    const auto crawler_result = flag_file_crawler_->Crawl(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
    EXPECT_EQ(crawler_result.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileCrawlerCrawlSpecificInstanceFixture, DoesNotAddAnyWatch)
{
    GivenAFlagFileCrawler();

    // Expecting that a watch will NOT be added on any path
    EXPECT_CALL(inotify_instance_, AddWatch(_, _)).Times(0);

    // When calling Crawl for a specific instance ID
    score::cpp::ignore = flag_file_crawler_->Crawl(kConfigStoreQm1.GetEnrichedInstanceIdentifier());
}

using FlagFileCrawlerCrawlAnyInstanceFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerCrawlAnyInstanceFixture, DoesNotAddAnyWatch)
{
    GivenAFlagFileCrawler();

    // Expecting that a watch will NOT be added on any path
    EXPECT_CALL(inotify_instance_, AddWatch(_, _)).Times(0);

    // When calling Crawl for any instance ID
    score::cpp::ignore = flag_file_crawler_->Crawl(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());
}

using FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, AddsWatchOnlyForInstance)
{
    GivenAFlagFileCrawler();

    // Expecting that a watch will NOT be added on the service ID path
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(inotify_instance_,
                AddWatch(service_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .Times(0);

    // and expecting that a watch will be added on the instance ID path
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(inotify_instance_,
                AddWatch({instance_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    // When calling CrawlAndWatch for a specific instance ID
    score::cpp::ignore = flag_file_crawler_->CrawlAndWatch(kConfigStoreQm1.GetEnrichedInstanceIdentifier());
}

using FlagFileCrawlerCrawlAndWatchAnyInstanceFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, AddsWatchForServiceId)
{
    GivenAFlagFileCrawler();

    // Expecting that a watch will be added on the service ID path
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQmAny).Native();
    EXPECT_CALL(inotify_instance_,
                AddWatch(service_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete));

    // When calling CrawlAndWatch for any instance ID
    score::cpp::ignore = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, AddsWatchForExistingInstanceId)
{
    // Given that an Instance ID directory already exists
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    filesystem_factory_fake_.GetUtils().CreateDirectories(instance_search_path, kAllPerms);
    GivenAFlagFileCrawler();

    // Expecting that a watch will be added on the service ID path
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQmAny).Native();
    EXPECT_CALL(inotify_instance_,
                AddWatch(service_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    // and expecting that a watch will be added on the existing instance ID path
    EXPECT_CALL(inotify_instance_,
                AddWatch(instance_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    score::cpp::ignore = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, ReturnsAddedInstanceIdWatchDescriptor)
{
    GivenAFlagFileCrawler();

    // Given that a watch will be added on the instance ID path
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(inotify_instance_,
                AddWatch({instance_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    // When calling CrawlAndWatch for a specific instance ID
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    // Then the result will contain the watch descriptor added to the instance ID directory
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    ASSERT_EQ(descriptors.size(), 1U);
    EXPECT_THAT(descriptors, Contains(std::make_pair(kExpectedDescriptor1, kEnrichedInstanceIdentifier1Invalid)));
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, ReturnsAddedServiceIdWatchDescriptor)
{
    GivenAFlagFileCrawler();

    // Given that a watch will be added on the service ID path
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQmAny).Native();
    ON_CALL(inotify_instance_,
            AddWatch(service_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillByDefault(Return(kExpectedDescriptor1));

    // When calling CrawlAndWatch for any instance ID
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then the result will contain the watch descriptor added to the service ID directory
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    ASSERT_EQ(descriptors.size(), 1U);
    EXPECT_THAT(descriptors, Contains(std::make_pair(kExpectedDescriptor1, kEnrichedInstanceIdentifierAnyInvalid)));
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, ReturnsAddedExistingInstanceIdWatchDescriptor)
{
    GivenAFlagFileCrawler();

    // Given that an Instance ID directory already exists
    const auto instance__qm_1_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    filesystem_factory_fake_.GetUtils().CreateDirectories(instance__qm_1_search_path, kAllPerms);

    // and that a watch will be added on the service ID path which returns a valid descriptor
    const auto service_search_path = GetServiceIdSearchPath(kConfigStoreQmAny).Native();
    ON_CALL(inotify_instance_,
            AddWatch(service_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillByDefault(Return(kExpectedDescriptor1));

    // and that a watch will be added on the existing instance ID path which returns a different valid descriptor
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    ON_CALL(inotify_instance_,
            AddWatch(instance_search_path, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillByDefault(Return(kExpectedDescriptor2));

    // When calling CrawlAndWatch for any instance iD
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then the result will contain the watch descriptors from the watch added to the service ID / instance ID
    // directories
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    EXPECT_THAT(descriptors, Contains(std::make_pair(kExpectedDescriptor1, kEnrichedInstanceIdentifierAnyInvalid)));
    EXPECT_THAT(descriptors, Contains(std::make_pair(kExpectedDescriptor2, kEnrichedInstanceIdentifierAnyInvalid1)));
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, IgnoresDirectoriesInInstanceIdDirectories)
{
    CreateFlagFile(kPid1, kConfigStoreQm1);

    GivenAFlagFileCrawler();

    // We create a flag file in the subdirectory for an ASIL-B offer that we expect to NOT find below
    const auto instance_search_path = GetInstanceIdSearchPath(kConfigStoreQm1);
    const auto broken_flag_file_path = instance_search_path / "1234_asil-b_5678";

    filesystem_factory_fake_.GetUtils().CreateDirectories(broken_flag_file_path, kAllPerms);
    filesystem_factory_fake_.GetStandard().CreateRegularFile(broken_flag_file_path / "1234_asil-b_5678",
                                                             kUserWriteRestRead);

    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    EXPECT_TRUE(instances.asil_b.Empty());
    const auto asil_qm_handles = instances.asil_qm.GetKnownHandles(
        kConfigStoreQmAny.GetEnrichedInstanceIdentifier(ServiceInstanceId{kInstanceId1}));
    EXPECT_THAT(asil_qm_handles, Contains(kConfigStoreQmAny.GetHandle(kInstanceId1)));
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, IgnoresFilesOnInstanceIdDirectoryLevel)
{
    const auto service_path = test::GetServiceDiscoveryPath() / "1";
    filesystem_factory_fake_.GetUtils().CreateDirectories(service_path, kAllPerms);
    filesystem_factory_fake_.GetStandard().CreateRegularFile(service_path / "1", kUserWriteRestRead);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kEnrichedInstanceIdentifierAnyInvalid);
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    EXPECT_TRUE(instances.asil_qm.Empty());
    const auto asil_b_handles = instances.asil_b.GetKnownHandles(kConfigStoreAsilB2.GetEnrichedInstanceIdentifier());
    EXPECT_THAT(asil_b_handles, Contains(kConfigStoreAsilB2.GetHandle()));
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, IgnoresDirectoryOnInstanceIdIfCannotBeParsedToInstanceId)
{
    const auto service_path = test::GetServiceDiscoveryPath() / "whatever";
    filesystem_factory_fake_.GetUtils().CreateDirectories(service_path, kAllPerms);
    filesystem_factory_fake_.GetUtils().CreateDirectories(service_path / "a", kAllPerms);
    CreateFlagFile(kPid2, kConfigStoreAsilB2);

    GivenAFlagFileCrawler();

    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kEnrichedInstanceIdentifierAnyInvalid);
    ASSERT_TRUE(crawler_result.has_value());
    const auto& [descriptors, instances] = crawler_result.value();
    EXPECT_TRUE(instances.asil_qm.Empty());
    const auto asil_b_handles = instances.asil_b.GetKnownHandles(kConfigStoreAsilB2.GetEnrichedInstanceIdentifier());
    EXPECT_THAT(asil_b_handles, Contains(kConfigStoreAsilB2.GetHandle()));
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, ReturnsErrorIfInitialWatchDirectoryCouldNotBeCreated)
{
    GivenAFlagFileCrawler();

    // Given that trying to create service instance directories returns an error
    const auto service_id_search_path = GetServiceIdSearchPath(kConfigStoreQmAny);
    ON_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(service_id_search_path, _))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kNotImplemented)));

    // When calling CrawlAndWatch
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
    EXPECT_EQ(crawler_result.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileCrawlerCrawlAndWatchAnyInstanceFixture, ReturnsErrorIfInitialWatchCouldNotBeCreated)
{
    GivenAFlagFileCrawler();

    // Given that trying to add a watch on the service directory returns an error
    const auto service_id_search_path = GetServiceIdSearchPath(kConfigStoreQmAny).Native();
    ON_CALL(inotify_instance_, AddWatch({service_id_search_path}, _))
        .WillByDefault(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EINTR))));

    // When calling CrawlAndWatch
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQmAny.GetEnrichedInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
    EXPECT_EQ(crawler_result.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, ReturnsErrorIfSubdirectoryWatchCouldNotBeCreated)
{
    CreateFlagFile(kPid1, kConfigStoreQm1);

    GivenAFlagFileCrawler();

    // Given that trying to add a watch on the instance directory returns an error
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    ON_CALL(inotify_instance_, AddWatch({instance_id_search_path}, _))
        .WillByDefault(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EINTR))));

    // When calling CrawlAndWatch
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kEnrichedInstanceIdentifierAnyInvalid);

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
    EXPECT_EQ(crawler_result.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture,
       LogsSearchPathPermissionsAsOctalIntegerOnStatusOperationNotPermittedError)
{
    // Given that trying to add a watch on the inotify instance returns an error
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1);
    EXPECT_CALL(inotify_instance_,
                AddWatch({instance_id_search_path.Native()},
                         os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EPERM))));

    // And that Status is called which returns a status containing permissions
    const auto file_permissions_octal = 777;
    filesystem::FileStatus file_status{filesystem::FileType::kDirectory, kAllPerms};
    ON_CALL(filesystem_factory_fake_.GetStandard(), Status(instance_id_search_path)).WillByDefault(Return(file_status));

    GivenAFlagFileCrawler();

    // capture stdout output during CrawlAndWatch.
    testing::internal::CaptureStdout();

    // When calling CrawlAndWatch
    score::cpp::ignore = flag_file_crawler_->CrawlAndWatch(kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    // stop capture, get captured data and print output for easier test debugging
    std::string log_output = testing::internal::GetCapturedStdout();
    std::cout << log_output << std::endl;

    // Then a message should be logged containing the file permissions in octal format
    std::stringstream expected_text_snippet{};
    expected_text_snippet << "Current file permissions are: " << file_permissions_octal;
    EXPECT_TRUE(log_output.find(expected_text_snippet.str()) != log_output.npos);
}

TEST_F(FlagFileCrawlerCrawlAndWatchSpecificInstanceFixture, ReturnsErrorIfCannotGetDirectoryStatusToCheckPermissions)
{
    // Set the default behaviour of Status for other calls to Status other than the expectations that we set in this
    // test.
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Status(_)).Times(AnyNumber()).WillRepeatedly(DoDefault());

    // Given that trying to add a watch on the inotify instance returns an error
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1);
    EXPECT_CALL(inotify_instance_,
                AddWatch({instance_id_search_path.Native()},
                         os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EPERM))));

    // And that Status is called first when creating the search path which returns success and then when checking the
    // permissions on the directory after AddWatch failed, which returns an error. Note. gtest will match the last
    // EXPECT_CALL first, so we expect Status to first succeed and then to return an error.
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Status(instance_id_search_path))
        .WillOnce(Return(MakeUnexpected(filesystem::ErrorCode::kNotImplemented)));
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Status(instance_id_search_path)).RetiresOnSaturation();

    GivenAFlagFileCrawler();

    // When calling CrawlAndWatch
    const auto crawler_result = flag_file_crawler_->CrawlAndWatch(kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
    EXPECT_EQ(crawler_result.error(), ComErrc::kBindingFailure);
}

using FlagFileCrawlerCrawlAndWatchWithRetryFixture = FlagFileCrawlerFixture;
TEST_F(FlagFileCrawlerFixture, ReturnsValidResultIfAddWatchSucceedsFirstTime)
{
    // Given that a watch is added on the inotify instance which returns a valid descriptor
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(
        inotify_instance_,
        AddWatch({instance_id_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    GivenAFlagFileCrawler();

    // When calling CrawlAndWatchWithRetry
    const auto crawler_result = flag_file_crawler_->CrawlAndWatchWithRetry(
        kConfigStoreQm1.GetEnrichedInstanceIdentifier(), kMaxNumberOfWatchAndCrawlRetries);

    // Then a valid result is returned
    ASSERT_TRUE(crawler_result.has_value());
}

TEST_F(FlagFileCrawlerCrawlAndWatchWithRetryFixture, AddsWatchForInstanceIdIfAddWatchSucceedsOnRetry)
{
    InSequence in_sequence{};

    // Given that a watch is added on the inotify instance which returns an error on the first try and a valid
    // descriptor on the second
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(
        inotify_instance_,
        AddWatch({instance_id_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EINTR))));
    EXPECT_CALL(
        inotify_instance_,
        AddWatch({instance_id_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .WillOnce(Return(kExpectedDescriptor1));

    GivenAFlagFileCrawler();

    // When calling CrawlAndWatchWithRetry
    const auto crawler_result = flag_file_crawler_->CrawlAndWatchWithRetry(
        kConfigStoreQm1.GetEnrichedInstanceIdentifier(), kMaxNumberOfWatchAndCrawlRetries);

    // Then a valid result is returned
    ASSERT_TRUE(crawler_result.has_value());
}

TEST_F(FlagFileCrawlerCrawlAndWatchWithRetryFixture, ReturnsErrorIfAddWatchFailsOnEveryRetry)
{
    // Given that a watch is added on the inotify instance which returns an error on every retry
    const auto instance_id_search_path = GetInstanceIdSearchPath(kConfigStoreQm1).Native();
    EXPECT_CALL(
        inotify_instance_,
        AddWatch({instance_id_search_path}, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete))
        .Times(kMaxNumberOfWatchAndCrawlRetries)
        .WillRepeatedly(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EINTR))));

    GivenAFlagFileCrawler();

    // When calling CrawlAndWatchWithRetry
    const auto crawler_result = flag_file_crawler_->CrawlAndWatchWithRetry(
        kConfigStoreQm1.GetEnrichedInstanceIdentifier(), kMaxNumberOfWatchAndCrawlRetries);

    // Then an error is returned
    ASSERT_FALSE(crawler_result.has_value());
}

class FlagFileCrawlerConvertFromStringToInstanceIdParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<std::string, LolaServiceInstanceId>>
{
};
INSTANTIATE_TEST_CASE_P(FlagFileCrawlerConvertFromStringToInstanceIdParamaterisedFixture,
                        FlagFileCrawlerConvertFromStringToInstanceIdParamaterisedFixture,
                        ::testing::Values(std::make_pair("0", 0U),
                                          std::make_pair("00000", 0U),
                                          std::make_pair("00001", 1U),
                                          std::make_pair("10000", 10000U),
                                          std::make_pair("65535", 65535U)));

TEST_P(FlagFileCrawlerConvertFromStringToInstanceIdParamaterisedFixture,
       ReturnsInstanceIdWhenPassingValidStringContainingInstanceId)
{
    const auto [instance_id_string, expected_instance_id] = GetParam();

    // When calling ConvertFromStringToInstanceId with a string containing an instance id
    const auto instance_id_result = FlagFileCrawler::ConvertFromStringToInstanceId(instance_id_string);

    // Then the parsed instance ID is returned
    ASSERT_TRUE(instance_id_result.has_value());
    EXPECT_EQ(instance_id_result, expected_instance_id);
}

TEST(FlagFileCrawlerConvertFromStringToInstanceIdTest, ReturnsErrorWhenPassingEmptyString)
{
    // When calling ConvertFromStringToInstanceId with an empty string
    const std::string empty_instance_string{};
    const auto instance_id_result = FlagFileCrawler::ConvertFromStringToInstanceId(empty_instance_string);

    // Then an error is returned
    ASSERT_FALSE(instance_id_result.has_value());
}

TEST(FlagFileCrawlerConvertFromStringToInstanceIdTest, ReturnsErrorWhenPassingStringContainingLetter)
{
    // When calling ConvertFromStringToInstanceId with a string containing a letter
    const std::string instance_string_containing_letter{"a"};
    const auto instance_id_result = FlagFileCrawler::ConvertFromStringToInstanceId(instance_string_containing_letter);

    // Then an error is returned
    ASSERT_FALSE(instance_id_result.has_value());
}

class FlagFileCrawlerParseQualityTypeFromStringParamaterisedFixture
    : public ::testing::TestWithParam<std::pair<std::string, QualityType>>
{
};
INSTANTIATE_TEST_CASE_P(FlagFileCrawlerParseQualityTypeFromStringParamaterisedFixture,
                        FlagFileCrawlerParseQualityTypeFromStringParamaterisedFixture,
                        ::testing::Values(std::make_pair("asil-qm", QualityType::kASIL_QM),
                                          std::make_pair("00000-asil-qm", QualityType::kASIL_QM),
                                          std::make_pair("asil-b", QualityType::kASIL_B),
                                          std::make_pair("00000-asil-b", QualityType::kASIL_B),
                                          std::make_pair("", QualityType::kInvalid),
                                          std::make_pair("00000", QualityType::kInvalid)));

TEST_P(FlagFileCrawlerParseQualityTypeFromStringParamaterisedFixture, ReturnsExpectedQualityType)
{
    const auto [quality_type_string, expected_quality_type] = GetParam();

    // When calling ParseQualityTypeFromString
    const auto quality_type = FlagFileCrawler::ParseQualityTypeFromString(quality_type_string);

    // Then the resulting quality type should be correct
    EXPECT_EQ(quality_type, expected_quality_type);
}

}  // namespace score::mw::com::impl::lola
