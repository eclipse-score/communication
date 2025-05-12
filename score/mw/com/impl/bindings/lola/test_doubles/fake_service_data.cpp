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
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_service_data.h"
#include "score/filesystem/path.h"
#include "score/memory/shared/shared_memory_resource.h"
#include <memory>

namespace score::mw::com::impl::lola
{

using ::score::memory::shared::SharedMemoryResource;

std::unique_ptr<FakeServiceData> FakeServiceData::Create(const std::string& control_file_name,
                                                         const std::string& data_file_name,
                                                         const std::string& usage_marker_file,
                                                         const pid_t skeleton_process_pid_in,
                                                         bool initialise_skeleton_data) noexcept
{
    score::filesystem::Path path{usage_marker_file};
    const auto lola_tmp_folder = path.RemoveFilename();
    auto filesystem = filesystem::FilesystemFactory{}.CreateInstance();
    constexpr auto permissions{os::Stat::Mode::kReadWriteExecUser | os::Stat::Mode::kReadWriteExecGroup |
                               os::Stat::Mode::kReadWriteExecOthers};
    const auto create_dir_result = filesystem.utils->CreateDirectories(lola_tmp_folder, permissions);
    if (!create_dir_result.has_value())
    {
        std::cerr << "Failed to create: " << lola_tmp_folder.Native() << std::endl;
        return {};
    }

    auto marker_file = memory::shared::LockFile::Create(usage_marker_file);
    if (!marker_file.has_value())
    {
        std::cerr << "Could not create or open marker file: " << usage_marker_file;
        return {};
    }

    return std::make_unique<FakeServiceData>(control_file_name,
                                             data_file_name,
                                             std::move(marker_file.value()),
                                             skeleton_process_pid_in,
                                             initialise_skeleton_data);
}

FakeServiceData::FakeServiceData(const std::string& control_file_name,
                                 const std::string& data_file_name,
                                 memory::shared::LockFile service_instance_usage_marker_file_in,
                                 const pid_t skeleton_process_pid_in,
                                 bool initialise_skeleton_data) noexcept
    : control_path{control_file_name},
      data_path{data_file_name},
      filesystem{filesystem::FilesystemFactory{}.CreateInstance()},
      service_instance_usage_marker_file{std::move(service_instance_usage_marker_file_in)}
{
    score::memory::shared::SharedMemoryFactory::Remove(control_path);
    score::memory::shared::SharedMemoryFactory::Remove(data_path);

    control_memory = score::memory::shared::SharedMemoryFactory::Create(
        control_file_name,
        [this, initialise_skeleton_data](std::shared_ptr<SharedMemoryResource> memory_resource) {
            if (initialise_skeleton_data)
            {
                data_control =
                    memory_resource->construct<ServiceDataControl>(memory_resource->getMemoryResourceProxy());
            }
        },
        65535U);

    data_memory = score::memory::shared::SharedMemoryFactory::Create(
        data_file_name,
        [this, initialise_skeleton_data, skeleton_process_pid_in](
            std::shared_ptr<SharedMemoryResource> memory_resource) {
            if (initialise_skeleton_data)
            {
                data_storage =
                    memory_resource->construct<ServiceDataStorage>(memory_resource->getMemoryResourceProxy());
                data_storage->skeleton_pid_ = skeleton_process_pid_in;
            }
        },
        65535U);
}

FakeServiceData::~FakeServiceData() noexcept
{
    score::memory::shared::SharedMemoryFactory::Remove(control_path);
    score::memory::shared::SharedMemoryFactory::Remove(data_path);

    const auto remove_result = filesystem.standard->Remove(lola_tmp_folder);
    if (!remove_result.has_value())
    {
        std::cerr << "Failed to remove: " << lola_tmp_folder.Native() << std::endl;
        return;
    }
}

}  // namespace score::mw::com::impl::lola
