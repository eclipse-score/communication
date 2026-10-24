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
#include "score/mw/com/impl/bindings/someip/serialization/someip_serialization_client.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/serialization/serialization_infrastructure.h"
#include "score/mw/com/impl/serialization/serialization_orchestrator.h"
#include "score/mw/com/impl/serialization/serialization_request.h"
#include "score/mw/com/impl/serialization/serialization_service.h"
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

namespace score::mw::com::impl::someip::serialization
{
namespace
{

using ::testing::Return;
using ::testing::StrEq;

using SerializerFactoryFn = impl::serialization::ISerializer* (*)() noexcept;

constexpr std::uintptr_t kLoadedHandleValue{0x1234U};
constexpr std::uint64_t kStructuralHash{0xABCDU};
constexpr std::uint64_t kAttributeHash{0x12U};

class FakeKeyResolver final : public impl::serialization::ISerializerKeyResolver
{
  public:
    explicit FakeKeyResolver(score::Result<impl::serialization::SerializerLookupKey> result)
        : result_{std::move(result)}
    {
    }

    score::Result<impl::serialization::SerializerLookupKey> Resolve(
        const impl::serialization::SerializationRequest&) const noexcept override
    {
        return result_;
    }

  private:
    score::Result<impl::serialization::SerializerLookupKey> result_;
};

class FakeSerializer final : public impl::serialization::ISerializer
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

impl::serialization::ISerializer* CreateFakeSerializer() noexcept
{
    return new (std::nothrow) FakeSerializer{};
}

void* SymbolAddress(SerializerFactoryFn factory) noexcept
{
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(factory));
}

InstanceSpecifier MakeInstanceSpecifier(const std::string& value)
{
    auto instance_specifier = InstanceSpecifier::Create(std::string{value});
    return std::move(instance_specifier).value();
}

TEST(SomeIpSerializationClientTest, OptsInAndReceivesSerializer)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);
    const impl::serialization::SerializerLookupKey key{kStructuralHash, kAttributeHash};

    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/path/to/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));
    EXPECT_CALL(dlfcn_mock, dlsym(loaded_handle, StrEq(key.FactorySymbolName())))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{SymbolAddress(&CreateFakeSerializer)}));

    impl::serialization::SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    impl::serialization::SerializationOrchestrator orchestrator{infrastructure};
    const FakeKeyResolver key_resolver{key};
    impl::serialization::SerializationService service{orchestrator, key_resolver};

    const SomeIpSerializationClient client{service};
    const auto serializer_result = client.OptInForSerialization(MakeInstanceSpecifier("/some/instance"));

    ASSERT_TRUE(serializer_result.has_value());
    EXPECT_NE(serializer_result.value().get(), nullptr);
}

TEST(SomeIpSerializationClientTest, ForwardsFailureFromBindingIndependentLayer)
{
    score::os::DlfcnMock dlfcn_mock{};
    void* const loaded_handle = reinterpret_cast<void*>(kLoadedHandleValue);

    EXPECT_CALL(dlfcn_mock,
                dlopen(StrEq("/path/to/plugin.so"),
                       score::os::Dlfcn::DlOpenFlag::kNow | score::os::Dlfcn::DlOpenFlag::kLocal))
        .WillOnce(Return(score::cpp::expected<void*, score::os::Error>{loaded_handle}));

    impl::serialization::SerializationInfrastructure infrastructure{"/path/to/plugin.so", &dlfcn_mock};
    impl::serialization::SerializationOrchestrator orchestrator{infrastructure};
    const FakeKeyResolver key_resolver{
        score::MakeUnexpected(ComErrc::kBindingFailure, "no serializer for instance")};
    impl::serialization::SerializationService service{orchestrator, key_resolver};

    const SomeIpSerializationClient client{service};

    EXPECT_FALSE(client.OptInForSerialization(MakeInstanceSpecifier("/some/instance")).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip::serialization
