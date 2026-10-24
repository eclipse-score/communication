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
#include "score/mw/com/impl/serialization/serialization_orchestrator.h"

#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include "score/os/dlfcn.h"
#include "score/os/mocklib/dlfcn_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::serialization
{
namespace
{

using ::testing::Return;
using ::testing::StrEq;

using SerializerFactoryFn = ISerializer* (*)() noexcept;

constexpr std::uintptr_t kLoadedHandleValue{0x1234U};
constexpr std::uint64_t kStructuralHash{0xABCDU};
constexpr std::uint64_t kAttributeHash{0x12U};

/// Minimal serializer used to prove the orchestrator returns a usable object that delegates work.
/// Serialize/Deserialize simply copy bytes so the test can assert on the result deterministically.
class FakeSerializer final : public ISerializer
{
  public:
    std::size_t Serialize(const void* input, std::byte* output, std::size_t size) override
    {
        const auto* const in = static_cast<const std::byte*>(input);
        for (std::size_t index = 0U; index < size; ++index)
        {
            output[index] = in[index];
        }
        return size;
    }

    std::size_t Deserialize(void* output, const std::byte* input, std::size_t size) override
    {
        auto* const out = static_cast<std::byte*>(output);
        for (std::size_t index = 0U; index < size; ++index)
        {
            out[index] = input[index];
        }
        return size;
    }
};

ISerializer* CreateFakeSerializer() noexcept
{
    return new (std::nothrow) FakeSerializer{};
}

ISerializer* CreateNullSerializer() noexcept
{
    return nullptr;
}

void* SymbolAddress(SerializerFactoryFn factory) noexcept
{
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(factory));
}

void ExpectLibraryOpens(score::os::DlfcnMock& dlfcn_mock, void* handle)
{
    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/path/to/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{handle}));
}

TEST(SerializationOrchestratorTest, SelectsAndCreatesSerializerForLookupKey)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    const SerializerLookupKey key{kStructuralHash, kAttributeHash};

    ExpectLibraryOpens(dlfcn_mock, loaded_handle);
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, StrEq(key.FactorySymbolName())))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{SymbolAddress(&CreateFakeSerializer)}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    const SerializationOrchestrator orchestrator{infrastructure};

    const auto serializer_result = orchestrator.SelectSerializer(key);
    ASSERT_TRUE(serializer_result.has_value());
    ASSERT_NE(serializer_result.value().get(), nullptr);

    const std::array<std::byte, 4U> source{std::byte{1U}, std::byte{2U}, std::byte{3U}, std::byte{4U}};
    std::array<std::byte, 4U> destination{};
    EXPECT_EQ(serializer_result.value()->Serialize(source.data(), destination.data(), source.size()),
              source.size());
    EXPECT_EQ(destination, source);
}

TEST(SerializationOrchestratorTest, ReportsFailureWhenFactorySymbolIsMissing)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    const SerializerLookupKey key{kStructuralHash, kAttributeHash};

    ExpectLibraryOpens(dlfcn_mock, loaded_handle);
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, StrEq(key.FactorySymbolName())))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    const SerializationOrchestrator orchestrator{infrastructure};

    EXPECT_FALSE(orchestrator.SelectSerializer(key).has_value());
}

TEST(SerializationOrchestratorTest, ReportsFailureWhenFactoryReturnsNullptr)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    const SerializerLookupKey key{kStructuralHash, kAttributeHash};

    ExpectLibraryOpens(dlfcn_mock, loaded_handle);
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, StrEq(key.FactorySymbolName())))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{SymbolAddress(&CreateNullSerializer)}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    const SerializationOrchestrator orchestrator{infrastructure};

    EXPECT_FALSE(orchestrator.SelectSerializer(key).has_value());
}

TEST(SerializationOrchestratorTest, ReportsFailureWhenLibraryIsNotOpen)
{
    score::os::DlfcnMock dlfcn_mock{};
    const SerializerLookupKey key{kStructuralHash, kAttributeHash};

    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/definitely/not/a/real/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createUnspecifiedError())));

    SerializationInfrastructure infrastructure{"/definitely/not/a/real/plugin.so", &dlfcn_mock};
    const SerializationOrchestrator orchestrator{infrastructure};

    EXPECT_FALSE(orchestrator.SelectSerializer(key).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
