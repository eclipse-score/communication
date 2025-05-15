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
#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file.h"

#include "score/filesystem/path.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/filesystem/factory/filesystem_factory_fake.h"
#include "score/os/unistd.h"

#include <gtest/gtest.h>
#include <string>

namespace score::mw::com::impl::lola
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::DoDefault;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Not;
using ::testing::Return;

InstanceSpecifier kInstanceSpecifier{InstanceSpecifier::Create("/bla/blub/specifier").value()};
LolaServiceTypeDeployment kServiceId{1U};
LolaServiceInstanceId kInstanceId1{1U};
ServiceTypeDeployment kServiceTypeDeployment{kServiceId};
ServiceInstanceDeployment kInstanceDeployment1{make_ServiceIdentifierType("/bla/blub/service1"),
                                               LolaServiceInstanceDeployment{kInstanceId1},
                                               QualityType::kASIL_QM,
                                               kInstanceSpecifier};
ServiceInstanceDeployment kInstanceDeployment2{make_ServiceIdentifierType("/bla/blub/service1"),
                                               LolaServiceInstanceDeployment{kInstanceId1},
                                               QualityType::kASIL_B,
                                               kInstanceSpecifier};

InstanceIdentifier kInstanceIdentifier1{make_InstanceIdentifier(kInstanceDeployment1, kServiceTypeDeployment)};
InstanceIdentifier kInstanceIdentifier2{make_InstanceIdentifier(kInstanceDeployment2, kServiceTypeDeployment)};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifier1{kInstanceIdentifier1};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifier2{kInstanceIdentifier2};

ConfigurationStore kConfigStoreInvalid1{kInstanceSpecifier,
                                        make_ServiceIdentifierType("/bla/blub/service1"),
                                        QualityType::kInvalid,
                                        kServiceId,
                                        LolaServiceInstanceDeployment{kInstanceId1}};

filesystem::Perms kAllPerms{filesystem::Perms::kReadWriteExecUser | filesystem::Perms::kReadWriteExecGroup |
                            filesystem::Perms::kReadWriteExecOthers};
filesystem::Perms kUserWriteRestRead{filesystem::Perms::kReadUser | filesystem::Perms::kWriteUser |
                                     filesystem::Perms::kReadGroup | filesystem::Perms::kReadOthers};

class FlagFileTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        filesystem::StandardFilesystem::set_testing_instance(*(filesystem_.standard));
    }

    void TearDown() override
    {
        filesystem::StandardFilesystem::restore_instance();
    }

    FlagFile::Disambiguator disambiguator_{0};
    filesystem::FilesystemFactoryFake filesystem_factory_fake_{};
    filesystem::Filesystem filesystem_{filesystem_factory_fake_.CreateInstance()};

    std::unique_ptr<os::Unistd> unistd_ = std::make_unique<os::internal::UnistdImpl>();
    pid_t pid_{unistd_->getpid()};

    filesystem::Path flag_file_path1 =
        test::GetServiceDiscoveryPath() / "1/1" / (std::to_string(pid_) + "_asil-qm_" + std::to_string(disambiguator_));
    filesystem::Path flag_file_path2 =
        test::GetServiceDiscoveryPath() / "1/1" / (std::to_string(pid_) + "_asil-b_" + std::to_string(disambiguator_));
};

TEST_F(FlagFileTest, FlagFileIsCreatedAtConstructionForAsilQm)
{
    InSequence in_sequence{};
    EXPECT_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(flag_file_path1.ParentPath(), kAllPerms));
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Permissions(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(filesystem_factory_fake_.GetStreams(), Open(flag_file_path1, std::ios_base::out));
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(),
                Permissions(flag_file_path1, kUserWriteRestRead, filesystem::PermOptions::kReplace));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    EXPECT_TRUE(flag_file.has_value());
}

TEST_F(FlagFileTest, FlagFileIsCreatedAtConstructionForAsilB)
{
    InSequence in_sequence{};
    EXPECT_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(flag_file_path2.ParentPath(), kAllPerms));
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Permissions(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(filesystem_factory_fake_.GetStreams(), Open(flag_file_path2, std::ios_base::out));
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(),
                Permissions(flag_file_path2, kUserWriteRestRead, filesystem::PermOptions::kReplace));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier2, disambiguator_, filesystem_);
    EXPECT_TRUE(flag_file.has_value());
}

TEST_F(FlagFileTest, ExistingMatchingFlagFileIsRemovedAtConstructionAsilQm)
{
    InSequence in_sequence{};

    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());
    filesystem_factory_fake_.GetStandard().CreateRegularFile(flag_file_path1, kUserWriteRestRead);
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path1));
    EXPECT_CALL(filesystem_factory_fake_.GetStreams(), Open(flag_file_path1, std::ios_base::out));

    // For destruction
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path1));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    EXPECT_TRUE(flag_file.has_value());
}

TEST_F(FlagFileTest, ExistingMatchingFlagFileIsRemovedAtConstructionAsilB)
{
    InSequence in_sequence{};

    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path2.ParentPath()).has_value());
    filesystem_factory_fake_.GetStandard().CreateRegularFile(flag_file_path2, kUserWriteRestRead);
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path2));
    EXPECT_CALL(filesystem_factory_fake_.GetStreams(), Open(flag_file_path2, std::ios_base::out));

    // For destruction
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path2));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier2, disambiguator_, filesystem_);
    EXPECT_TRUE(flag_file.has_value());
}

TEST_F(FlagFileTest, FailsToRemoveExistingMatchingFlagFileAtConstruction)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());
    ASSERT_TRUE(
        filesystem_factory_fake_.GetStandard().CreateRegularFile(flag_file_path1, kUserWriteRestRead).has_value());
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path1))
        .WillOnce(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotRemoveFileOrDirectory)));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_FALSE(flag_file.has_value());
    EXPECT_EQ(flag_file.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileTest, FlagFileConstructionCopesWithExistingPath)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    EXPECT_TRUE(flag_file.has_value());
}

TEST_F(FlagFileTest, FailsToCreateFlagFileAtConstruction)
{
    EXPECT_CALL(filesystem_factory_fake_.GetStreams(), Open(flag_file_path1, std::ios_base::out))
        .WillOnce(Return(
            ByMove(MakeUnexpected<std::unique_ptr<std::iostream>>(filesystem::ErrorCode::kCouldNotOpenFileStream))));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_FALSE(flag_file.has_value());
    EXPECT_EQ(flag_file.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileTest, FailsToSetPermissionsOnFlagFileAtConstruction)
{
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Permissions(Not(Eq(flag_file_path1)), _, _)).Times(AnyNumber());
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(),
                Permissions(flag_file_path1, kUserWriteRestRead, filesystem::PermOptions::kReplace))
        .WillOnce(Return(ByMove(MakeUnexpected(filesystem::ErrorCode::kCouldNotSetPermissions))));

    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_FALSE(flag_file.has_value());
    EXPECT_EQ(flag_file.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileTest, FlagFileIsNotRemovedWhenMoving)
{
    auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_TRUE(flag_file.has_value());
    const auto moved_to_flag_file = std::move(flag_file).value();
    score::cpp::ignore = moved_to_flag_file;

    // Will be called once for destructor of moved_to_flag_file
    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path1));
}

TEST_F(FlagFileTest, ExistsReturnsTrueIfFlagFileDoesExist)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());
    filesystem_factory_fake_.GetStandard().CreateRegularFile(flag_file_path1, kUserWriteRestRead);

    EXPECT_TRUE(FlagFile::Exists(kEnrichedInstanceIdentifier1));
}

TEST_F(FlagFileTest, ExistsReturnsFalseIfFlagFileDoesNotExist)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());

    EXPECT_FALSE(FlagFile::Exists(kEnrichedInstanceIdentifier1));
}

TEST_F(FlagFileTest, ExistsReturnsFalseIfFlagFileAndPathDoNotExist)
{
    EXPECT_FALSE(FlagFile::Exists(kEnrichedInstanceIdentifier1));
}

TEST_F(FlagFileTest, CreateSearchPathReturnsPathIfCreatedSuccessfully)
{
    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path.value(), flag_file_path1.ParentPath());
    EXPECT_TRUE(filesystem_.standard->Exists(flag_file_path1.ParentPath()).value());
}

TEST_F(FlagFileTest, CreateSearchPathReturnsPathIfAlreadyExists)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard()
                    .Permissions(flag_file_path1.ParentPath(), kAllPerms, filesystem::PermOptions::kReplace)
                    .has_value());

    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path.value(), flag_file_path1.ParentPath());
}

TEST_F(FlagFileTest, CreateSearchPathReturnsPathAndHealsPermissionsIfAlreadyExistsWithWrongPermissions)
{
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath()).has_value());
    ASSERT_TRUE(filesystem_factory_fake_.GetStandard()
                    .Permissions(flag_file_path1.ParentPath(), kUserWriteRestRead, filesystem::PermOptions::kReplace)
                    .has_value());

    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path.value(), flag_file_path1.ParentPath());
}

TEST_F(FlagFileTest, CreateSearchPathReturnsErrorIfCannotCreateDirectoryRepeatedly)
{
    ON_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(_, _))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotCreateDirectory)));

    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    ASSERT_FALSE(path.has_value());
    EXPECT_EQ(path.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileTest, CreateSearchPathReturnsPathIfItAppearsDuringBackoffTime)
{
    EXPECT_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(_, _))
        .WillOnce(InvokeWithoutArgs([this]() {
            filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath());
            filesystem_factory_fake_.GetStandard().Permissions(
                flag_file_path1.ParentPath(), kUserWriteRestRead, filesystem::PermOptions::kReplace);
            return MakeUnexpected(filesystem::ErrorCode::kCouldNotCreateDirectory);
        }))
        .WillRepeatedly(DoDefault());

    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path.value(), flag_file_path1.ParentPath());
}

TEST_F(FlagFileTest, FlagFileIsRemovedAtDestruction)
{
    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_TRUE(flag_file.has_value());

    EXPECT_CALL(filesystem_factory_fake_.GetStandard(), Remove(flag_file_path1));
}

TEST_F(FlagFileTest, FlagFileRetainsFlagFilePathAtDestruction)
{
    const auto flag_file = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    ASSERT_TRUE(flag_file.has_value());

    EXPECT_TRUE(filesystem_.standard->Exists(flag_file_path1.ParentPath()).has_value());
}

TEST_F(FlagFileTest, GivenFlagFileWithInvalidQualityWhenMakeThenCreatesFlagFilePath)
{
    // Given an invalid flag file
    // When Make is called
    const auto flag_file =
        FlagFile::Make(kConfigStoreInvalid1.GetEnrichedInstanceIdentifier(), disambiguator_, filesystem_);
    ASSERT_TRUE(flag_file.has_value());
    // Then flag file path is created
    EXPECT_TRUE(filesystem_.standard->Exists(flag_file_path1.ParentPath()).has_value());
}

TEST_F(FlagFileTest, GivenCreateDirectoryFailsRepeatedlyWhenMakeThenReturnsError)
{
    // Given CreateDirectories returns error
    ON_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(_, _))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotCreateDirectory)));

    // When Make is called
    const auto path = FlagFile::Make(kEnrichedInstanceIdentifier1, disambiguator_, filesystem_);
    // Then path has value
    ASSERT_FALSE(path.has_value());
    EXPECT_EQ(path.error(), ComErrc::kBindingFailure);
}

TEST_F(FlagFileTest, GivenFileRemoveFailWhenMakeThenWillDie)
{
    // Given Remove returns error
    ON_CALL(filesystem_factory_fake_.GetStandard(), Remove(_))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotRemoveFileOrDirectory)));

    // When Make is called
    // Then the program terminates
    EXPECT_DEATH(FlagFile::Make(kEnrichedInstanceIdentifier2, disambiguator_, filesystem_), ".*");
}

TEST_F(FlagFileTest, CreatePathReturnsValidPathWhenDirectoryAlreadyExists)
{
    // Given CreateDirectories returns error
    EXPECT_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(_, _))
        .WillOnce(InvokeWithoutArgs([this]() {
            filesystem_factory_fake_.GetStandard().CreateDirectories(flag_file_path1.ParentPath());
            filesystem_factory_fake_.GetStandard().Permissions(
                flag_file_path1.ParentPath(), kAllPerms, filesystem::PermOptions::kReplace);
            return MakeUnexpected(filesystem::ErrorCode::kCouldNotCreateDirectory);
        }))
        .WillRepeatedly(DoDefault());

    // When CreateSearchPath is called
    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    // Then a valid search path is created
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path.value(), flag_file_path1.ParentPath());
}

TEST_F(FlagFileTest, GivenStatusAndCreateDirFailWhenCreatePathThenPathHasNoValue)
{
    // Given Status and CreateDirectories return errors
    ON_CALL(filesystem_factory_fake_.GetStandard(), Status(_))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotRetrieveStatus)));
    ON_CALL(filesystem_factory_fake_.GetUtils(), CreateDirectories(_, _))
        .WillByDefault(Return(MakeUnexpected(filesystem::ErrorCode::kNotImplemented)));
    // When CreateSearchPath is called
    const auto path = FlagFile::CreateSearchPath(kEnrichedInstanceIdentifier1, filesystem_);
    // Then path has no value
    ASSERT_FALSE(path.has_value());
}

}  // namespace score::mw::com::impl::lola
