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

#include "score/os/dlfcn.h"
#include "score/os/mocklib/dlfcn_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace score::mw::com::impl::serialization
{
namespace
{

using BackendProbeValueFn = std::uint32_t (*)() noexcept;

using ::testing::Return;
using ::testing::StrEq;

constexpr std::uintptr_t kLoadedHandleValue{0x1234U};
constexpr std::uint32_t kExpectedBackendProbeValue = 0xC0FFEE42U;

std::uint32_t BackendProbeValue() noexcept
{
    return kExpectedBackendProbeValue;
}

void* MakeProbeSymbolAddress() noexcept
{
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&BackendProbeValue));
}

class SerializationInfrastructureBackendIntegration : public ::testing::Test
{
  protected:
    void ExpectLibraryOpens() noexcept
    {
        EXPECT_CALL(dlfcn_mock_, dlopen(StrEq(kPluginPath),
                                        score::os::Dlfcn::DlOpenFlag::kNow |
                                            score::os::Dlfcn::DlOpenFlag::kLocal))
            .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle_}));
    }

    void ExpectLibraryFailsToOpen(const std::string& library_path) noexcept
    {
        EXPECT_CALL(dlfcn_mock_, dlopen(StrEq(library_path),
                                        score::os::Dlfcn::DlOpenFlag::kNow |
                                            score::os::Dlfcn::DlOpenFlag::kLocal))
            .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));
    }

    void ExpectSymbolResolves(const char* symbol_name, void* address) noexcept
    {
        EXPECT_CALL(dlfcn_mock_, dlsym(loaded_handle_, symbol_name))
            .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{address}));
    }

    void ExpectSymbolMissing(const char* symbol_name) noexcept
    {
        EXPECT_CALL(dlfcn_mock_, dlsym(loaded_handle_, symbol_name))
            .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));
    }

    static constexpr const char* kPluginPath = "/path/to/plugin.so";
    score::os::DlfcnMock dlfcn_mock_{};
    void* const loaded_handle_ = reinterpret_cast<void*>(kLoadedHandleValue);
};

TEST_F(SerializationInfrastructureBackendIntegration, LoadsFixtureLibraryAndResolvesKnownSymbol)
{
    ExpectLibraryOpens();
    ExpectSymbolResolves("BackendProbeValue", MakeProbeSymbolAddress());
    ExpectSymbolMissing("NotPresent");

    SerializationInfrastructure infrastructure{kPluginPath, &dlfcn_mock_};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto probe_value = infrastructure.ResolveSerializer<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_TRUE(probe_value.has_value());
    EXPECT_EQ(probe_value.value()(), kExpectedBackendProbeValue);

    const auto missing = infrastructure.ResolveSerializer<BackendProbeValueFn>("NotPresent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(SerializationInfrastructureBackendIntegration, CachesResolvedSymbolsForReuse)
{
    ExpectLibraryOpens();
    ExpectSymbolResolves("BackendProbeValue", MakeProbeSymbolAddress());

    SerializationInfrastructure infrastructure{kPluginPath, &dlfcn_mock_};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto first = infrastructure.ResolveSerializer<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_TRUE(first.has_value());

    const auto second = infrastructure.ResolveSerializer<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first.value(), second.value());
    EXPECT_EQ(second.value()(), kExpectedBackendProbeValue);
}

TEST_F(SerializationInfrastructureBackendIntegration, ReportsFailureForMissingLibrary)
{
    constexpr const char* kMissingLibraryPath = "/definitely/not/a/real/backend/library.so";
    ExpectLibraryFailsToOpen(kMissingLibraryPath);

    SerializationInfrastructure infrastructure{kMissingLibraryPath, &dlfcn_mock_};
    EXPECT_FALSE(infrastructure.IsOpen().has_value());
}

TEST_F(SerializationInfrastructureBackendIntegration, MissingSymbolReturnsNull)
{
    ExpectLibraryOpens();
    ExpectSymbolMissing("NotPresent");

    SerializationInfrastructure infrastructure{kPluginPath, &dlfcn_mock_};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto missing = infrastructure.ResolveSerializer<BackendProbeValueFn>("NotPresent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(SerializationInfrastructureBackendIntegration, CloseSucceedsAndSecondCloseFails)
{
    ExpectLibraryOpens();
    EXPECT_CALL(dlfcn_mock_, dlclose(loaded_handle_)).WillOnce(Return(score::cpp::blank{}));

    SerializationInfrastructure infrastructure{kPluginPath, &dlfcn_mock_};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());
    EXPECT_TRUE(infrastructure.Close().has_value());
    EXPECT_FALSE(infrastructure.Close().has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
