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
#include "score/mw/com/impl/bindings/lola/proxy_method.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert_support.hpp>
#include <score/span.hpp>
#include <score/stop_token.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

constexpr LolaMethodId kDummyMethodId{123U};
constexpr std::size_t kDummyQueuePosition{3U};
constexpr std::size_t kDummyQueueSize{10U};
constexpr memory::DataTypeSizeInfo kValidInArgSizeInfo{sizeof(std::uint32_t), alignof(std::uint32_t)};
constexpr memory::DataTypeSizeInfo kValidReturnSizeInfo{sizeof(std::uint64_t), alignof(std::uint64_t)};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithInArgsAndReturn{kValidInArgSizeInfo,
                                                                                    kValidReturnSizeInfo,
                                                                                    kDummyQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithInArgsOnly{
    kValidInArgSizeInfo,
    std::optional<memory::DataTypeSizeInfo>{},
    10U};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithReturnOnly{
    std::optional<memory::DataTypeSizeInfo>{},
    kValidReturnSizeInfo,
    10U};

const std::optional<score::cpp::span<std::byte>> kValidInArgStorage{score::cpp::span<std::byte>{}};
const std::optional<score::cpp::span<std::byte>> kValidReturnStorage{score::cpp::span<std::byte>{}};

const std::optional<score::cpp::span<std::byte>> kEmptyInArgStorage{};
const std::optional<score::cpp::span<std::byte>> kEmptyReturnStorage{};

class ProxyMethodFixture : public ProxyMockedMemoryFixture
{
  public:
    ProxyMethodFixture()
    {
        InitialiseProxyWithConstructor(config_store_.GetInstanceIdentifier());
    }

    ProxyMethodFixture& GivenAProxyMethod()
    {
        unit_ = std::make_unique<ProxyMethod>(*proxy_, element_fq_id_, kTypeErasedInfoWithInArgsAndReturn);
        return *this;
    }

    ProxyMethodFixture& GivenAProxyMethodWithoutTypeErasedElementInfo()
    {
        unit_ = std::make_unique<ProxyMethod>(
            *proxy_, element_fq_id_, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{});
        return *this;
    }

    ProxyMethodFixture& GivenAProxyMethodWithoutInArgsTypeErasedElementInfo()
    {
        unit_ = std::make_unique<ProxyMethod>(*proxy_, element_fq_id_, kTypeErasedInfoWithReturnOnly);
        return *this;
    }

    ProxyMethodFixture& GivenAProxyMethodWithoutReturnTypeErasedElementInfo()
    {
        unit_ = std::make_unique<ProxyMethod>(*proxy_, element_fq_id_, kTypeErasedInfoWithInArgsOnly);
        return *this;
    }

    ConfigurationStore config_store_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
                                     make_ServiceIdentifierType("foo"),
                                     QualityType::kASIL_QM,
                                     LolaServiceTypeDeployment{42U},
                                     LolaServiceInstanceDeployment{1U}};

    std::unique_ptr<ProxyMethod> unit_{nullptr};

    score::cpp::stop_source stop_source_{};
    const ElementFqId element_fq_id_{lola_service_id_,
                                     kDummyMethodId,
                                     lola_service_instance_id_.GetId(),
                                     ServiceElementType::METHOD};
};

TEST_F(ProxyMethodFixture, GetTypeErasedElementInfoReturnsValueSetOnConstruction)
{
    GivenAProxyMethod();

    // When calling GetInArgTypeErasedData
    const auto result = unit_->GetTypeErasedElementInfo();

    // Then the result is the same as the DataTypeSizeInfo that was passed to the constructor.
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().in_arg_type_info, kTypeErasedInfoWithInArgsAndReturn.in_arg_type_info);
    EXPECT_EQ(result.value().return_type_info, kTypeErasedInfoWithInArgsAndReturn.return_type_info);
    EXPECT_EQ(result.value().queue_size, kTypeErasedInfoWithInArgsAndReturn.queue_size);
}

TEST_F(ProxyMethodFixture, GetTypeErasedElementInfoReturnsEmptyValueWhenNotSetOnConstruction)
{
    GivenAProxyMethodWithoutTypeErasedElementInfo();

    // When calling GetInArgTypeErasedData
    const auto result = unit_->GetTypeErasedElementInfo();

    // Then the result is empty
    ASSERT_FALSE(result.has_value());
}

TEST_F(ProxyMethodFixture, FailingToGetBindingRuntimeTerminates)
{
    // Expecting that GetBindingRuntime is called on the impl runtime which returns a nullptr
    EXPECT_CALL(runtime_mock_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillOnce(Return(nullptr));

    // When constructing the ProxyMethod
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(GivenAProxyMethod());
}

using ProxyMethodAllocateInArgsFixture = ProxyMethodFixture;
TEST_F(ProxyMethodAllocateInArgsFixture, CallingAfterSettingValidStoragesWithValidTypeInfosDispatchesToBinding)
{
    GivenAProxyMethod();

    // Given that SetInArgsAndReturnStorages was called with valid InArgs storage
    unit_->SetInArgsAndReturnStorages(kValidInArgStorage, kEmptyReturnStorage);

    // When calling AllocateInArgs
    const auto result = unit_->AllocateInArgs(kDummyQueuePosition);

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodAllocateInArgsFixture, CallingAfterSettingValidInArgsStorageWithoutInArgsTypeInfoTerminates)
{
    GivenAProxyMethodWithoutInArgsTypeErasedElementInfo();

    // Given that SetInArgsAndReturnStorages was called with valid InArgs storage
    unit_->SetInArgsAndReturnStorages(kValidInArgStorage, kEmptyReturnStorage);

    // When calling AllocateInArgs
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateInArgs(kDummyQueuePosition));
}

TEST_F(ProxyMethodAllocateInArgsFixture, CallingAfterSettingEmptyInArgsStorageWithInArgsTypeInfoTerminates)
{
    GivenAProxyMethod();

    // Given that SetInArgsAndReturnStorages was called with empty InArgs storage
    unit_->SetInArgsAndReturnStorages(kEmptyReturnStorage, kEmptyReturnStorage);

    // When calling AllocateInArgs
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateInArgs(kDummyQueuePosition));
}

TEST_F(ProxyMethodAllocateInArgsFixture, CallingAfterSettingValidInArgsStorageWithoutTypeInfoTerminates)
{
    GivenAProxyMethodWithoutTypeErasedElementInfo();

    // Given that SetInArgsAndReturnStorages was called with empty InArgs storage
    unit_->SetInArgsAndReturnStorages(kEmptyReturnStorage, kEmptyReturnStorage);

    // When calling AllocateInArgs
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateInArgs(kDummyQueuePosition));
}

using ProxyMethodAllocateReturnTypeFixture = ProxyMethodFixture;
TEST_F(ProxyMethodAllocateReturnTypeFixture, CallingAfterSettingValidStoragesWithValidTypeInfosDispatchesToBinding)
{
    GivenAProxyMethod();

    // Given that SetInArgsAndReturnStorages was called with valid Return storage
    unit_->SetInArgsAndReturnStorages(kEmptyInArgStorage, kValidReturnStorage);

    // When calling AllocateReturnType
    const auto result = unit_->AllocateReturnType(kDummyQueuePosition);

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodAllocateReturnTypeFixture, CallingAfterSettingValidReturnStorageWithoutReturnTypeInfoTerminates)
{
    GivenAProxyMethodWithoutReturnTypeErasedElementInfo();

    // Given that SetInArgsAndReturnStorages was called with valid Return storage
    unit_->SetInArgsAndReturnStorages(kEmptyInArgStorage, kValidReturnStorage);

    // When calling AllocateReturnType
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateReturnType(kDummyQueuePosition));
}

TEST_F(ProxyMethodAllocateReturnTypeFixture, CallingAfterSettingEmptyReturnStorageWithReturnTypeInfoTerminates)
{
    GivenAProxyMethod();

    // Given that SetInArgsAndReturnStorages was called with empty Return storage
    unit_->SetInArgsAndReturnStorages(kEmptyReturnStorage, kEmptyReturnStorage);

    // When calling AllocateReturnType
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateReturnType(kDummyQueuePosition));
}

TEST_F(ProxyMethodAllocateReturnTypeFixture, CallingAfterSettingValidReturnStorageWithoutTypeInfoTerminates)
{
    GivenAProxyMethodWithoutTypeErasedElementInfo();

    // Given that SetInArgsAndReturnStorages was called with empty Return storage
    unit_->SetInArgsAndReturnStorages(kEmptyReturnStorage, kEmptyReturnStorage);

    // When calling AllocateReturnType
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->AllocateReturnType(kDummyQueuePosition));
}

using ProxyMethodDoCallFixture = ProxyMethodFixture;
TEST_F(ProxyMethodDoCallFixture, DispatchesToMessagePassingBinding)
{
    GivenAProxyMethod();

    // Expecting that CallMethod is called on the message passing binding which returns success
    EXPECT_CALL(*mock_service_, CallMethod(_, kDummyQueuePosition))
        .WillOnce(WithArg<0>(Invoke([](auto proxy_instance_identifier) -> ResultBlank {
            // Then CallMethod is called with a ProxyInstanceIdentifier containing the application id from the
            // configuration
            EXPECT_EQ(proxy_instance_identifier.process_identifier, kDummyApplicationId);
            return ResultBlank{};
        })));

    // When calling DoCall
    const auto result = unit_->DoCall(kDummyQueuePosition);

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodDoCallFixture, PropagatesErrorFromMessagePassingBinding)
{
    GivenAProxyMethod();

    // Expecting that CallMethod is called on the message passing binding which returns an erro
    const auto call_method_error_code = ComErrc::kBindingFailure;
    EXPECT_CALL(*mock_service_, CallMethod(_, kDummyQueuePosition))
        .WillOnce(Return(MakeUnexpected(call_method_error_code)));

    // When calling DoCall
    const auto result = unit_->DoCall(kDummyQueuePosition);

    // Then the error from the call to CallMethod is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), call_method_error_code);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
