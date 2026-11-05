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
#include "score/mw/com/impl/serialization/serialization_service.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/serialization/serialization_infrastructure.h"
#include "score/mw/com/impl/serialization/serialization_orchestrator.h"
#include "score/mw/com/impl/serialization/serialization_request.h"
#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_key_resolver.h"
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include "score/os/dlfcn.h"
#include "score/os/mocklib/dlfcn_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

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

/// Returns a fixed resolution result for any request, standing in for the deployment-config lookup.
class FakeKeyResolver final : public ISerializerKeyResolver
{
  public:
    explicit FakeKeyResolver(score::Result<SerializerLookupKey> result) : result_{std::move(result)} {}

    score::Result<SerializerLookupKey> Resolve(const SerializationRequest&) const noexcept override
    {
        return result_;
    }

  private:
    score::Result<SerializerLookupKey> result_;
};

class FakeSerializer final : public ISerializer
{
  public:
    std::size_t Serialize(const void*, std::byte*, std::size_t size) override
    {
        return size;
    }

    std::size_t Deserialize(void*, const std::byte*, std::size_t size) override
    {
        return size;
    }
};

ISerializer* CreateFakeSerializer() noexcept
{
    return new (std::nothrow) FakeSerializer{};
}

void* SymbolAddress(SerializerFactoryFn factory) noexcept
{
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(factory));
}

SerializationRequest MakeRequest(const std::string& instance)
{
    auto instance_specifier = InstanceSpecifier::Create(std::string{instance});
    return SerializationRequest{std::move(instance_specifier).value()};
}

TEST(SerializationServiceTest, ReturnsSerializerForOptInRequest)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    const SerializerLookupKey key{kStructuralHash, kAttributeHash};

    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/path/to/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, StrEq(key.FactorySymbolName())))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{SymbolAddress(&CreateFakeSerializer)}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    SerializationOrchestrator orchestrator{infrastructure};
    const FakeKeyResolver key_resolver{key};
    const SerializationService service{orchestrator, key_resolver};

    const auto serializer_result = service.GetSerializer(MakeRequest("/some/instance"));
    ASSERT_TRUE(serializer_result.has_value());
    EXPECT_NE(serializer_result.value().get(), nullptr);
}

TEST(SerializationServiceTest, PropagatesFailureWhenRequestCannotBeResolved)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);

    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/path/to/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));

    SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    SerializationOrchestrator orchestrator{infrastructure};
    const FakeKeyResolver key_resolver{score::MakeUnexpected(ComErrc::kBindingFailure, "no serializer for instance")};
    const SerializationService service{orchestrator, key_resolver};

    EXPECT_FALSE(service.GetSerializer(MakeRequest("/some/instance")).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
