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

#include "score/mw/com/impl/serialization/serialization_infrastructure.h"

#include "tools/cpp/runfiles/runfiles.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace score::mw::com::impl::serialization
{
namespace
{

using bazel::tools::cpp::runfiles::Runfiles;

using BackendProbeValueFn = std::uint32_t (*)() noexcept;

constexpr const char* kLifecycleFileEnv = "SERIALIZER_BACKEND_LIFECYCLE_FILE";
constexpr const char* kTestPluginRunfile =
    "score_communication/score/mw/com/impl/serialization/serialization_infrastructure_test_plugin.so";

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

TEST(SerializationInfrastructureBackendIntegration, LoadsFixtureLibraryAndResolvesKnownSymbol)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    SerializationInfrastructure infrastructure{plugin_path};
    ASSERT_TRUE(infrastructure.IsOpen()) << infrastructure.ErrorMessage();

    const auto probe_value = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(probe_value, nullptr) << infrastructure.ErrorMessage();
    EXPECT_EQ(probe_value(), 0xC0FFEE42U);

    const auto missing = infrastructure.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
    EXPECT_FALSE(infrastructure.ErrorMessage().empty());
}

TEST(SerializationInfrastructureBackendIntegration, CachesResolvedSymbolsForReuse)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    SerializationInfrastructure infrastructure{plugin_path};
    ASSERT_TRUE(infrastructure.IsOpen()) << infrastructure.ErrorMessage();

    const auto first = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(first, nullptr) << infrastructure.ErrorMessage();

    const auto second = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(second, nullptr) << infrastructure.ErrorMessage();
    EXPECT_EQ(first, second);
    EXPECT_EQ(second(), 0xC0FFEE42U);
}

TEST(SerializationInfrastructureBackendIntegration, ReportsFailureForMissingLibrary)
{
    SerializationInfrastructure infrastructure{"/definitely/not/a/real/backend/library.so"};

    EXPECT_FALSE(infrastructure.IsOpen());
    EXPECT_FALSE(infrastructure.ErrorMessage().empty());
}

TEST(SerializationInfrastructureBackendIntegration, MissingSymbolReturnsNull)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    SerializationInfrastructure infrastructure{plugin_path};
    ASSERT_TRUE(infrastructure.IsOpen()) << infrastructure.ErrorMessage();

    const auto missing = infrastructure.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
    EXPECT_FALSE(infrastructure.ErrorMessage().empty());
}

TEST(SerializationInfrastructureBackendIntegration, CloseSucceedsAndSecondCloseFails)
{
    const std::string plugin_path = GetTestPluginPath();
    ASSERT_FALSE(plugin_path.empty());

    const std::string lifecycle_path = std::string{testing::TempDir()} + "serializer_backend_lifecycle.txt";
    std::remove(lifecycle_path.c_str());
    ASSERT_EQ(setenv(kLifecycleFileEnv, lifecycle_path.c_str(), 1), 0);

    {
        SerializationInfrastructure infrastructure{plugin_path};
        ASSERT_TRUE(infrastructure.IsOpen()) << infrastructure.ErrorMessage();
        EXPECT_TRUE(infrastructure.Close());
        EXPECT_FALSE(infrastructure.Close());
    }

    EXPECT_EQ(unsetenv(kLifecycleFileEnv), 0);
    EXPECT_EQ(ReadLifecycleLog(lifecycle_path), (std::vector<std::string>{"loaded", "unloaded"}));
}

}  // namespace
}  // namespace score::mw::com::impl::serialization