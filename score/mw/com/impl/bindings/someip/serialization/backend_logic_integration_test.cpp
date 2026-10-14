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

#include "score/mw/com/impl/bindings/someip/serialization/serializer_library.h"

#include "tools/cpp/runfiles/runfiles.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::impl::someip::serialization
{
namespace
{

using bazel::tools::cpp::runfiles::Runfiles;

using BackendProbeValueFn = std::uint32_t (*)() noexcept;

constexpr const char* kLifecycleFileEnv = "SERIALIZER_BACKEND_LIFECYCLE_FILE";
constexpr const char* kTestPluginRunfile =
    "score_communication/score/mw/com/impl/bindings/someip/serialization/backend_logic_test_plugin.so";

std::string GetTestPluginPath()
{
    std::string runfiles_error{};
    const std::unique_ptr<Runfiles> runfiles{Runfiles::CreateForTest(&runfiles_error)};
    EXPECT_NE(runfiles, nullptr) << runfiles_error;
    if (runfiles == nullptr)
    {
        return {};
    }

    const std::string plugin_path = runfiles->Rlocation(kTestPluginRunfile);
    EXPECT_FALSE(plugin_path.empty());
    return plugin_path;
}

std::vector<std::string> ReadLifecycleLog(const std::string& file_path)
{
    std::ifstream stream{file_path};
    std::vector<std::string> lines{};
    std::string line{};
    while (std::getline(stream, line))
    {
        lines.push_back(line);
    }
    return lines;
}

TEST(SerializerLibraryBackendIntegration, LoadsFixtureLibraryAndResolvesKnownSymbol)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    SerializerLibrary library{plugin_path};
    ASSERT_TRUE(library.IsOpen()) << library.ErrorMessage();

    const auto probe_value = library.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(probe_value, nullptr) << library.ErrorMessage();
    EXPECT_EQ(probe_value(), 0xC0FFEE42U);

    const auto missing = library.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
    EXPECT_FALSE(library.ErrorMessage().empty());
}

TEST(SerializerLibraryBackendIntegration, ReportsFailureForMissingLibrary)
{
    SerializerLibrary library{"/definitely/not/a/real/backend/library.so"};

    EXPECT_FALSE(library.IsOpen());
    EXPECT_FALSE(library.ErrorMessage().empty());
}

TEST(SerializerLibraryBackendIntegration, MissingSymbolReturnsNull)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    SerializerLibrary library{plugin_path};
    ASSERT_TRUE(library.IsOpen()) << library.ErrorMessage();

    const auto missing = library.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
    EXPECT_FALSE(library.ErrorMessage().empty());
}

TEST(SerializerLibraryBackendIntegration, CloseSucceedsAndSecondCloseFails)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    const std::string lifecycle_path = std::string{testing::TempDir()} + "serializer_backend_lifecycle.txt";
    std::remove(lifecycle_path.c_str());
    ASSERT_EQ(setenv(kLifecycleFileEnv, lifecycle_path.c_str(), 1), 0);

    {
        SerializerLibrary library{plugin_path};
        ASSERT_TRUE(library.IsOpen()) << library.ErrorMessage();
        EXPECT_TRUE(library.Close());
        EXPECT_FALSE(library.Close());
    }

    EXPECT_EQ(unsetenv(kLifecycleFileEnv), 0);
    EXPECT_EQ(ReadLifecycleLog(lifecycle_path), (std::vector<std::string>{"loaded", "unloaded"}));
}

}  // namespace
}  // namespace score::mw::com::impl::someip::serialization