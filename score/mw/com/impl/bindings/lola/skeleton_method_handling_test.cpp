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
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/method_data.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/shared_memory_resource_mock.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>
#include <score/blank.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

/// \brief This file contains tests related to how a lola/Skeleton handles SkeletonMethods. Tests for
/// lola/SkeletonMethod are in skeleton_method_test.cpp.

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

constexpr auto kMethodChannelNameQm{"/lola-methods-0000000000000001-00016-06543-00005"};
constexpr auto kMethodChannelNameAsilB{"/lola-methods-0000000000000001-00016-06543-00006"};

const TypeErasedCallQueue::TypeErasedElementInfo kFooTypeErasedElementInfo{memory::DataTypeSizeInfo{32, 8},
                                                                           memory::DataTypeSizeInfo{64, 16},
                                                                           test::kFooMethodQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kDumbTypeErasedElementInfo{std::optional<memory::DataTypeSizeInfo>{},
                                                                            std::optional<memory::DataTypeSizeInfo>{},
                                                                            test::kDumbMethodQueueSize};

SkeletonBinding::SkeletonEventBindings kEmptyEventBindings{};
SkeletonBinding::SkeletonFieldBindings kEmptyFieldBindings{};

constexpr pid_t kDummyPid{15};
const auto kDummyQualityType = QualityType::kASIL_QM;

ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounterQm{5U};
ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounterAsilB{6U};

std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> kEmptyRegisterShmObjectTraceCallback{};

const std::vector<uid_t> kAllowedQmConsumers{test::kAllowedQmMethodConsumer};
const std::vector<uid_t> kAllowedAsilBConsumers{test::kAllowedAsilBMethodConsumer};

/// \brief Fake MethodData which simulates the MethodData which would be created by the Proxy side
///
/// This class owns its own memory resource which it uses to allocate and initialise MethodData and the contained
/// TypeErasedCallQueues. In order to use it in a test, a SharedMemoryFactoryMock should be used and the
/// getUsableBaseAddress should return a pointer to the method_data_ member. This is because the Proxy would normally
/// create a MethodData object at the start of the methods shared memory region.
class FakeMethodData
{
  public:
    FakeMethodData(std::vector<std::pair<LolaMethodId, TypeErasedCallQueue::TypeErasedElementInfo>> method_data)
        : memory_resource_{1000U}, method_data_{method_data.size(), memory_resource_}
    {
        for (auto& [method_id, type_erased_element_info] : method_data)
        {
            score::cpp::ignore = method_data_.method_call_queues_.emplace_back(
                std::piecewise_construct,
                std::forward_as_tuple(method_id),
                std::forward_as_tuple(*memory_resource_.getMemoryResourceProxy(), type_erased_element_info));
        }
    }

    memory::shared::test::MyBoundedMemoryResource memory_resource_;
    MethodData method_data_;
};

class SkeletonMethodHandlingFixture : public SkeletonMockedMemoryFixture
{
    void InitialiseSkeletonAndConstructMethods(const InstanceIdentifier& instance_identifier)
    {
        InitialiseSkeletonWithRealPathBuilders(instance_identifier);

        const ElementFqId foo_element_fq_id{
            test::kLolaServiceId, test::kFooMethodId, test::kDefaultLolaInstanceId, ServiceElementType::METHOD};
        const ElementFqId dumb_element_fq_id{
            test::kLolaServiceId, test::kDumbMethodId, test::kDefaultLolaInstanceId, ServiceElementType::METHOD};
        foo_method_ = std::make_unique<SkeletonMethod>(*skeleton_, foo_element_fq_id);
        dumb_method_ = std::make_unique<SkeletonMethod>(*skeleton_, dumb_element_fq_id);

        foo_method_->RegisterHandler(
            [this](std::optional<score::cpp::span<std::byte>> in_args, std::optional<score::cpp::span<std::byte>> return_arg) {
                std::invoke(foo_mock_type_erased_callback_.AsStdFunction(), in_args, return_arg);
            });
        dumb_method_->RegisterHandler(
            [this](std::optional<score::cpp::span<std::byte>> in_args, std::optional<score::cpp::span<std::byte>> return_arg) {
                std::invoke(dumb_mock_type_erased_callback_.AsStdFunction(), in_args, return_arg);
            });
    }

  public:
    SkeletonMethodHandlingFixture()
    {
        ON_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _))
            .WillByDefault(Return(mock_method_memory_resource_qm_));
        ON_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameAsilB, true, _))
            .WillByDefault(Return(mock_method_memory_resource_asil_b_));

        ON_CALL(*mock_method_memory_resource_2_, getUsableBaseAddress())
            .WillByDefault(Return(static_cast<void*>(&fake_method_data_2_.method_data_)));
        ON_CALL(*mock_method_memory_resource_qm_, getUsableBaseAddress())
            .WillByDefault(Return(static_cast<void*>(&fake_method_data_qm_.method_data_)));
        ON_CALL(*mock_method_memory_resource_asil_b_, getUsableBaseAddress())
            .WillByDefault(Return(static_cast<void*>(&fake_method_data_b_.method_data_)));

        ON_CALL(message_passing_mock_, RegisterMethodCallHandler(_, _, _, _))
            .WillByDefault(WithArgs<0, 1>(Invoke(
                [this](auto asil_level, auto proxy_method_instance_identifier) -> Result<MethodCallRegistrationGuard> {
                    return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                                      asil_level,
                                                                      proxy_method_instance_identifier,
                                                                      method_call_registration_guard_scope_);
                })));

        ON_CALL(message_passing_mock_, RegisterOnServiceMethodSubscribedHandler(_, _, _, _))
            .WillByDefault(WithArgs<0, 1>(Invoke([this](auto asil_level, auto skeleton_instance_identifier) {
                return MethodSubscriptionRegistrationGuardFactory::Create(message_passing_mock_,
                                                                          asil_level,
                                                                          skeleton_instance_identifier,
                                                                          method_call_registration_guard_scope_);
            })));
    }

    SkeletonMethodHandlingFixture& GivenASkeletonWithTwoMethods()
    {
        InitialiseSkeletonAndConstructMethods(GetValidInstanceIdentifierWithMethods());
        return *this;
    }

    SkeletonMethodHandlingFixture& GivenAnAsilBSkeletonWithTwoMethods()
    {
        InitialiseSkeletonAndConstructMethods(GetValidASILInstanceIdentifierWithMethods());
        return *this;
    }

    SkeletonMethodHandlingFixture& GivenASkeletonWithoutConfiguredMethods()
    {
        InitialiseSkeleton(GetValidInstanceIdentifier());
        return *this;
    }

    SkeletonMethodHandlingFixture& GivenAnAsilBSkeletonWithoutConfiguredMethods()
    {
        InitialiseSkeleton(GetValidASILInstanceIdentifier());
        return *this;
    }

    SkeletonMethodHandlingFixture& WhichCapturesRegisteredMethodSubscribedHandlers()
    {
        EXPECT_CALL(
            message_passing_mock_,
            RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _))
            .WillOnce(WithArgs<0, 1, 2>(
                Invoke([this](auto asil_level, auto skeleton_instance_identifier, auto method_subscribed_handler) {
                    captured_method_subscribed_handler_qm_.emplace(std::move(method_subscribed_handler));
                    return MethodSubscriptionRegistrationGuardFactory::Create(message_passing_mock_,
                                                                              asil_level,
                                                                              skeleton_instance_identifier,
                                                                              method_call_registration_guard_scope_);
                })));
        if (skeleton_->GetInstanceQualityType() == QualityType::kASIL_B)
        {
            EXPECT_CALL(
                message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
                .WillOnce(WithArgs<0, 1, 2>(
                    Invoke([this](auto asil_level, auto skeleton_instance_identifier, auto method_subscribed_handler) {
                        captured_method_subscribed_handler_b_.emplace(std::move(method_subscribed_handler));
                        return MethodSubscriptionRegistrationGuardFactory::Create(
                            message_passing_mock_,
                            asil_level,
                            skeleton_instance_identifier,
                            method_call_registration_guard_scope_);
                    })));
        }
        return *this;
    }

    SkeletonMethodHandlingFixture& WhichIsOffered()
    {
        score::cpp::ignore = skeleton_->PrepareOffer(
            kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));
        return *this;
    }

    ProxyInstanceIdentifier proxy_instance_identifier_qm_{kDummyApplicationId, kDummyProxyInstanceCounterQm};
    ProxyInstanceIdentifier proxy_instance_identifier_b_{kDummyApplicationId, kDummyProxyInstanceCounterAsilB};
    ProxyMethodInstanceIdentifier foo_proxy_method_identifier_qm_{proxy_instance_identifier_qm_, test::kFooMethodId};
    ProxyMethodInstanceIdentifier dumb_proxy_method_identifier_qm_{proxy_instance_identifier_qm_, test::kDumbMethodId};
    ProxyMethodInstanceIdentifier foo_proxy_method_identifier_b_{proxy_instance_identifier_b_, test::kFooMethodId};
    ProxyMethodInstanceIdentifier dumb_proxy_method_identifier_b_{proxy_instance_identifier_b_, test::kDumbMethodId};
    SkeletonInstanceIdentifier skeleton_instance_identifier_{test::kLolaServiceId, test::kDefaultLolaInstanceId};

    FakeMethodData fake_method_data_qm_{
        {{test::kFooMethodId, kFooTypeErasedElementInfo}, {test::kDumbMethodId, kDumbTypeErasedElementInfo}}};
    FakeMethodData fake_method_data_b_{
        {{test::kFooMethodId, kFooTypeErasedElementInfo}, {test::kDumbMethodId, kDumbTypeErasedElementInfo}}};
    FakeMethodData fake_method_data_2_{
        {{test::kFooMethodId, kFooTypeErasedElementInfo}, {test::kDumbMethodId, kDumbTypeErasedElementInfo}}};

    std::unique_ptr<SkeletonMethod> foo_method_{nullptr};
    std::unique_ptr<SkeletonMethod> dumb_method_{nullptr};

    std::shared_ptr<NiceMock<memory::shared::SharedMemoryResourceMock>> mock_method_memory_resource_qm_{
        std::make_shared<NiceMock<memory::shared::SharedMemoryResourceMock>>()};
    std::shared_ptr<NiceMock<memory::shared::SharedMemoryResourceMock>> mock_method_memory_resource_asil_b_{
        std::make_shared<NiceMock<memory::shared::SharedMemoryResourceMock>>()};
    std::shared_ptr<NiceMock<memory::shared::SharedMemoryResourceMock>> mock_method_memory_resource_2_{
        std::make_shared<NiceMock<memory::shared::SharedMemoryResourceMock>>()};

    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> foo_mock_type_erased_callback_{};
    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> dumb_mock_type_erased_callback_{};
    std::optional<IMessagePassingService::ServiceMethodSubscribedHandler> captured_method_subscribed_handler_qm_{};
    std::optional<IMessagePassingService::ServiceMethodSubscribedHandler> captured_method_subscribed_handler_b_{};

    safecpp::Scope<> method_call_registration_guard_scope_{};
};

using SkeletonPrepareOfferFixture = SkeletonMethodHandlingFixture;
TEST_F(SkeletonPrepareOfferFixture, PrepareOfferWillRegisterServiceMethodSubscribedHandler)
{
    GivenASkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing for QM only which returns a
    // valid result
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
        .Times(0);

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferOnAsilBSkeletonWillRegisterQmAndAsilBServiceMethodSubscribedHandler)
{
    GivenAnAsilBSkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing for QM and Asil B which
    // returns a valid result
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _));

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferReturnsErrorIfRegisterServiceMethodSubscribedHandlerReturnsError)
{
    GivenAnAsilBSkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing which returns an error
    const auto error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then an error is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferReturnsErrorIfAsilBRegisterServiceMethodSubscribedHandlerReturnsError)
{
    GivenAnAsilBSkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing for QM which returns blank
    // and ASIL B which returns an error
    const auto error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then an error is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error_code);
}

TEST_F(SkeletonPrepareOfferFixture, FailingToGetBindingRuntimeInPrepareOfferTerminates)
{
    GivenASkeletonWithTwoMethods();

    // Expecting that trying to get the lola binding runtime returns an nullptr
    EXPECT_CALL(runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillOnce(Return(nullptr));

    // When calling PrepareOffer
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        score::cpp::ignore = skeleton_->PrepareOffer(
            kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback)));
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferWillNotRegisterServiceMethodSubscribedHandlerWhenNoMethodsExistQm)
{
    GivenASkeletonWithoutConfiguredMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is not called on message passing for QM or ASIL-B
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _))
        .Times(0);
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
        .Times(0);

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferWillNotRegisterServiceMethodSubscribedHandlerWhenNoMethodsExistAsilB)
{
    GivenAnAsilBSkeletonWithoutConfiguredMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is not called on message passing for QM or ASIL-B
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _))
        .Times(0);
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
        .Times(0);

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferWillNotCallUnregisterSubscribedMethodHandler)
{
    GivenAnAsilBSkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler will be called for QM and ASIL-B
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _));

    // Expecting that UnregisterOnServiceMethodSubscribedHandler will not be called for each method for QM and ASIL-B
    EXPECT_CALL(message_passing_mock_, UnregisterOnServiceMethodSubscribedHandler(_, _)).Times(0);

    // When calling PrepareOffer
    score::cpp::ignore = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));
}

TEST_F(SkeletonPrepareOfferFixture, CallingAsilBWillUnregisterQmHandlerOnAsilBRegistrationFailure)
{
    GivenAnAsilBSkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler will be called for QM and ASIL-B which succeeds for QM
    // but fails for ASIL-B
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kCallQueueFull))));

    // Expecting that UnregisterOnServiceMethodSubscribedHandler will be called for method for QM
    EXPECT_CALL(message_passing_mock_,
                UnregisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_));

    // When calling PrepareOffer
    score::cpp::ignore = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));
}

using SkeletonPrepareStopOfferFixture = SkeletonMethodHandlingFixture;
TEST_F(SkeletonPrepareStopOfferFixture, PrepareStopOfferExpiresScopeOfMethodCallHandlers)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a method call handler is registered for both methods
    std::optional<IMessagePassingService::MethodCallHandler> method_call_handler_1{};
    std::optional<IMessagePassingService::MethodCallHandler> method_call_handler_2{};
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(kDummyQualityType, foo_proxy_method_identifier_qm_, _, _))
        .WillOnce(WithArgs<2>(Invoke([this, &method_call_handler_1](auto method_call_handler) {
            method_call_handler_1.emplace(method_call_handler);
            return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                              kDummyQualityType,
                                                              foo_proxy_method_identifier_qm_,
                                                              method_call_registration_guard_scope_);
        })));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(kDummyQualityType, dumb_proxy_method_identifier_qm_, _, _))
        .WillOnce(WithArgs<2>(Invoke([this, &method_call_handler_2](auto method_call_handler) {
            method_call_handler_2.emplace(method_call_handler);
            return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                              kDummyQualityType,
                                                              dumb_proxy_method_identifier_qm_,
                                                              method_call_registration_guard_scope_);
        })));

    // and given that the registered method subscribed handler is called
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // and given that PrepareStopOffer was called
    skeleton_->PrepareStopOffer({});

    // When calling the method call handlers
    const auto method_call_handler_result_1 = std::invoke(method_call_handler_1.value(), 0U);
    const auto method_call_handler_result_2 = std::invoke(method_call_handler_1.value(), 0U);

    // Then both call results will contain errors indicating that the scope has expired
    EXPECT_FALSE(method_call_handler_result_1.has_value());
    EXPECT_FALSE(method_call_handler_result_2.has_value());
}

TEST_F(SkeletonPrepareStopOfferFixture, PrepareStopOfferExpiresScopeOfSubscribeMethodHandler)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // and given that PrepareStopOffer was called
    skeleton_->PrepareStopOffer({});

    // When calling a ServiceMethodSubscribedHandler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto subscribe_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                      proxy_instance_identifier_qm_,
                                                      test::kAllowedQmMethodConsumer,
                                                      kDummyPid);

    // Then the result will contain an error indicating that the scope has expired
    EXPECT_FALSE(subscribe_handler_result.has_value());
}

TEST_F(SkeletonPrepareStopOfferFixture, PrepareStopOfferDestroysPointerToSharedMemory)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // When calling the registered method subscribed handler which will open the shared memory region
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling PrepareStopOffer
    const auto shm_resource_ref_counter_after_opening = mock_method_memory_resource_qm_.use_count();
    skeleton_->PrepareStopOffer({});

    // Then the reference counter for the methods SharedMemoryResource should be decremented, indicating that it's
    // been deleted from the Skeleton's state
    EXPECT_EQ(mock_method_memory_resource_qm_.use_count(), shm_resource_ref_counter_after_opening - 1U);
}

TEST_F(SkeletonPrepareStopOfferFixture, UnregistersQmAndAsilBSubscribedMethodHandlers)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichIsOffered();

    // Expecting that UnregisterOnServiceMethodSubscribedHandler will be called for method for QM and Asil-B
    EXPECT_CALL(message_passing_mock_,
                UnregisterOnServiceMethodSubscribedHandler(QualityType::kASIL_QM, skeleton_instance_identifier_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterOnServiceMethodSubscribedHandler(QualityType::kASIL_B, skeleton_instance_identifier_));

    // When calling PrepareStopOffer
    skeleton_->PrepareStopOffer({});
}

TEST_F(SkeletonPrepareStopOfferFixture, UnregistersAllRegisteredMethodCallHandlers)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that UnregisterMethodCallHandler will be called for each method for QM and ASIL-B
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, foo_proxy_method_identifier_qm_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_));

    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, foo_proxy_method_identifier_b_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, dumb_proxy_method_identifier_b_));

    // and given that the registered method subscribed handler was called for both QM and AsilB
    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   kDummyPid);
    EXPECT_TRUE(scoped_handler_result.has_value());
    const auto scoped_handler_result_2 = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                     proxy_instance_identifier_b_,
                                                     test::kAllowedAsilBMethodConsumer,
                                                     kDummyPid);
    EXPECT_TRUE(scoped_handler_result_2.has_value());

    // When calling PrepareStopOffer
    skeleton_->PrepareStopOffer({});
}

using SkeletonOnServiceMethodsSubscribedFixture = SkeletonMethodHandlingFixture;
TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingRegistersMethodCallHandlerWithQualityTypeOfMessagePassingInstanceQmOnly)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler is called with ASIL level QM for both methods which return valid
    // results
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_QM, foo_proxy_method_identifier_qm_, _, test::kAllowedQmMethodConsumer));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_, _, test::kAllowedQmMethodConsumer));

    // When calling the registered Qm method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the handler should return a valid result
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_TRUE(scoped_handler_result->has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingRegistersMethodCallHandlerWithQualityTypeOfMessagePassingInstanceQmAndAsilB)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler is called with both QM and ASIL-B levels for both methods which
    // return valid results
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_QM, foo_proxy_method_identifier_qm_, _, test::kAllowedQmMethodConsumer));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_, _, test::kAllowedQmMethodConsumer));

    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_B, foo_proxy_method_identifier_b_, _, test::kAllowedAsilBMethodConsumer));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(
                    QualityType::kASIL_B, dumb_proxy_method_identifier_b_, _, test::kAllowedAsilBMethodConsumer));

    // When calling the registered Qm and ASIL-B method subscribed handlers
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result_qm = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                      proxy_instance_identifier_qm_,
                                                      test::kAllowedQmMethodConsumer,
                                                      kDummyPid);
    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result_b = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                     proxy_instance_identifier_b_,
                                                     test::kAllowedAsilBMethodConsumer,
                                                     kDummyPid);

    // Then both handlers should return valid results
    ASSERT_TRUE(scoped_handler_result_qm.has_value());
    ASSERT_TRUE(scoped_handler_result_qm->has_value());

    ASSERT_TRUE(scoped_handler_result_b.has_value());
    ASSERT_TRUE(scoped_handler_result_b->has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorIfRegisteringMethodCallHandlerReturnedError)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler is called on the first method which returns an error
    const auto error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(kDummyQualityType, foo_proxy_method_identifier_qm_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(error_code))));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the handler should return an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), error_code);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingOpensShmIfAlreadyCalledWithDifferentApplicationIdAndSamePid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{123, kDummyProxyInstanceCounterQm};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-00123-00005"};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _));
    EXPECT_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillOnce(Return(mock_method_memory_resource_2_));

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceCounter and PID but a different application ID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingOpensShmIfAlreadyCalledWithDifferentProxyInstanceCounterAndSamePid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyApplicationId, 15U};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-06543-00015"};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _));
    EXPECT_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillOnce(Return(mock_method_memory_resource_2_));

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // application ID and PID but a different ProxyInstanceCounter
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingOpensShmIfAlreadyCalledWithSameProxyInstanceIdentifierAndDifferentPid)
{
    const pid_t pid_2{25U};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler with the same path
    // (the first region will be cleaned up in the second call, but this is tested in a different test).
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _)).Times(2);

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier but a different PID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   pid_2);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingDoesNotOpenShmIfAlreadyCalledWithSameProxyInstanceIdAndPid)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will only be opened once
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _)).Times(1);

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier and the same PID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingRemovesOldRegionsFromCallWithSameApplicationIdAndDifferentPid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyApplicationId, 15U};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-06543-00015"};
    const pid_t pid_2{25U};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Given that the second shared memory region will be opened which returns a valid resource
    ON_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillByDefault(Return(mock_method_memory_resource_2_));

    const auto first_initial_shm_resource_ref_counter = mock_method_memory_resource_qm_.use_count();

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier and a different PID
    EXPECT_EQ(mock_method_memory_resource_qm_.use_count(), first_initial_shm_resource_ref_counter + 1U);
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   pid_2);

    // Then the reference counter for the first methods SharedMemoryResource should be have been decremented,
    // indicating that it's been removed from the Skeleton's state
    EXPECT_EQ(mock_method_memory_resource_qm_.use_count(), first_initial_shm_resource_ref_counter);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingRegistersAMethodCallHandlerPerMethodWithInfoFromMethodData)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that the type erased callback will be called for each method with InArgs and ReturnArg storage
    // provided if TypeErasedElementInfo for the method in MethodData contains InArgs / a ReturnArg
    EXPECT_CALL(foo_mock_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_args_optional, auto result_optional) {
            EXPECT_EQ(in_args_optional.has_value(), kFooTypeErasedElementInfo.in_arg_type_info.has_value());
            EXPECT_EQ(result_optional.has_value(), kFooTypeErasedElementInfo.return_type_info.has_value());
        }));
    EXPECT_CALL(dumb_mock_type_erased_callback_, Call(_, _))
        .WillOnce(Invoke([](auto in_args_optional, auto result_optional) {
            EXPECT_EQ(in_args_optional.has_value(), kDumbTypeErasedElementInfo.in_arg_type_info.has_value());
            EXPECT_EQ(result_optional.has_value(), kDumbTypeErasedElementInfo.return_type_info.has_value());
        }));

    // Expecting that a method call handler is registered for both methods which calls the handler directly with the
    // largest possible queue index for that method
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(kDummyQualityType, foo_proxy_method_identifier_qm_, _, _))
        .WillOnce(WithArgs<2>(Invoke([this](auto method_call_handler) {
            std::invoke(method_call_handler, test::kFooMethodQueueSize - 1U);
            return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                              kDummyQualityType,
                                                              foo_proxy_method_identifier_qm_,
                                                              method_call_registration_guard_scope_);
        })));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(kDummyQualityType, dumb_proxy_method_identifier_qm_, _, _))
        .WillOnce(WithArgs<2>(Invoke([this](auto method_call_handler) {
            std::invoke(method_call_handler, test::kDumbMethodQueueSize - 1U);
            return MethodCallRegistrationGuardFactory::Create(message_passing_mock_,
                                                              kDummyQualityType,
                                                              dumb_proxy_method_identifier_qm_,
                                                              method_call_registration_guard_scope_);
        })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingQmOpensSharedMemoryWithProxyUidAsAllowedProvider)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will be opened with the proxy's uid from the configuration in the
    // allowed provider list
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _))
        .WillOnce(WithArgs<2>(
            Invoke([this](auto allowed_providers) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_TRUE(allowed_providers.has_value());
                EXPECT_EQ(allowed_providers.value().size(), 1U);
                EXPECT_EQ(allowed_providers.value()[0], test::kAllowedQmMethodConsumer);
                return mock_method_memory_resource_qm_;
            })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilBOpensSharedMemoryWithProxyUidAsAllowedProvider)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will be opened with the proxy's uid from the configuration in the
    // allowed provider list
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameAsilB, true, _))
        .WillOnce(WithArgs<2>(
            Invoke([this](auto allowed_providers) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_TRUE(allowed_providers.has_value());
                EXPECT_EQ(allowed_providers.value().size(), 1U);
                EXPECT_EQ(allowed_providers.value()[0], test::kAllowedAsilBMethodConsumer);
                return mock_method_memory_resource_qm_;
            })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                   proxy_instance_identifier_b_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingStoresSharedMemoryInClassState)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    const auto initial_shm_resource_ref_counter = mock_method_memory_resource_qm_.use_count();

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                              proxy_instance_identifier_qm_,
                              test::kAllowedQmMethodConsumer,
                              kDummyPid);

    // Then the reference counter for the methods SharedMemoryResource should be incremented, indicating that it's
    // been stored in the Skeleton's state
    EXPECT_EQ(mock_method_memory_resource_qm_.use_count(), initial_shm_resource_ref_counter + 1U);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, FailingToOpenSharedMemoryReturnsError)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will be opened which returns a nullptr
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _)).WillOnce(Return(nullptr));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the handler should return an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, FailingToGetUsableBaseAddressForRetrievingMethodDataTerminates)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that getUsableBaseAddress is called on the methods shared memory resource which returns an error
    EXPECT_CALL(*mock_method_memory_resource_qm_, getUsableBaseAddress()).WillOnce(Return(nullptr));

    // When calling the registered method subscribed handler
    // Then the program terminates
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                           proxy_instance_identifier_qm_,
                                                           test::kAllowedQmMethodConsumer,
                                                           kDummyPid));
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilQmWithoutInArgsOrReturnStillOpensSharedMemory)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will be opened
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameQm, true, _)).Times(1);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilBWithoutInArgsOrReturnStillOpensSharedMemory)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that a shared memory region will be opened
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelNameAsilB, true, _)).Times(1);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                   proxy_instance_identifier_b_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilBWillNotCallUnregisterMethodCallHandler)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler will be called for each method for QM and ASIL-B
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, foo_proxy_method_identifier_qm_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_, _, _));

    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, foo_proxy_method_identifier_b_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, dumb_proxy_method_identifier_b_, _, _));

    // Expecting that UnregisterMethodCallHandler will not be called for each method for QM and ASIL-B
    EXPECT_CALL(message_passing_mock_, UnregisterMethodCallHandler(_, _)).Times(0);

    // When calling the registered method subscribed handler for both QM and AsilB
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);
    EXPECT_TRUE(scoped_handler_result.has_value());

    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result_2 = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                     proxy_instance_identifier_b_,
                                                     test::kAllowedAsilBMethodConsumer,
                                                     kDummyPid);
    EXPECT_TRUE(scoped_handler_result_2.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingWillUnregisterRegisteredMethodCallHandlersOnSubscriptionError)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler will be called for each method which fails on the second call
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, foo_proxy_method_identifier_qm_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kBindingFailure))));

    // Expecting that UnregisterMethodCallHandler will be called only for the method which was successfully registered
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, foo_proxy_method_identifier_qm_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_QM, dumb_proxy_method_identifier_qm_))
        .Times(0);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_qm_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_qm_.value(),
                                                   proxy_instance_identifier_qm_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingAsilBWillUnregisterRegisteredMethodCallHandlersOnSubscriptionError)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandlers().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler will be called for each method which fails on the second call
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, foo_proxy_method_identifier_b_, _, _));
    EXPECT_CALL(message_passing_mock_,
                RegisterMethodCallHandler(QualityType::kASIL_B, dumb_proxy_method_identifier_b_, _, _))
        .WillOnce(Return(ByMove(MakeUnexpected(ComErrc::kBindingFailure))));

    // Expecting that UnregisterMethodCallHandler will be called only for the method which was successfully registered
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, foo_proxy_method_identifier_b_));
    EXPECT_CALL(message_passing_mock_,
                UnregisterMethodCallHandler(QualityType::kASIL_B, dumb_proxy_method_identifier_b_))
        .Times(0);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_b_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_b_.value(),
                                                   proxy_instance_identifier_b_,
                                                   test::kAllowedQmMethodConsumer,
                                                   kDummyPid);
    EXPECT_TRUE(scoped_handler_result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
