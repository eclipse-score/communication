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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_FIXTURES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_FIXTURES_H

#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/file_system_guard.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include "score/concurrency/long_running_threads_container.h"
#include "score/filesystem/details/standard_filesystem.h"
#include "score/filesystem/factory/filesystem_factory.h"
#include "score/filesystem/factory/filesystem_factory_fake.h"
#include "score/filesystem/file_status.h"
#include "score/filesystem/file_utils/file_utils_mock.h"
#include "score/filesystem/filestream/file_factory_fake.h"
#include "score/filesystem/filestream/file_factory_mock.h"
#include "score/filesystem/filesystem_struct.h"
#include "score/filesystem/path.h"
#include "score/filesystem/standard_filesystem_fake.h"
#include "score/os/unistd.h"
#include "score/os/utils/inotify/inotify_instance_facade.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"
#include "score/os/utils/inotify/inotify_instance_mock.h"

#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <cstdint>
#include <ios>
#include <list>
#include <memory>
#include <string>
#include <utility>

namespace score::mw::com::impl::lola::test
{

class ServiceDiscoveryClientFixture : public ::testing::Test
{
  public:
    static constexpr auto kAllPermissions = filesystem::Perms::kReadWriteExecUser |
                                            filesystem::Perms::kReadWriteExecGroup |
                                            filesystem::Perms::kReadWriteExecOthers;

    void SetUp() override
    {
        ASSERT_TRUE(inotify_instance_->IsValid());

        ON_CALL(inotify_instance_mock_, IsValid()).WillByDefault([this]() {
            return inotify_instance_->IsValid();
        });
        ON_CALL(inotify_instance_mock_, Close()).WillByDefault([this]() {
            return inotify_instance_->Close();
        });
        ON_CALL(inotify_instance_mock_, AddWatch(::testing::_, ::testing::_))
            .WillByDefault([this](auto path, auto mask) {
                return inotify_instance_->AddWatch(path, mask);
            });
        ON_CALL(inotify_instance_mock_, RemoveWatch(::testing::_)).WillByDefault([this](auto watch_descriptor) {
            return inotify_instance_->RemoveWatch(watch_descriptor);
        });
        ON_CALL(inotify_instance_mock_, Read()).WillByDefault([this]() {
            return inotify_instance_->Read();
        });
    }

    ServiceDiscoveryClientFixture& WhichContainsAServiceDiscoveryClient()
    {
        auto inotify_instance_facade = std::make_unique<os::InotifyInstanceFacade>(inotify_instance_mock_);
        auto unistd = std::make_unique<os::internal::UnistdImpl>();
        service_discovery_client_ = std::make_unique<ServiceDiscoveryClient>(
            long_running_threads_container_, std::move(inotify_instance_facade), std::move(unistd), filesystem_);
        return *this;
    }

    ServiceDiscoveryClientFixture& WithAnOfferedService(const InstanceIdentifier& instance_identifier)
    {
        EXPECT_TRUE(service_discovery_client_->OfferService(instance_identifier).has_value());
        return *this;
    }

    ServiceDiscoveryClientFixture& WithAnActiveStartFindService(
        const InstanceIdentifier& instance_identifier,
        const FindServiceHandle find_service_handle,
        FindServiceHandler<HandleType> find_service_handler = [](auto, auto) noexcept {})
    {
        EXPECT_TRUE(service_discovery_client_->StartFindService(
            find_service_handle, std::move(find_service_handler), EnrichedInstanceIdentifier{instance_identifier}));
        return *this;
    }

    filesystem::Path GetFlagFilePrefix(const LolaServiceId service_id,
                                       const LolaServiceInstanceId instance_id,
                                       const filesystem::Path& tmp_path) noexcept
    {
        const auto service_id_str = std::to_string(static_cast<std::uint32_t>(service_id));
        const auto instance_id_str = std::to_string(static_cast<std::uint32_t>(instance_id.GetId()));
        const auto pid = std::to_string(os::internal::UnistdImpl{}.getpid());
        return tmp_path / service_id_str / instance_id_str / pid;
    }

    void CreateRegularFile(filesystem::Filesystem& filesystem, const filesystem::Path& path) noexcept
    {
        ASSERT_TRUE(filesystem.utils->CreateDirectories(path.ParentPath(), kAllPermissions).has_value());
        ASSERT_TRUE(filesystem.streams->Open(path, std::ios_base::out).has_value());
    }

    filesystem::Filesystem filesystem_{filesystem::FilesystemFactory{}.CreateInstance()};
    FileSystemGuard filesystem_guard_{filesystem_, GetServiceDiscoveryPath()};

    std::unique_ptr<os::InotifyInstanceImpl> inotify_instance_{std::make_unique<os::InotifyInstanceImpl>()};
    ::testing::NiceMock<os::InotifyInstanceMock> inotify_instance_mock_{};

    concurrency::LongRunningThreadsContainer long_running_threads_container_{};
    std::unique_ptr<ServiceDiscoveryClient> service_discovery_client_{nullptr};
};

class ServiceDiscoveryClientWithFakeFileSystemFixture : public ::testing::Test
{
  public:
    static constexpr auto kAllPermissions = filesystem::Perms::kReadWriteExecUser |
                                            filesystem::Perms::kReadWriteExecGroup |
                                            filesystem::Perms::kReadWriteExecOthers;

    void SetUp() override
    {
        CreateFakeFilesystem();
    }

    void TearDown() override
    {
        filesystem::StandardFilesystem::restore_instance();
    }

    void CreateFakeFilesystem()
    {
        filesystem::FilesystemFactoryFake filesystem_factory_fake{};
        filesystem_mock_ = filesystem_factory_fake.CreateInstance();
        standard_filesystem_fake_ = &(filesystem_factory_fake.GetStandard());
        file_factory_mock_ = &(filesystem_factory_fake.GetStreams());
        file_utils_mock_ = &(filesystem_factory_fake.GetUtils());
        filesystem::StandardFilesystem::set_testing_instance(*standard_filesystem_fake_);
    }

    ServiceDiscoveryClientWithFakeFileSystemFixture& WhichContainsAServiceDiscoveryClient()
    {
        auto inotify_instance = std::make_unique<os::InotifyInstanceImpl>();
        EXPECT_TRUE(inotify_instance->IsValid());

        auto unistd = std::make_unique<os::internal::UnistdImpl>();
        service_discovery_client_ = std::make_unique<ServiceDiscoveryClient>(
            long_running_threads_container_, std::move(inotify_instance), std::move(unistd), filesystem_mock_);
        return *this;
    }

    ServiceDiscoveryClientWithFakeFileSystemFixture& ThatSavesTheFlagFilePath()
    {
        GetFlagFilePath(flag_file_path_);
        return *this;
    }

    ServiceDiscoveryClientWithFakeFileSystemFixture& WithAnOfferedService(const InstanceIdentifier& instance_identifier)
    {
        EXPECT_TRUE(service_discovery_client_->OfferService(instance_identifier).has_value());
        return *this;
    }

    void GetFlagFilePath(std::list<filesystem::Path>& flag_file_path) noexcept
    {
        GetFlagFilePath(flag_file_path, [](const auto&, const auto&) noexcept {});
    }

    template <typename Callable>
    void GetFlagFilePath(std::list<filesystem::Path>& flag_file_path, Callable callable) noexcept
    {
        ON_CALL(*file_factory_mock_, Open(::testing::_, ::testing::_))
            .WillByDefault([this, &flag_file_path, callable](const auto& path, const auto& mode) {
                flag_file_path.push_back(path);

                callable(path, mode);

                return filesystem::FileFactoryFake{*standard_filesystem_fake_}.Open(path, mode);
            });
    }

    void CreateRegularFile(filesystem::Filesystem& filesystem, const filesystem::Path& path) noexcept
    {
        ASSERT_TRUE(filesystem.utils->CreateDirectories(path.ParentPath(), kAllPermissions).has_value());
        ASSERT_TRUE(filesystem.streams->Open(path, std::ios_base::out).has_value());
    }

    std::list<filesystem::Path> flag_file_path_{};
    filesystem::Filesystem filesystem_mock_{};
    filesystem::StandardFilesystemFake* standard_filesystem_fake_{};
    filesystem::FileFactoryMock* file_factory_mock_{};
    filesystem::FileUtilsMock* file_utils_mock_{};

    concurrency::LongRunningThreadsContainer long_running_threads_container_{};
    std::unique_ptr<ServiceDiscoveryClient> service_discovery_client_{nullptr};
};

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_FIXTURES_H
