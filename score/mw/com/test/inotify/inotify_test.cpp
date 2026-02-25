/*******************************************************************************
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
 *******************************************************************************/

#include "score/filesystem/filesystem.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>

#if defined(__QNXNTO__)
const std::string kBaseFolder{"/tmp_discovery"};
#else
const std::string kBaseFolder{"/tmp"};
#endif
const std::string kTestDirName{"inotifyTest"};
const std::string kTestDir{kBaseFolder + "/" + kTestDirName};

namespace score::mw::com::test
{

namespace
{

void do_cleanup(const std::string& folder)
{
    score::filesystem::StandardFilesystem fs;
    const auto ret = fs.RemoveAll(folder);
    if (!ret.has_value())
    {
        std::cerr << "cleanup failed" << ret.error();
    }
}

std::string to_str(os::InotifyEvent::ReadMask mask)
{
    switch (mask)
    {
        case os::InotifyEvent::ReadMask::kInAccess:
            return "Access";

        case os::InotifyEvent::ReadMask::kInCreate:
            return "Create";

        case os::InotifyEvent::ReadMask::kInDelete:
            return "Delete";

        case os::InotifyEvent::ReadMask::kInMovedTo:
            return "InMovedTo";

        case os::InotifyEvent::ReadMask::kInIgnored:
            return "InIgnored";

        case os::InotifyEvent::ReadMask::kInIsDir:
            return "InIsDir";

        case os::InotifyEvent::ReadMask::kInQOverflow:
            return "Queue Overflow";

        case os::InotifyEvent::ReadMask::kUnknown:  // intentional
        default:
            return "Unknown";
    }

    return "";
}

int run_inotify_test()
{
    os::InotifyInstanceImpl i_notify{};
    std::cout << "Adding watch to folder:" << kBaseFolder << std::endl;
    const auto watch_descriptor =
        i_notify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
    if (!(watch_descriptor.has_value()))
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Add watch failed" << std::endl;
        return -1;
    }

    const std::string test_file_name{"C"};
    const std::string test_file{kBaseFolder + "/" + test_file_name};
    bool is_test_dir_create_event_received{false};
    bool is_test_file_create_event_received{false};
    bool is_test_file_delete_event_received{false};

    // spawn a thread and poll for events
    std::thread events_checker_thread([&]() {
        while (true)
        {
            std::cout << "Calling inotify Read" << std::endl;
            auto res = i_notify.Read();
            if (!(res.has_value()))
            {
                std::cerr << __FILE__ << ":" << __LINE__ << ":Failed to read:" << res.error() << std::endl;
                break;
            }

            for (auto& val : res.value())
            {
                const std::string name{val.GetName()};
                const auto mask{val.GetMask()};
                std::cout << "Received Event:" << name << ":" << to_str(mask) << std::endl;
                if (name == kTestDirName)
                {
                    is_test_dir_create_event_received = mask & os::InotifyEvent::ReadMask::kInCreate;
                }
                else if (name == test_file_name)
                {
                    if (mask & os::InotifyEvent::ReadMask::kInCreate)
                    {
                        is_test_file_create_event_received = true;
                    }
                    else if (mask & os::InotifyEvent::ReadMask::kInDelete)
                    {
                        is_test_file_delete_event_received = true;
                    }
                }
            }

            if (is_test_dir_create_event_received && is_test_file_create_event_received &&
                is_test_file_delete_event_received)
            {
                break;
            }
        }
    });

    score::filesystem::StandardFilesystem fs;
    const auto create_dir = fs.CreateDirectory(kTestDir);
    if (!create_dir.has_value())
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Create directory failed" << create_dir.error() << std::endl;
        return -1;
    }
    std::cout << "Created directory:" << kTestDir << std::endl;

    std::ofstream file{test_file};
    if (!file.good())
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Create file failed" << std::endl;
        return -1;
    }
    std::cout << "Created file: " << test_file << std::endl;

    const auto delete_file = fs.Remove(test_file);
    if (!delete_file.has_value())
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Remove file failed" << delete_file.error();
        return -1;
    }
    std::cout << "Deleted file: " << test_file << std::endl;

    std::cout << "waiting for events_checker_thread to receive and process the events." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // On QNX it takes around 251ms to receive the event.

    auto inotify_result = i_notify.RemoveWatch(watch_descriptor.value());
    if (!(inotify_result.has_value()))
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Remove watch failed" << std::endl;
        return -1;
    }
    std::cout << "removed watch" << std::endl;
    i_notify.Close();
    events_checker_thread.join();

    if (!is_test_dir_create_event_received)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Failed to receive create directory event" << std::endl;
        return -1;
    }
    if (!is_test_file_create_event_received)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Failed to receive create file event" << std::endl;
        return -1;
    }
    if (!is_test_file_delete_event_received)
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ":Failed to receive delete file event" << std::endl;
        return -1;
    }

    return 0;
}

}  // namespace

}  // namespace score::mw::com::test

int main()
{
    const auto ret = score::mw::com::test::run_inotify_test();
    std::cout << __FILE__ << ":" << __LINE__ << ":"
              << "doing cleanup" << std::endl;
    score::mw::com::test::do_cleanup(kTestDir);
    return ret;
}
