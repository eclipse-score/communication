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
#include "score/memory/shared/shared_memory_factory.h"

#include <score/stop_token.hpp>

#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

#include <unistd.h>

namespace
{

const char kPermissionCheckPath[] = "/permission_check_test";
const std::size_t kPermissionCheckSize = 1024U;

void print_usage(const char* program)
{
    std::cout << "Usage: " << program
              << " [option] [uid]...\n"
                 "Option is a single one from the following:\n"
                 "  -h, Print this message and exit.\n"
                 "  -s, Run a server (shared memory creator), possibly with empty ACL\n"
                 "  -t, Run a client (shared memory accessor), expect success\n"
                 "  -f, Run a client (shared memory accessor), expect failure\n"
                 "If no option is given, run a server with a non-empty ACL. At least one UID must be provided.\n"
                 "If a client is running, the UIDs specify an (optional) ACL of allowed providers."
              << std::endl;
}

int run_client(const bool expect_success, const std::vector<uid_t>& uids)
{
    std::cout << "run_client(uid=" << getuid() << ", gid=" << getgid() << ", expect=" << expect_success << "):";
    for (const auto& user_identifier : uids)
    {
        std::cout << " " << user_identifier;
    }
    std::cout << std::endl;

    using namespace score::memory::shared;
    std::optional<score::cpp::span<const uid_t>> uid_param{};
    if (!uids.empty())
    {
        uid_param = uids;
    }
    auto resource = SharedMemoryFactory::Open(kPermissionCheckPath, false, uid_param);

    std::cout << "result: " << bool(resource) << std::endl;

    return bool(resource) == expect_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

score::cpp::stop_source stop_test{score::cpp::nostopstate_t{}};

void sigterm_handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT)
    {
        std::cout << "Stop requested" << std::endl;
        score::cpp::ignore = stop_test.request_stop();
    }
}

int run_server(const std::vector<uid_t>& uids)
{
    std::cout << "run_server:";
    for (const auto& user_identifier : uids)
    {
        std::cout << " " << user_identifier;
    }
    std::cout << std::endl;

    stop_test = score::cpp::stop_source{};
    const auto stop_token{stop_test.get_token()};

    if (std::signal(SIGTERM, sigterm_handler) == SIG_ERR || std::signal(SIGINT, sigterm_handler) == SIG_ERR)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing" << std::endl;
    }

    using namespace score::memory::shared;

    SharedMemoryFactory::UserPermissionsMap permissions{};
    for (const auto& user_identifier : uids)
    {
        permissions[score::os::Acl::Permission::kRead].push_back(user_identifier);
    }

    auto resource = SharedMemoryFactory::Create(
        kPermissionCheckPath,
        [](auto&&) {},
        kPermissionCheckSize,
        permissions.empty() ? SharedMemoryFactory::WorldReadable{} : SharedMemoryFactory::UserPermissions{permissions});

    if (!bool(resource))
    {
        std::cerr << "failure\n";
        return EXIT_FAILURE;
    }

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    SharedMemoryFactory::Remove(kPermissionCheckPath);

    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* const* argv)
{
    bool client_expect_success = false;
    bool client_expect_failure = false;
    std::int32_t option = 0;
    std::int32_t options_given = 0;
    while ((option = getopt(argc, argv, "hstf")) != -1)
    {
        switch (option)
        {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;

            case 's':
                // server (by default)
                ++options_given;
                break;

            case 't':
                // client
                client_expect_success = true;
                ++options_given;
                break;

            case 'f':
                // client
                client_expect_failure = true;
                ++options_given;
                break;

            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    if (options_given > 1 || (options_given == 0 && optind == argc))
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    std::vector<uid_t> uids;
    for (int index = optind; index < argc; ++index)
    {
        uids.push_back(static_cast<uid_t>(std::atoi(argv[index])));
    }
    if (client_expect_success || client_expect_failure)
    {
        return run_client(client_expect_success, uids);
    }
    return run_server(uids);
}
