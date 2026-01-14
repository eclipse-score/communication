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
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

constexpr LolaMethodId kDummyMethodId{123U};
constexpr LolaServiceId kDummyServiceId{123U};
constexpr LolaServiceInstanceId::InstanceId kDummyInstanceId{456U};
constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueueSize{5U};
constexpr ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounter{5U};

constexpr memory::DataTypeSizeInfo kValidInArgSizeInfo{sizeof(std::uint32_t), alignof(std::uint32_t)};
constexpr memory::DataTypeSizeInfo kValidReturnSizeInfo{sizeof(std::uint64_t), alignof(std::uint64_t)};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithInArgsAndReturn{kValidInArgSizeInfo,
                                                                                    kValidReturnSizeInfo,
                                                                                    10U};
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

class SkeletonMethodFixture : public SkeletonMockedMemoryFixture
{
  public:
    SkeletonMethodFixture()
    {
        InitialiseSkeleton(config_store_.GetInstanceIdentifier());
    }

    SkeletonMethodFixture& GivenASkeletonMethod()
    {
        unit_ = std::make_unique<SkeletonMethod>(*skeleton_, element_fq_id_);
        return *this;
    }

    SkeletonMethodFixture& WithARegisteredCallback()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(unit_ != nullptr);
        unit_->Register(registered_type_erased_callback_.AsStdFunction());
        return *this;
    }

    SkeletonMethodFixture& WhichCapturesRegisteredMethodCallHandler()
    {
        ON_CALL(message_passing_mock_, RegisterMethodCallHandler(proxy_instance_identifier_, _))
            .WillByDefault(WithArg<1>(Invoke([this](auto method_call_handler) -> ResultBlank {
                captured_method_call_handler_.emplace(std::move(method_call_handler));
                return {};
            })));
        return *this;
    }

    ConfigurationStore config_store_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
                                     make_ServiceIdentifierType("foo"),
                                     QualityType::kASIL_QM,
                                     LolaServiceTypeDeployment{kDummyServiceId},
                                     LolaServiceInstanceDeployment{kDummyInstanceId}};

    std::unique_ptr<SkeletonMethod> unit_{nullptr};

    const ElementFqId element_fq_id_{kDummyServiceId, kDummyMethodId, kDummyInstanceId, ServiceElementType::METHOD};
    ProxyInstanceIdentifier proxy_instance_identifier_{kDummyProxyInstanceCounter, kDummyApplicationId};
    SkeletonInstanceIdentifier skeleton_instance_identifier_{kDummyServiceId, kDummyInstanceId};

    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> registered_type_erased_callback_{};
    std::optional<IMessagePassingService::MethodCallHandler> captured_method_call_handler_{};
};

// Note: OnProxyMethodSubscribeFinished is private to prevent end users of mw::com accessing it. However, it is used
// by the parent Skeleton (accessed via SkeletonMethodView) and so is part of the public API within the implementation
// of mw::com. Therefore, we test it here.
using SkeletonMethodOnProxyMethodSubscribedFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingWithoutRegisteringCallbackTerminates)
{
    GivenASkeletonMethod();

    // When calling OnProxyMethodSubscribeFinished without first calling Register
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(
        score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
            kTypeErasedInfoWithInArgsAndReturn, kValidInArgStorage, kValidReturnStorage, proxy_instance_identifier_));
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingRegistersRegisteredCallbackWithMessagePassing)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing with the
    // registered callback. We check this by calling the subscribed callback and checking that the registered
    // callback was called.
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _));
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(proxy_instance_identifier_, _))
        .WillOnce(WithArg<1>(Invoke([](auto method_call_handler) -> ResultBlank {
            std::invoke(method_call_handler, kDummyQueueSize);
            return {};
        })));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    const auto result = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithInArgsAndReturn, kValidInArgStorage, kValidReturnStorage, proxy_instance_identifier_);

    // Then the result will be valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, PropagatesErrorFromMessagePassing)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing which returns an error.
    const auto error_code = ComErrc::kCallQueueFull;
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(proxy_instance_identifier_, _))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    const auto result = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithInArgsAndReturn, kValidInArgStorage, kValidReturnStorage, proxy_instance_identifier_);

    // Then the result will contain an error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, FailingToGetLolaRuntimeTerminates)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that trying to get the lola binding runtime returns an nullptr
    EXPECT_CALL(runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillOnce(Return(nullptr));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
            kTypeErasedInfoWithInArgsAndReturn, kValidInArgStorage, kValidReturnStorage, proxy_instance_identifier_));
}

using SkeletonMethodCallFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodCallFixture, CallingWithInArgTypeInfoAndStorageDispatchesToRegisteredCallbackWithValidInArgStorage)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Expecting that the registered type erased callback is called with only InArgs storage
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_arg_storage, auto result_storage) {
            EXPECT_TRUE(in_arg_storage.has_value());
            EXPECT_FALSE(result_storage.has_value());
        }));

    // Given that OnProxyMethodSubscribeFinished was called with only InArgs TypeErasedElementInfo and storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithInArgsOnly, kValidInArgStorage, kEmptyReturnStorage, proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueueSize);
}

TEST_F(SkeletonMethodCallFixture,
       CallingWithReturnTypeInfoAndStorageDispatchesToRegisteredCallbackWithValidReturnStorage)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Expecting that the registered type erased callback is called with only Return storage
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_arg_storage, auto result_storage) {
            EXPECT_FALSE(in_arg_storage.has_value());
            EXPECT_TRUE(result_storage.has_value());
        }));

    // Given that OnProxyMethodSubscribeFinished was called with only Return TypeErasedElementInfo and storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithReturnOnly, kEmptyInArgStorage, kValidReturnStorage, proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueueSize);
}

TEST_F(SkeletonMethodCallFixture,
       CallingWithInArgAndReturnTypeInfoAndStorageDispatchesToRegisteredCallbackWithValidStorages)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Expecting that the registered type erased callback is called with InArgs and Return storage
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_arg_storage, auto result_storage) {
            EXPECT_TRUE(in_arg_storage.has_value());
            EXPECT_TRUE(result_storage.has_value());
        }));

    // Given that OnProxyMethodSubscribeFinished was called with both InArgs and Return TypeErasedElementInfo and
    // storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithInArgsAndReturn, kValidInArgStorage, kValidReturnStorage, proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueueSize);
}

TEST_F(SkeletonMethodCallFixture, CallingWithNoTypeInfosAndStoragesDispatchesToRegisteredCallbackWithNoValidStorages)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Expecting that the registered type erased callback is called with no InArgs or Return storage
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_arg_storage, auto result_storage) {
            EXPECT_FALSE(in_arg_storage.has_value());
            EXPECT_FALSE(result_storage.has_value());
        }));

    // Given that OnProxyMethodSubscribeFinished was called with only Return TypeErasedElementInfo and storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{},
        kEmptyInArgStorage,
        kEmptyReturnStorage,
        proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueueSize);
}

TEST_F(SkeletonMethodCallFixture, CallingWithInArgTypeInfoAndNoValidStorageTerminates)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Given that OnProxyMethodSubscribeFinished was called with only InArgs TypeErasedElementInfo but no valid InArgs
    // storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithInArgsOnly, kEmptyInArgStorage, kEmptyReturnStorage, proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    // Then the program terminates
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(std::invoke(captured_method_call_handler_.value(), kDummyQueueSize));
}

TEST_F(SkeletonMethodCallFixture, CallingWithReturnTypeInfoAndNoValidStorageTerminates)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Given that OnProxyMethodSubscribeFinished was called with only Return TypeErasedElementInfo but no valid Return
    // storage
    score::cpp::ignore = SkeletonMethodView{*unit_}.OnProxyMethodSubscribeFinished(
        kTypeErasedInfoWithReturnOnly, kEmptyInArgStorage, kEmptyReturnStorage, proxy_instance_identifier_);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing
    // message to call the method) Then the program terminates
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(std::invoke(captured_method_call_handler_.value(), kDummyQueueSize));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
