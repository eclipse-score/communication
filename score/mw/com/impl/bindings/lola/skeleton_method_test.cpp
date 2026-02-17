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
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/shared_memory_resource.h"
#include "score/memory/shared/shared_memory_resource_mock.h"
#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_call_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_subscription_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
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
constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueueSize{12U};
constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueuePosition{kDummyQueueSize / 2};
constexpr ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounter{6U};

constexpr memory::DataTypeSizeInfo kValidInArgSizeInfo{sizeof(std::uint32_t), alignof(std::uint32_t)};
constexpr memory::DataTypeSizeInfo kValidReturnSizeInfo{sizeof(std::uint64_t), alignof(std::uint64_t)};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithInArgsAndReturn{kValidInArgSizeInfo,
                                                                                    kValidReturnSizeInfo,
                                                                                    kDummyQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithInArgsOnly{
    kValidInArgSizeInfo,
    std::optional<memory::DataTypeSizeInfo>{},
    kDummyQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithReturnOnly{
    std::optional<memory::DataTypeSizeInfo>{},
    kValidReturnSizeInfo,
    kDummyQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kTypeErasedInfoWithNoInArgsOrReturn{
    std::optional<memory::DataTypeSizeInfo>{},
    std::optional<memory::DataTypeSizeInfo>{},
    kDummyQueueSize};

const uid_t kAllowedProxyUid{10};
const auto kAsilLevel{QualityType::kASIL_QM};

constexpr auto InArgsQueueStorageSize = kValidInArgSizeInfo.Size() * kDummyQueueSize;
constexpr auto ReturnQueueStorageSize = kValidReturnSizeInfo.Size() * kDummyQueueSize;
std::array<std::byte, InArgsQueueStorageSize> InArgsData{};
std::array<std::byte, ReturnQueueStorageSize> ReturnData{};
const std::optional<score::cpp::span<std::byte>> kValidInArgStorage{{InArgsData.data(), InArgsData.size()}};
const std::optional<score::cpp::span<std::byte>> kValidReturnStorage{{ReturnData.data(), ReturnData.size()}};

const std::optional<score::cpp::span<std::byte>> kEmptyInArgStorage{};
const std::optional<score::cpp::span<std::byte>> kEmptyReturnStorage{};

class SkeletonMethodFixture : public SkeletonMockedMemoryFixture
{
  public:
    SkeletonMethodFixture()
    {
        InitialiseSkeleton(config_store_.GetInstanceIdentifier());

        ON_CALL(message_passing_mock_, RegisterMethodCallHandler(_, _, _, _))
            .WillByDefault(WithArgs<0, 1>(Invoke(
                [this](auto asil_level, auto proxy_method_instance_identifier) -> Result<MethodCallRegistrationGuard> {
                    return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                                      asil_level,
                                                                      proxy_method_instance_identifier,
                                                                      method_call_registration_guard_scope_);
                })));
    }

    SkeletonMethodFixture& GivenASkeletonMethod()
    {
        unit_ = std::make_unique<SkeletonMethod>(*skeleton_, element_fq_id_);
        return *this;
    }

    SkeletonMethodFixture& WithARegisteredCallback()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(unit_ != nullptr);
        unit_->RegisterHandler(registered_type_erased_callback_.AsStdFunction());
        return *this;
    }

    SkeletonMethodFixture& WhichCapturesRegisteredMethodCallHandler()
    {
        EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(_, _, _, _))
            .WillOnce(WithArgs<0, 1, 2>(Invoke([this](auto asil_level,
                                                      auto proxy_method_instance_identifier,
                                                      auto method_call_handler) -> Result<MethodCallRegistrationGuard> {
                captured_method_call_handler_.emplace(std::move(method_call_handler));
                return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                                  asil_level,
                                                                  proxy_method_instance_identifier,
                                                                  method_call_registration_guard_scope_);
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

    const ProxyInstanceIdentifier proxy_instance_identifier_{kDummyProxyInstanceCounter, kDummyApplicationId};
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier_{proxy_instance_identifier_,
                                                                          element_fq_id_.element_id_};
    const ProxyInstanceIdentifier proxy_instance_identifier_2_{kDummyProxyInstanceCounter + 1, kDummyApplicationId + 1};
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier_2_{proxy_instance_identifier_2_,
                                                                            element_fq_id_.element_id_};

    SkeletonInstanceIdentifier skeleton_instance_identifier_{kDummyServiceId, kDummyInstanceId};
    std::shared_ptr<memory::shared::ISharedMemoryResource> methods_shared_memory_resource_{
        std::make_shared<memory::shared::SharedMemoryResourceMock>()};

    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> registered_type_erased_callback_{};
    std::optional<IMessagePassingService::MethodCallHandler> captured_method_call_handler_{};

    safecpp::Scope<> method_call_handler_scope_{};
    safecpp::Scope<> method_call_registration_guard_scope_{};
};

using SkeletonMethodOnProxyMethodSubscribedFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingWithoutRegisteringCallbackTerminates)
{
    GivenASkeletonMethod();

    // When calling OnProxyMethodSubscribeFinished without first calling Register
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_ASSERT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                                                     kValidInArgStorage,
                                                                                     kValidReturnStorage,
                                                                                     proxy_method_instance_identifier_,
                                                                                     method_call_handler_scope_,
                                                                                     kAllowedProxyUid,
                                                                                     kAsilLevel));
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingRegistersCallbackWithProvidedData)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing with the data provided to
    // OnProxyMethoSubscribeFinished.
    const auto asil_level = QualityType::kASIL_B;
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(asil_level, proxy_method_instance_identifier_, _, kAllowedProxyUid));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              asil_level);

    // Then the result will be valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingRegistersRegisteredCallbackWithMessagePassing)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing with the
    // registered callback. We check this by calling the subscribed callback and checking that the registered
    // callback was called.
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _));
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(_, _, _, _))
        .WillOnce(WithArgs<0, 1, 2>(Invoke([this](auto asil_level,
                                                  auto proxy_method_instance_identifier,
                                                  auto method_call_handler) -> Result<MethodCallRegistrationGuard> {
            std::invoke(method_call_handler, kDummyQueuePosition);
            return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                              asil_level,
                                                              proxy_method_instance_identifier,
                                                              method_call_registration_guard_scope_);
        })));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              kAsilLevel);

    // Then the result will be valid
    ASSERT_TRUE(result.has_value());
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, PropagatesErrorFromMessagePassing)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing which returns an error.
    const auto error_code = ComErrc::kCallQueueFull;
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(_, proxy_method_instance_identifier_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When calling OnProxyMethodSubscribeFinished with a registered callback
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              kAsilLevel);

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
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                                                     kValidInArgStorage,
                                                                                     kValidReturnStorage,
                                                                                     proxy_method_instance_identifier_,
                                                                                     method_call_handler_scope_,
                                                                                     kAllowedProxyUid,
                                                                                     kAsilLevel));
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, CallingWillNotCallUnregister)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing for each call to
    // OnProxyMethodSubscribeFinished
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_, _, _));

    // And expecting that UnregisterMethodCallHandler will NOT be called
    EXPECT_CALL(message_passing_mock_, UnregisterMethodCallHandler(_, _)).Times(0);

    // When calling OnProxyMethodSubscribeFinished
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              QualityType::kASIL_QM);
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonMethodOnProxyMethodSubscribedFixture, UnregisterWillBeCalledOnAllRegisteredHandlersOnDestruction)
{

    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing for each call to
    // OnProxyMethodSubscribeFinished
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2_, _, _));

    // And expecting that UnregisterMethodCallHandler will be called for each registered handler
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2_));

    // given that OnProxyMethodSubscribeFinished is called twice
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              QualityType::kASIL_QM);
    EXPECT_TRUE(result.has_value());

    const auto result_2 = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                                kValidInArgStorage,
                                                                kValidReturnStorage,
                                                                proxy_method_instance_identifier_2_,
                                                                method_call_handler_scope_,
                                                                kAllowedProxyUid,
                                                                QualityType::kASIL_B);
    EXPECT_TRUE(result_2.has_value());

    // When destroying the SkeletonMethod
    unit_.reset();
}

using SkeletonMethodUnregisterHandlersFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodUnregisterHandlersFixture, CallingWillUnregisterAllHandlersRegisteredOnSubscribe)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyProxyInstanceCounter + 1, kDummyApplicationId + 1};
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier_2{proxy_instance_identifier_2,
                                                                           element_fq_id_.element_id_};
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing for each call to
    // OnProxyMethodSubscribeFinished
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2, _, _));

    // And expecting that UnregisterMethodCallHandler will be called for each registered handler
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2));

    // given that OnProxyMethodSubscribeFinished is called twice
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              QualityType::kASIL_QM);
    EXPECT_TRUE(result.has_value());

    const auto result_2 = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                                kValidInArgStorage,
                                                                kValidReturnStorage,
                                                                proxy_method_instance_identifier_2,
                                                                method_call_handler_scope_,
                                                                kAllowedProxyUid,
                                                                QualityType::kASIL_B);
    EXPECT_TRUE(result_2.has_value());

    // When calling UnregisterMethodCallHandlers
    unit_->UnregisterMethodCallHandlers();
}

using SkeletonMethodOnProxyMethodUnsubscribedFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodOnProxyMethodUnsubscribedFixture, CallingWillUnregisterHandlerCorrespondingToProvidedIdentifier)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyProxyInstanceCounter + 1U,
                                                              kDummyApplicationId + 1U};
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier_2{proxy_instance_identifier_2,
                                                                           element_fq_id_.element_id_};
    GivenASkeletonMethod().WithARegisteredCallback();

    // Expecting that RegisterMethodCallHandler will be called on message passing for each call to
    // OnProxyMethodSubscribeFinished
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2, _, _));

    // And expecting that UnregisterMethodCallHandler will only be called for the handler corresponding to
    // proxy_method_instance_identifier_
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, proxy_method_instance_identifier_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, proxy_method_instance_identifier_2))
        .Times(0);

    // given that OnProxyMethodSubscribeFinished is called twice
    const auto result = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                              kValidInArgStorage,
                                                              kValidReturnStorage,
                                                              proxy_method_instance_identifier_,
                                                              method_call_handler_scope_,
                                                              kAllowedProxyUid,
                                                              QualityType::kASIL_QM);
    EXPECT_TRUE(result.has_value());

    const auto result_2 = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                                kValidInArgStorage,
                                                                kValidReturnStorage,
                                                                proxy_method_instance_identifier_2,
                                                                method_call_handler_scope_,
                                                                kAllowedProxyUid,
                                                                QualityType::kASIL_B);
    EXPECT_TRUE(result_2.has_value());

    // When calling OnProxyMethodUnsubscribe with proxy_method_instance_identifier_
    unit_->OnProxyMethodUnsubscribe(proxy_method_instance_identifier_);
}

TEST_F(SkeletonMethodOnProxyMethodUnsubscribedFixture, CallingBeforeSubscribingTerminates)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // When calling OnProxyMethodUnsubscribe with proxy_method_instance_identifier_ which was never subscribed
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(unit_->OnProxyMethodUnsubscribe(proxy_method_instance_identifier_));
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
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsOnly,
                                                        kValidInArgStorage,
                                                        kEmptyReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition);
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
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithReturnOnly,
                                                        kEmptyInArgStorage,
                                                        kValidReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition);
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
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsAndReturn,
                                                        kValidInArgStorage,
                                                        kValidReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition);
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
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithNoInArgsOrReturn,
                                                        kEmptyInArgStorage,
                                                        kEmptyReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition);
}

TEST_F(SkeletonMethodCallFixture, CallingWithInArgTypeInfoAndNoValidStorageTerminates)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Given that OnProxyMethodSubscribeFinished was called with only InArgs TypeErasedElementInfo but no valid InArgs
    // storage
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithInArgsOnly,
                                                        kEmptyInArgStorage,
                                                        kEmptyReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    // Then the program terminates
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition));
}

TEST_F(SkeletonMethodCallFixture, CallingAfterScopeHasExpiredDoesNotCallTypeErasedCallback)
{
    GivenASkeletonMethod().WithARegisteredCallback().WhichCapturesRegisteredMethodCallHandler();

    // Given that OnProxyMethodSubscribeFinished was called with only Return TypeErasedElementInfo but no valid Return
    // storage
    score::cpp::ignore = unit_->OnProxyMethodSubscribeFinished(kTypeErasedInfoWithReturnOnly,
                                                        kEmptyInArgStorage,
                                                        kEmptyReturnStorage,
                                                        proxy_method_instance_identifier_,
                                                        method_call_handler_scope_,
                                                        kAllowedProxyUid,
                                                        kAsilLevel);
    // and given that the method call handler scope has expired
    method_call_handler_scope_.Expire();

    // Expecting that the registered type erased callback will not be called
    EXPECT_CALL(registered_type_erased_callback_, Call(_, _)).Times(0);

    // When the method call handler is called by the message passing (i.e. when a Proxy sends a message passing message
    // to call the method)
    ASSERT_TRUE(captured_method_call_handler_.has_value());
    std::invoke(captured_method_call_handler_.value(), kDummyQueuePosition);
}

using SkeletonMethodIsRegisteredFixture = SkeletonMethodFixture;
TEST_F(SkeletonMethodIsRegisteredFixture, IsRegisteredReturnsFalseIfRegisterHandlerNeverCalled)
{
    GivenASkeletonMethod();

    // When calling IsRegistered when no handler was ever registered
    const auto is_registered = unit_->IsRegistered();

    // Then the result should be false
    EXPECT_FALSE(is_registered);
}

TEST_F(SkeletonMethodIsRegisteredFixture, IsRegisteredReturnsTrueIfRegisterHandlerWasCalled)
{
    GivenASkeletonMethod().WithARegisteredCallback();

    // When calling IsRegistered when a handler was registered
    const auto is_registered = unit_->IsRegistered();

    // Then the result should be true
    EXPECT_TRUE(is_registered);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
