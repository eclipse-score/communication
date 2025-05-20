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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_CREATOR_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_CREATOR_H

#include "score/memory/shared/lock_file.h"
#include "score/os/errno.h"
#include "score/os/fcntl.h"
#include "score/os/mman.h"
#include "score/os/stat.h"
#include "score/os/unistd.h"

#include <amp_expected.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace bmw::mw::com::test
{

    namespace detail_shared_memory_object_creator
    {

        std::string CreateLockFilePath(const std::string &shared_memory_file_name) noexcept;
        bool WaitForFreeLockFile(const std::string &lock_file_path) noexcept;

    } // namespace detail_shared_memory_object_creator

    template <typename T>
    class SharedMemoryObjectCreator
    {
    public:
        template <typename... Args>
        static os::Result<SharedMemoryObjectCreator> CreateObject(const std::string &shared_memory_file_name,
                                                                  Args &&...args) noexcept;
        template <typename... Args>
        static os::Result<SharedMemoryObjectCreator> CreateOrOpenObject(const std::string &shared_memory_file_name,
                                                                        Args &&...args) noexcept;

        static os::Result<SharedMemoryObjectCreator> OpenObject(const std::string &shared_memory_file_name) noexcept;

        ~SharedMemoryObjectCreator() noexcept = default;
        SharedMemoryObjectCreator(const SharedMemoryObjectCreator &) = delete;
        SharedMemoryObjectCreator &operator=(const SharedMemoryObjectCreator &) = delete;
        SharedMemoryObjectCreator(SharedMemoryObjectCreator &&other) noexcept = default;
        SharedMemoryObjectCreator &operator=(SharedMemoryObjectCreator &&other) noexcept = default;

        T &GetObject() noexcept { return *object_address_; }

        void CleanUp() noexcept;

    private:
        SharedMemoryObjectCreator(std::string path,
                                  std::int32_t file_descriptor,
                                  T *object_address,
                                  bool created_file) noexcept
            : path_{std::move(path)},
              object_address_{object_address},
              file_descriptor_{file_descriptor},
              created_file_{created_file}
        {
        }

        std::string path_;
        T *object_address_;
        std::int32_t file_descriptor_;

        bool created_file_;
    };

    template <typename T>
    template <typename... Args>
    os::Result<SharedMemoryObjectCreator<T>> SharedMemoryObjectCreator<T>::CreateObject(
        const std::string &shared_memory_file_name,
        Args &&...args) noexcept
    {
        const auto lock_file_path = detail_shared_memory_object_creator::CreateLockFilePath(shared_memory_file_name);
        const auto lock_file_result = memory::shared::LockFile::Create(lock_file_path);
        if (!lock_file_result.has_value())
        {
            std::stringstream ss;
            ss << "SharedMemoryObjectCreator: Could not create lock file (" << lock_file_path << ")";
            std::cout << ss.str() << std::endl;
            return amp::make_unexpected(os::Error::createFromErrno(EAGAIN));
        }

        const auto open_result = ::bmw::os::Mman::instance().shm_open(
            shared_memory_file_name.data(),
            ::bmw::os::Fcntl::Open::kCreate | ::bmw::os::Fcntl::Open::kReadWrite | ::bmw::os::Fcntl::Open::kExclusive,
            bmw::os::Stat::Mode::kReadWriteExecUser);
        if (!open_result.has_value())
        {
            return amp::make_unexpected(open_result.error());
        }
        const auto file_descriptor = open_result.value();

        const auto truncation_result = ::bmw::os::Unistd::instance().ftruncate(file_descriptor, sizeof(T));
        if (!truncation_result.has_value())
        {
            std::stringstream ss;
            ss << "SharedMemoryObjectCreator: Could not ftruncate file to size " << sizeof(T) << " for file "
               << shared_memory_file_name << " with error " << truncation_result.error();
            std::cerr << ss.str() << std::endl;
            return amp::make_unexpected(truncation_result.error());
        }

        const auto mmap_result =
            ::bmw::os::Mman::instance().mmap(NULL,
                                             sizeof(T),
                                             ::bmw::os::Mman::Protection::kRead | ::bmw::os::Mman::Protection::kWrite,
                                             ::bmw::os::Mman::Map::kShared,
                                             file_descriptor,
                                             0);
        if (!mmap_result.has_value())
        {
            std::stringstream ss;
            ss << "SharedMemoryObjectCreator: Unexpected error while mapping memory into process "
               << shared_memory_file_name << " with errno" << mmap_result.error();
            std::cerr << ss.str() << std::endl;
            return amp::make_unexpected(mmap_result.error());
        }
        auto *const object_address = static_cast<T *>(mmap_result.value());
        amp::ignore = new (object_address) T(std::forward<Args>(args)...);

        const bool created_file{true};
        return SharedMemoryObjectCreator{shared_memory_file_name, file_descriptor, object_address, created_file};
    }

    template <typename T>
    os::Result<SharedMemoryObjectCreator<T>> SharedMemoryObjectCreator<T>::OpenObject(
        const std::string &shared_memory_file_name) noexcept
    {
        const auto lock_file_path = detail_shared_memory_object_creator::CreateLockFilePath(shared_memory_file_name);
        if (!detail_shared_memory_object_creator::WaitForFreeLockFile(lock_file_path))
        {
            std::stringstream ss;
            ss << "SharedMemoryObjectCreator: Lock file at (" << lock_file_path
               << ") still present after timeout. Exiting.";
            std::cout << ss.str() << std::endl;
            return amp::make_unexpected<os::Error>(os::Error::createFromErrno(EBUSY));
        }

        const auto open_result = ::bmw::os::Mman::instance().shm_open(
            shared_memory_file_name.data(), ::bmw::os::Fcntl::Open::kReadWrite, bmw::os::Stat::Mode::kReadWriteExecUser);
        if (!open_result.has_value())
        {
            return amp::make_unexpected(open_result.error());
        }
        const auto file_descriptor = open_result.value();

        const auto mmap_result =
            ::bmw::os::Mman::instance().mmap(NULL,
                                             sizeof(T),
                                             ::bmw::os::Mman::Protection::kRead | ::bmw::os::Mman::Protection::kWrite,
                                             ::bmw::os::Mman::Map::kShared,
                                             file_descriptor,
                                             0);
        if (!mmap_result.has_value())
        {
            std::stringstream ss;
            ss << "SharedMemoryObjectCreator: Unexpected error while mapping memory into process "
               << shared_memory_file_name << " with errno " << mmap_result.error().ToString();
            std::cerr << ss.str() << std::endl;
            return amp::make_unexpected(mmap_result.error());
        }
        auto *const object_address = static_cast<T *>(mmap_result.value());

        const bool created_file{false};
        return SharedMemoryObjectCreator{shared_memory_file_name, file_descriptor, object_address, created_file};
    }

    template <typename T>
    template <typename... Args>
    os::Result<SharedMemoryObjectCreator<T>> SharedMemoryObjectCreator<T>::CreateOrOpenObject(
        const std::string &shared_memory_file_name,
        Args &&...args) noexcept
    {
        auto open_object_result = OpenObject(shared_memory_file_name);
        if (open_object_result.has_value())
        {
            return open_object_result;
        }

        if (open_object_result.error() == os::Error::Code::kNoSuchFileOrDirectory)
        {
            auto create_object_result = CreateObject(shared_memory_file_name, std::forward<Args>(args)...);
            if (create_object_result.has_value())
            {
                return create_object_result;
            }

            // In case the object was created by another SharedMemoryObjectCreator during the CreateObject call, we try
            // to open it again here.
            return OpenObject(shared_memory_file_name);
        }
        else
        {
            std::stringstream ss{};
            ss << "SharedMemoryObjectCreator: Unexpected error while opening object with SharedMemoryObjectCreator at path"
               << shared_memory_file_name << "with errno" << open_object_result.error();
            std::cerr << ss.str() << std::endl;
            std::terminate();
        }
    }

    template <typename T>
    void SharedMemoryObjectCreator<T>::CleanUp() noexcept
    {
        ::bmw::os::Mman::instance().munmap(object_address_, sizeof(T));
        const auto &unistd = ::bmw::os::Unistd::instance();
        unistd.close(file_descriptor_);
        if (created_file_)
        {
            ::bmw::os::Mman::instance().shm_unlink(path_.data());
            unistd.unlink(path_.data());
        }
    }

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_CREATOR_H
