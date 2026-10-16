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

TEST(SerializationInfrastructureBackendIntegration, LoadsFixtureLibraryAndResolvesKnownSymbol)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    EXPECT_CALL(dlfcn_mock, dlopen(StrEq("/path/to/plugin.so"),
                                  score::os::Dlfcn::DlOpenFlag::kNow |
                                      score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, "BackendProbeValue"))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{MakeProbeSymbolAddress()}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, "NotPresent"))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto probe_value = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(probe_value, nullptr);
    EXPECT_EQ(probe_value(), kExpectedBackendProbeValue);

    const auto missing = infrastructure.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
}

TEST(SerializationInfrastructureBackendIntegration, CachesResolvedSymbolsForReuse)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    EXPECT_CALL(dlfcn_mock, dlopen(StrEq("/path/to/plugin.so"),
                                  score::os::Dlfcn::DlOpenFlag::kNow |
                                      score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, "BackendProbeValue"))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{MakeProbeSymbolAddress()}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto first = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(first, nullptr);

    const auto second = infrastructure.ResolveSymbol<BackendProbeValueFn>("BackendProbeValue");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first, second);
    EXPECT_EQ(second(), kExpectedBackendProbeValue);
}

TEST(SerializationInfrastructureBackendIntegration, ReportsFailureForMissingLibrary)
{
    score::os::DlfcnMock dlfcn_mock{};
    EXPECT_CALL(dlfcn_mock, dlopen(StrEq("/definitely/not/a/real/backend/library.so"),
                                  score::os::Dlfcn::DlOpenFlag::kNow |
                                      score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));

    SerializationInfrastructure infrastructure{"/definitely/not/a/real/backend/library.so", &dlfcn_mock};
    EXPECT_FALSE(infrastructure.IsOpen().has_value());
}

TEST(SerializationInfrastructureBackendIntegration, MissingSymbolReturnsNull)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    EXPECT_CALL(dlfcn_mock, dlopen(StrEq("/path/to/plugin.so"),
                                  score::os::Dlfcn::DlOpenFlag::kNow |
                                      score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, "NotPresent"))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());

    const auto missing = infrastructure.ResolveSymbol<BackendProbeValueFn>("NotPresent");
    EXPECT_EQ(missing, nullptr);
}

TEST(SerializationInfrastructureBackendIntegration, CloseSucceedsAndSecondCloseFails)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    EXPECT_CALL(dlfcn_mock, dlopen(StrEq("/path/to/plugin.so"),
                                  score::os::Dlfcn::DlOpenFlag::kNow |
                                      score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlclose(loaded_handle)).WillOnce(Return(score::cpp::blank{}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    ASSERT_TRUE(infrastructure.IsOpen().has_value());
    EXPECT_TRUE(infrastructure.IsOpen().value());
    EXPECT_TRUE(infrastructure.Close().has_value());
    EXPECT_FALSE(infrastructure.Close().has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::serialization