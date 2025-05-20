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

#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include "score/os/stat.h"

#include <amp_string_view.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <thread>

namespace bmw::mw::com::test
{

    namespace
    {

#if defined(__QNXNTO__)
        constexpr auto kSharedMemoryPathPrefix = "/dev/shmem/";
#else
        constexpr auto kSharedMemoryPathPrefix = "/dev/shm/";
#endif

        bool DoesFileExist(const amp::string_view file_path) noexcept
        {
            ::bmw::os::StatBuffer buffer{};
            const auto result = ::bmw::os::Stat::instance().stat(file_path.data(), buffer);
            if (!result.has_value())
            {
                if (result.error() != os::Error::Code::kNoSuchFileOrDirectory)
                {
                    // Unexpected error, perform respective logging, but our decision does not change. File does not exist
                    std::stringstream ss;
                    ss << "Querying attributes for file " << file_path.data() << "failed with errno" << result.error();
                    std::cout << ss.str() << std::endl;
                }
            }
            return result.has_value();
        }

    } // namespace

    namespace detail_shared_memory_object_creator
    {

        std::string CreateLockFilePath(const std::string &shared_memory_file_name) noexcept
        {
            return kSharedMemoryPathPrefix + shared_memory_file_name + "-lock";
        }

        bool WaitForFreeLockFile(const std::string &lock_file_path) noexcept
        {
            constexpr std::chrono::milliseconds timeout{500};
            constexpr std::chrono::milliseconds retryAfter{10};
            constexpr std::size_t maxRetryCount = timeout / retryAfter;

            std::uint8_t retryCount{0U};
            static_assert(maxRetryCount <= std::numeric_limits<decltype(retryCount)>::max(),
                          "Counter `retryCount` cannot hold maxRetryCount.");

            bool lockFileExists = DoesFileExist(lock_file_path.c_str());
            while (lockFileExists && (retryCount < maxRetryCount))
            {
                lockFileExists = DoesFileExist(lock_file_path.c_str());
                retryCount++;
                std::this_thread::sleep_for(retryAfter);
            }
            return !lockFileExists; // we want to return true if lock file does no longer exist, otherwise false.
        }

    } // namespace detail_shared_memory_object_creator

} // namespace bmw::mw::com::test
