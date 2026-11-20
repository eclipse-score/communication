/********************************************************************************
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
 ********************************************************************************/

// Regression test for https://github.com/eclipse-score/communication/issues/1064
//
// A process which offers a service (e.g. the Launch Manager) typically spawns child processes with fork() + execve().
// Every file descriptor opened by the mw::com runtime must therefore be created with close-on-exec, otherwise it
// leaks into the child (sockets, pipes, inotify instances and flock()ed lock files in particular).
//
// Each scenario is its own test target and is selected with the second argument:
//   service_offered        offers a service
//   proxy_subscribed       offers a service, creates a proxy for it in the same process and subscribes to its event
//   service_offered_again  offers a service, stops offering it and offers it again
// At the end of a scenario the test
//   1. checks FD_CLOEXEC on every file descriptor which was opened since the start of main(), and
//   2. spawns a child via popen() (fork + execve) which lists the file descriptors it inherited.
//
// This is intentionally a plain executable (no gtest) so that it does not open file descriptors of its own after the
// baseline snapshot has been taken.

#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/test/close_on_exec/test_event_datatype.h"
#include "score/mw/com/test/close_on_exec/test_parameters.h"
#include "score/mw/com/types.h"

#include <fcntl.h>
#include <unistd.h>

#include <charconv>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace
{

using score::mw::com::test::CloseOnExecProxy;
using score::mw::com::test::CloseOnExecSkeleton;

/// \brief Returns all open file descriptors of this process together with a description of what they refer to.
///
/// All possible descriptor numbers are probed with fcntl(), so that this neither depends on /proc nor opens a
/// descriptor itself (e.g. for iterating a directory).
std::map<int, std::string> GetOpenFileDescriptors()
{
    std::map<int, std::string> result{};
    const auto table_size = ::sysconf(_SC_OPEN_MAX);
    const int max_descriptor =
        static_cast<int>(table_size > 0 ? std::min<decltype(table_size)>(table_size, 65536) : 1024);
    for (int descriptor = 0; descriptor < max_descriptor; ++descriptor)
    {
        if (::fcntl(descriptor, F_GETFD) == -1)
        {
            continue;
        }
        std::error_code error_code{};
        const auto target = std::filesystem::read_symlink("/proc/self/fd/" + std::to_string(descriptor), error_code);
        result[descriptor] = error_code ? "<unknown>" : target.string();
    }
    return result;
}

/// \brief Child mode: print every inherited file descriptor except stdin / stdout / stderr.
int ListInheritedFileDescriptors()
{
    for (const auto& [descriptor, target] : GetOpenFileDescriptors())
    {
        if (descriptor > STDERR_FILENO)
        {
            std::cout << descriptor << ' ' << target << '\n';
        }
    }
    std::cout << score::mw::com::test::kListFdsEndMarker << '\n';
    return EXIT_SUCCESS;
}

std::string GetOwnExecutablePath()
{
    std::error_code error_code{};
    return std::filesystem::read_symlink("/proc/self/exe", error_code).string();
}

/// \brief Removes the generated configuration file when it goes out of scope, also on early returns.
class ScopedFile
{
  public:
    explicit ScopedFile(std::string path) : path_{std::move(path)} {}
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
    ~ScopedFile()
    {
        std::error_code error_code{};
        std::filesystem::remove(path_, error_code);
    }

    const std::string& path() const
    {
        return path_;
    }

  private:
    std::string path_;
};

/// \brief Writes a copy of the configuration in which the placeholder uid is replaced by the uid of this process.
///
/// A proxy may only open the shared memory of a provider whose uid is listed in "allowedProvider", so the uid the test
/// runs with has to be part of the configuration.
std::string WriteConfigurationForCurrentUser(const std::string& template_path)
{
    std::ifstream input{template_path};
    std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    for (auto position = content.find(score::mw::com::test::kUidPlaceholder); position != std::string::npos;
         position = content.find(score::mw::com::test::kUidPlaceholder, position))
    {
        const auto uid = std::to_string(::getuid());
        content.replace(position, std::string{score::mw::com::test::kUidPlaceholder}.size(), uid);
        position += uid.size();
    }

    const auto output_path =
        (std::filesystem::temp_directory_path() / ("close_on_exec_config_" + std::to_string(::getpid()) + ".json"))
            .string();
    std::ofstream{output_path} << content;
    return output_path;
}

/// \brief A descriptor belongs to the baseline only if both its number and the object it refers to are unchanged.
///
/// Comparing the number alone would hide a runtime descriptor that reuses the number of a closed baseline descriptor.
bool IsBaselineDescriptor(const std::map<int, std::string>& baseline, const int descriptor, const std::string& target)
{
    const auto it = baseline.find(descriptor);
    return (it != baseline.end()) && (it->second == target);
}

/// \brief Reports (and counts) all descriptors opened since \p baseline which would leak into a child process.
std::size_t CheckNoLeaks(const std::string& phase, const std::map<int, std::string>& baseline)
{
    std::size_t failures{0U};

    std::cout << "[" << phase << "] file descriptors opened by the runtime:\n";
    for (const auto& [descriptor, target] : GetOpenFileDescriptors())
    {
        if (descriptor <= STDERR_FILENO || IsBaselineDescriptor(baseline, descriptor, target))
        {
            continue;
        }
        const int flags = ::fcntl(descriptor, F_GETFD);
        const bool close_on_exec = (flags != -1) && ((flags & FD_CLOEXEC) != 0);
        std::cout << "  fd " << descriptor << "  FD_CLOEXEC=" << (close_on_exec ? "yes" : "NO ") << "  -> " << target
                  << '\n';
        if (!close_on_exec)
        {
            ++failures;
        }
    }

    std::cout << "[" << phase << "] file descriptors inherited by a child process (fork + execve):\n";
    const std::string command = GetOwnExecutablePath() + " " + score::mw::com::test::kListFdsArgument;
    std::FILE* const child_output = ::popen(command.c_str(), "r");
    if (child_output == nullptr)
    {
        std::cerr << "popen failed\n";
        return failures + 1U;
    }
    bool end_marker_seen{false};
    std::array<char, 1024> line{};
    while (std::fgets(line.data(), static_cast<int>(line.size()), child_output) != nullptr)
    {
        std::string child_line{line.data()};
        while (!child_line.empty() && (child_line.back() == '\n'))
        {
            child_line.pop_back();
        }
        if (child_line == score::mw::com::test::kListFdsEndMarker)
        {
            end_marker_seen = true;
            continue;
        }
        const auto separator = child_line.find(' ');
        int child_descriptor{-1};
        static_cast<void>(std::from_chars(child_line.data(), child_line.data() + child_line.size(), child_descriptor));
        const std::string child_target = (separator == std::string::npos) ? "" : child_line.substr(separator + 1U);
        if (IsBaselineDescriptor(baseline, child_descriptor, child_target))
        {
            continue;
        }
        std::cout << "  LEAKED: " << child_line << '\n';
        ++failures;
    }
    const int child_status = ::pclose(child_output);
    if ((child_status != 0) || !end_marker_seen)
    {
        std::cout << "  The child did not list its descriptors (exit status " << child_status << ")\n";
        ++failures;
    }
    return failures;
}

}  // namespace

int main(int argc, const char** argv)
{
    namespace test = score::mw::com::test;

    if (argc == 2 && std::string{argv[1]} == test::kListFdsArgument)
    {
        return ListInheritedFileDescriptors();
    }
    const std::string scenario{(argc == 3) ? argv[2] : ""};
    if ((scenario != test::kScenarioServiceOffered) && (scenario != test::kScenarioProxySubscribed) &&
        (scenario != test::kScenarioServiceOfferedAgain))
    {
        std::cerr << "usage: " << argv[0] << " <path to mw_com_config.json> <" << test::kScenarioServiceOffered << '|'
                  << test::kScenarioProxySubscribed << '|' << test::kScenarioServiceOfferedAgain << ">\n";
        return EXIT_FAILURE;
    }

    const ScopedFile configuration_file{WriteConfigurationForCurrentUser(argv[1])};

    // Everything which is open before the runtime is touched (e.g. passed in by the test harness) is not our concern.
    const std::map<int, std::string> baseline = GetOpenFileDescriptors();

    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{configuration_file.path()});

    const auto instance_specifier =
        score::mw::com::InstanceSpecifier::Create(std::string{test::kInstanceSpecifier}).value();

    // Every scenario starts by offering the service.
    auto skeleton_result = CloseOnExecSkeleton::Create(instance_specifier);
    if (!skeleton_result.has_value())
    {
        std::cerr << "Could not create skeleton: " << skeleton_result.error() << '\n';
        return EXIT_FAILURE;
    }
    auto skeleton = std::move(skeleton_result).value();
    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Could not offer service: " << offer_result.error() << '\n';
        return EXIT_FAILURE;
    }

    // Kept alive until the check at the end of the scenario.
    std::optional<CloseOnExecProxy> proxy{};

    if (scenario == test::kScenarioProxySubscribed)
    {
        // Connect a proxy (in the same process) and subscribe to the event.
        auto handles_result = CloseOnExecProxy::FindService(instance_specifier);
        if (!handles_result.has_value() || handles_result.value().empty())
        {
            std::cerr << "Could not find the offered service\n";
            return EXIT_FAILURE;
        }
        auto proxy_result = CloseOnExecProxy::Create(handles_result.value().front());
        if (!proxy_result.has_value())
        {
            std::cerr << "Could not create proxy: " << proxy_result.error() << '\n';
            return EXIT_FAILURE;
        }
        proxy.emplace(std::move(proxy_result).value());
        const auto subscribe_result = proxy->test_event_.Subscribe(1U);
        if (!subscribe_result.has_value())
        {
            std::cerr << "Could not subscribe: " << subscribe_result.error() << '\n';
            return EXIT_FAILURE;
        }
        // Subscribe() completes synchronously today. If it ever becomes asynchronous, this check fails instead of the
        // test silently inspecting the descriptors before the subscription has finished.
        if (proxy->test_event_.GetSubscriptionState() != score::mw::com::SubscriptionState::kSubscribed)
        {
            std::cerr << "The event is not in the subscribed state after Subscribe()\n";
            return EXIT_FAILURE;
        }
    }
    else if (scenario == test::kScenarioServiceOfferedAgain)
    {
        // Stop offering and offer the service again, which creates its marker files and shared memory anew.
        skeleton.StopOfferService();
        const auto reoffer_result = skeleton.OfferService();
        if (!reoffer_result.has_value())
        {
            std::cerr << "Could not offer service again: " << reoffer_result.error() << '\n';
            return EXIT_FAILURE;
        }
    }

    const std::size_t failures = CheckNoLeaks(scenario, baseline);
    if (failures != 0U)
    {
        std::cerr << "FAILED: " << failures << " close-on-exec violation(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASSED: no file descriptor leaks into child processes\n";
    return EXIT_SUCCESS;
}
