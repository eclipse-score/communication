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

constexpr auto kMethodChannelName{"/lola-methods-0000000000000001-00016-06543-00005"};

const TypeErasedCallQueue::TypeErasedElementInfo kFooTypeErasedElementInfo{memory::DataTypeSizeInfo{32, 8},
                                                                           memory::DataTypeSizeInfo{64, 16},
                                                                           test::kFooMethodQueueSize};
const TypeErasedCallQueue::TypeErasedElementInfo kDumbTypeErasedElementInfo{std::optional<memory::DataTypeSizeInfo>{},
                                                                            std::optional<memory::DataTypeSizeInfo>{},
                                                                            test::kDumbMethodQueueSize};

SkeletonBinding::SkeletonEventBindings kEmptyEventBindings{};
SkeletonBinding::SkeletonFieldBindings kEmptyFieldBindings{};

constexpr pid_t kDummyPid{15};

ProxyInstanceIdentifier::ProxyInstanceCounter kDummyProxyInstanceCounter{5U};

std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> kEmptyRegisterShmObjectTraceCallback{};

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
        ON_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _))
            .WillByDefault(Return(mock_method_memory_resource_));

        ON_CALL(*mock_method_memory_resource_2_, getUsableBaseAddress())
            .WillByDefault(Return(static_cast<void*>(&fake_method_data_2_.method_data_)));
        ON_CALL(*mock_method_memory_resource_, getUsableBaseAddress())
            .WillByDefault(Return(static_cast<void*>(&fake_method_data_.method_data_)));
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

    SkeletonMethodHandlingFixture& WhichCapturesRegisteredMethodSubscribedHandler()
    {
        ON_CALL(message_passing_mock_, RegisterOnServiceMethodSubscribedHandler(skeleton_instance_identifier_, _))
            .WillByDefault(WithArg<1>(Invoke([this](auto method_subscribed_handler) -> ResultBlank {
                captured_method_subscribed_handler_.emplace(std::move(method_subscribed_handler));
                return {};
            })));
        return *this;
    }

    SkeletonMethodHandlingFixture& WhichIsOffered()
    {
        score::cpp::ignore = skeleton_->PrepareOffer(
            kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));
        return *this;
    }

    ProxyInstanceIdentifier proxy_instance_identifier_{kDummyApplicationId, kDummyProxyInstanceCounter};
    SkeletonInstanceIdentifier skeleton_instance_identifier_{test::kLolaServiceId, test::kDefaultLolaInstanceId};

    FakeMethodData fake_method_data_{
        {{test::kFooMethodId, kFooTypeErasedElementInfo}, {test::kDumbMethodId, kDumbTypeErasedElementInfo}}};
    FakeMethodData fake_method_data_2_{
        {{test::kFooMethodId, kFooTypeErasedElementInfo}, {test::kDumbMethodId, kDumbTypeErasedElementInfo}}};

    std::unique_ptr<SkeletonMethod> foo_method_{nullptr};
    std::unique_ptr<SkeletonMethod> dumb_method_{nullptr};

    std::shared_ptr<NiceMock<memory::shared::SharedMemoryResourceMock>> mock_method_memory_resource_{
        std::make_shared<NiceMock<memory::shared::SharedMemoryResourceMock>>()};
    std::shared_ptr<NiceMock<memory::shared::SharedMemoryResourceMock>> mock_method_memory_resource_2_{
        std::make_shared<NiceMock<memory::shared::SharedMemoryResourceMock>>()};

    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> foo_mock_type_erased_callback_{};
    MockFunction<SkeletonMethodBinding::TypeErasedCallbackSignature> dumb_mock_type_erased_callback_{};
    std::optional<IMessagePassingService::ServiceMethodSubscribedHandler> captured_method_subscribed_handler_{};
};

using SkeletonPrepareOfferFixture = SkeletonMethodHandlingFixture;
TEST_F(SkeletonPrepareOfferFixture, PrepareOfferWillRegisterServiceMethodSubscribedHandler)
{
    GivenASkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing which returns a valid
    // result
    EXPECT_CALL(message_passing_mock_, RegisterOnServiceMethodSubscribedHandler(skeleton_instance_identifier_, _))
        .WillOnce(Return(score::cpp::blank{}));

    // When calling PrepareOffer
    const auto result = skeleton_->PrepareOffer(
        kEmptyEventBindings, kEmptyFieldBindings, std::move(kEmptyRegisterShmObjectTraceCallback));

    // Then a valid result is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(SkeletonPrepareOfferFixture, PrepareOfferReturnsErrorIfRegisterServiceMethodSubscribedHandlerReturnsError)
{
    GivenASkeletonWithTwoMethods();

    // Expecting that RegisterOnServiceMethodSubscribedHandler is called on message passing which returns an error
    const auto error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(message_passing_mock_, RegisterOnServiceMethodSubscribedHandler(skeleton_instance_identifier_, _))
        .WillOnce(Return(MakeUnexpected(error_code)));

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

using SkeletonOnServiceMethodsSubscribedFixture = SkeletonMethodHandlingFixture;
TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsValidWhenQmProxyUidIsInAllowedConsumers)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with a QM uid that is in the configuration's
    // allowed_consumer list
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsValidWhenAsilBProxyUidIsInAllowedConsumers)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with an ASIL-B uid that is in the configuration's
    // allowed_consumer list
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   QualityType::kASIL_B,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorWhenNoQmAllowedConsumersAreInConfiguration)
{
    GivenASkeletonWithoutConfiguredMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with a QM uid when there is no QM allowed consumer list
    // in the configuration
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should contain an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorWhenNoAsilBAllowedConsumersAreInConfiguration)
{
    GivenASkeletonWithoutConfiguredMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with an ASIL-B uid when there is no ASIL-B allowed
    // consumer list in the configuration
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should contain an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorWhenQmProxyUidIsNotInAllowedConsumers)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with a QM uid that is not in the QM allowed consumer
    // list in the configuration
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should contain an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorWhenAsilBProxyUidIsNotInAllowedConsumers)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // When calling the registered method subscribed handler with an ASIL-B uid that is not in the ASIL-B allowed
    // consumer list in the configuration
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_B,
                                                   kDummyPid);

    // Then the result should contain an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingReturnsErrorIfRegisteringMethodCallHandlerReturnedError)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that RegisterMethodCallHandler is called on the first method which returns an error
    const auto error_code = ComErrc::kCommunicationLinkError;
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(proxy_instance_identifier_, _))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the handler should return an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), error_code);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingOpensShmIfAlreadyCalledWithDifferentApplicationIdAndSamePid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{123, kDummyProxyInstanceCounter};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-00123-00005"};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _));
    EXPECT_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillOnce(Return(mock_method_memory_resource_2_));

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceCounter and PID but a different application ID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingOpensShmIfAlreadyCalledWithDifferentProxyInstanceCounterAndSamePid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyApplicationId, 15U};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-06543-00015"};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _));
    EXPECT_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillOnce(Return(mock_method_memory_resource_2_));

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // application ID and PID but a different ProxyInstanceCounter
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture,
       CallingOpensShmIfAlreadyCalledWithSameProxyInstanceIdentifierAndDifferentPid)
{
    const pid_t pid_2{25U};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a different shared memory region will be opened in each call to the handler with the same path
    // (the first region will be cleaned up in the second call, but this is tested in a different test).
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _)).Times(2);

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier but a different PID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   pid_2);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingDoesNotOpenShmIfAlreadyCalledWithSameProxyInstanceIdAndPid)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will only be opened once
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _)).Times(1);

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier and the same PID
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingRemovesOldRegionsFromCallWithSameApplicationIdAndDifferentPid)
{
    const ProxyInstanceIdentifier proxy_instance_identifier_2{kDummyApplicationId, 15U};
    constexpr auto method_channel_name_2{"/lola-methods-0000000000000001-00016-06543-00015"};
    const pid_t pid_2{25U};

    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Given that the second shared memory region will be opened which returns a valid resource
    ON_CALL(shared_memory_factory_mock_, Open(method_channel_name_2, true, _))
        .WillByDefault(Return(mock_method_memory_resource_2_));

    const auto first_initial_shm_resource_ref_counter = mock_method_memory_resource_.use_count();

    // Given that the registered method subscribed handler was called once
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // When calling the registered method subscribed handler with a ProxyInstanceIdentifier containing the same
    // ProxyInstanceIdentifier and a different PID
    EXPECT_EQ(mock_method_memory_resource_.use_count(), first_initial_shm_resource_ref_counter + 1U);
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_2,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   pid_2);

    // Then the reference counter for the first methods SharedMemoryResource should be have been decremented, indicating
    // that it's been removed from the Skeleton's state
    EXPECT_EQ(mock_method_memory_resource_.use_count(), first_initial_shm_resource_ref_counter);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingRegistersAMethodCallHandlerPerMethodWithInfoFromMethodData)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that the type erased callback will be called for each method with InArgs and ReturnArg storage provided
    // if TypeErasedElementInfo for the method in MethodData contains InArgs / a ReturnArg
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
    EXPECT_CALL(message_passing_mock_, RegisterMethodCallHandler(proxy_instance_identifier_, _))
        .Times(2)
        .WillOnce(WithArgs<1>(Invoke([](auto method_call_handler) -> ResultBlank {
            std::invoke(method_call_handler, test::kFooMethodQueueSize - 1U);
            return {};
        })))
        .WillOnce(WithArgs<1>(Invoke([](auto method_call_handler) -> ResultBlank {
            std::invoke(method_call_handler, test::kDumbMethodQueueSize - 1U);
            return {};
        })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingQmOpensSharedMemoryWithProxyUidAsAllowedProvider)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will be opened with the proxy's uid from the configuration in the allowed
    // provider list
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _))
        .WillOnce(WithArgs<2>(
            Invoke([this](auto allowed_providers) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_TRUE(allowed_providers.has_value());
                EXPECT_EQ(allowed_providers.value().size(), 1U);
                EXPECT_EQ(allowed_providers.value()[0], test::kAllowedQmMethodConsumer);
                return mock_method_memory_resource_;
            })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilBOpensSharedMemoryWithProxyUidAsAllowedProvider)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will be opened with the proxy's uid from the configuration in the allowed
    // provider list
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _))
        .WillOnce(WithArgs<2>(
            Invoke([this](auto allowed_providers) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_TRUE(allowed_providers.has_value());
                EXPECT_EQ(allowed_providers.value().size(), 1U);
                EXPECT_EQ(allowed_providers.value()[0], test::kAllowedAsilBMethodConsumer);
                return mock_method_memory_resource_;
            })));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   QualityType::kASIL_B,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingStoresSharedMemoryInClassState)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    const auto initial_shm_resource_ref_counter = mock_method_memory_resource_.use_count();

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                              proxy_instance_identifier_,
                              test::kAllowedQmMethodConsumer,
                              QualityType::kASIL_QM,
                              kDummyPid);

    // Then the reference counter for the methods SharedMemoryResource should be incremented, indicating that it's
    // been stored in the Skeleton's state
    EXPECT_EQ(mock_method_memory_resource_.use_count(), initial_shm_resource_ref_counter + 1U);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, FailingToOpenSharedMemoryReturnsError)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will be opened which returns a nullptr
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _)).WillOnce(Return(nullptr));

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the handler should return an error
    ASSERT_TRUE(scoped_handler_result.has_value());
    ASSERT_FALSE(scoped_handler_result->has_value());
    EXPECT_EQ(scoped_handler_result->error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, FailingToGetUsableBaseAddressForRetrievingMethodDataTerminates)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that getUsableBaseAddress is called on the methods shared memory resource which returns an error
    EXPECT_CALL(*mock_method_memory_resource_, getUsableBaseAddress()).WillOnce(Return(nullptr));

    // When calling the registered method subscribed handler
    // Then the program terminates
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = std::invoke(captured_method_subscribed_handler_.value(),
                                                           proxy_instance_identifier_,
                                                           test::kAllowedQmMethodConsumer,
                                                           QualityType::kASIL_QM,
                                                           kDummyPid));
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilQmWithoutInArgsOrReturnStillOpensSharedMemory)
{
    GivenASkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will be opened
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _)).Times(1);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedQmMethodConsumer,
                                                   QualityType::kASIL_QM,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

TEST_F(SkeletonOnServiceMethodsSubscribedFixture, CallingAsilBWithoutInArgsOrReturnStillOpensSharedMemory)
{
    GivenAnAsilBSkeletonWithTwoMethods().WhichCapturesRegisteredMethodSubscribedHandler().WhichIsOffered();

    // Expecting that a shared memory region will be opened
    EXPECT_CALL(shared_memory_factory_mock_, Open(kMethodChannelName, true, _)).Times(1);

    // When calling the registered method subscribed handler
    ASSERT_TRUE(captured_method_subscribed_handler_.has_value());
    const auto scoped_handler_result = std::invoke(captured_method_subscribed_handler_.value(),
                                                   proxy_instance_identifier_,
                                                   test::kAllowedAsilBMethodConsumer,
                                                   QualityType::kASIL_B,
                                                   kDummyPid);

    // Then the result should be valid
    EXPECT_TRUE(scoped_handler_result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
