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
#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/test/proxy_resources.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestEventOrFieldType = std::uint16_t;

const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooservice{make_ServiceIdentifierType("foo")};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/dummy_instance_specifier"}).value();
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooservice,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{10U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};

class DummyProxy : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;
    bool AreBindingsValid() const noexcept
    {
        return ProxyBase::AreBindingsValid();
    }
};

const auto kEventName{"DummyEvent1"};
const auto kEventName2{"DummyEvent2"};

template <typename T>
class ProxyEventBaseFixture : public ::testing::Test
{
  public:
    using ServiceElementType = typename T::ServiceElementType;
    using MockServiceElementType = typename T::MockServiceElementType;

    void CreateServiceElement()
    {
        auto mock_service_element_binding_ptr = std::make_unique<MockServiceElementType>();
        mock_service_element_binding_ = mock_service_element_binding_ptr.get();

        service_element_ =
            std::make_unique<ServiceElementType>(empty_proxy_, std::move(mock_service_element_binding_ptr), kEventName);

        ON_CALL(*mock_service_element_binding_, SetReceiveHandler(_)).WillByDefault(Return(score::ResultBlank{}));
    }

    DummyProxy empty_proxy_{std::make_unique<mock_binding::Proxy>(),
                            make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment))};
    std::unique_ptr<ServiceElementType> service_element_{nullptr};
    MockServiceElementType* mock_service_element_binding_{nullptr};
};

struct ProxyEventStruct
{
    using ServiceElementType = ProxyEvent<TestEventOrFieldType>;
    using MockServiceElementType = mock_binding::ProxyEvent<TestEventOrFieldType>;
};

struct GenericProxyEventStruct
{
    using ServiceElementType = GenericProxyEvent;
    using MockServiceElementType = mock_binding::GenericProxyEvent;
};

struct ProxyFieldStruct
{
    using ServiceElementType = ProxyField<TestEventOrFieldType>;
    using MockServiceElementType = mock_binding::ProxyEvent<TestEventOrFieldType>;
};

// Gtest will run all tests in the LolaProxyEventFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::Types<ProxyEventStruct, GenericProxyEventStruct, ProxyFieldStruct>;
TYPED_TEST_SUITE(ProxyEventBaseFixture, MyTypes, );

template <typename T>
using ProxyEventBaseCreationFixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseCreationFixture, MyTypes, );

template <typename T>
using ProxyEventBaseUnsubscribeFixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseUnsubscribeFixture, MyTypes, );

template <typename T>
using ProxyEventBaseUnsubscribeDeathTest = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseUnsubscribeDeathTest, MyTypes, );

template <typename T>
using ProxyEventBaseSubscribeFixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseSubscribeFixture, MyTypes, );

template <typename T>
using ProxyEventBaseGetSubscriptionStatefixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseGetSubscriptionStatefixture, MyTypes, );

template <typename T>
using ProxyEventBaseGetFreeSampleCountFixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseGetFreeSampleCountFixture, MyTypes, );

template <typename T>
using ProxyEventBaseGetNumNewSamplesAvailableFixture = ProxyEventBaseFixture<T>;
TYPED_TEST_SUITE(ProxyEventBaseGetNumNewSamplesAvailableFixture, MyTypes, );

TEST(ProxyEventBaseTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-14137269");
    RecordProperty("Description", "Checks copy semantics for ProxyEventBases");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<ProxyEventBase>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<ProxyEventBase>::value, "Is wrongly copyable");
}

TEST(ProxyEventBaseTest, IsMoveable)
{
    static_assert(std::is_move_constructible<ProxyEventBase>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<ProxyEventBase>::value, "Is not move assignable");
}

TYPED_TEST(ProxyEventBaseCreationFixture, CreatingServiceElementWithValidEventBindingMarksProxyBindingValid)
{
    auto valid_proxy_event_binding =
        std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::MockServiceElementType>();

    // Given a proxy with a valid binding
    DummyProxy dummy_proxy(std::make_unique<mock_binding::Proxy>(),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // When creating a ProxyEvent with a valid binding
    auto service_element = std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::ServiceElementType>(
        dummy_proxy, std::move(valid_proxy_event_binding), kEventName);

    // Then the proxy should report that all bindings are valid
    EXPECT_TRUE(dummy_proxy.AreBindingsValid());
}

TYPED_TEST(ProxyEventBaseCreationFixture, CreatingServiceElementWithInvalidEventBindingMarksProxyBindingInvalid)
{
    // Given a proxy with a valid binding
    DummyProxy dummy_proxy(std::make_unique<mock_binding::Proxy>(),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // When creating a ProxyEvent with an invalid binding
    auto service_element = std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::ServiceElementType>(
        dummy_proxy,
        std::unique_ptr<typename ProxyEventBaseCreationFixture<TypeParam>::MockServiceElementType>(nullptr),
        kEventName);

    // Then the proxy should report that all bindings are not valid
    EXPECT_FALSE(dummy_proxy.AreBindingsValid());
}

TYPED_TEST(ProxyEventBaseCreationFixture, CreatingServiceElementWithInvalidProxyBindingMarksProxyBindingInvalid)
{
    auto valid_proxy_event_binding =
        std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::MockServiceElementType>();

    // Given a proxy with a valid binding
    DummyProxy dummy_proxy(nullptr,
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // When creating a ProxyEvent with a valid binding
    auto service_element = std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::ServiceElementType>(
        dummy_proxy, std::move(valid_proxy_event_binding), kEventName);

    // Then the proxy should report that all bindings are not valid
    EXPECT_FALSE(dummy_proxy.AreBindingsValid());
}

TYPED_TEST(ProxyEventBaseCreationFixture,
           CreatingServiceElementWithValidProxyEventBindingWillRegisterAndUnregisterEventBinding)
{
    auto valid_proxy_binding = std::make_unique<mock_binding::Proxy>();
    auto valid_proxy_event_binding =
        std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::MockServiceElementType>();

    auto* const mock_proxy_binding = valid_proxy_binding.get();
    ProxyEventBindingBase* const mock_proxy_event_binding = valid_proxy_event_binding.get();

    // Expecting that the ProxyEvent will register and unregister itself with the parent Proxy
    EXPECT_CALL(*mock_proxy_binding, RegisterEventBinding(kEventName, Ref(*mock_proxy_event_binding)));
    EXPECT_CALL(*mock_proxy_binding, UnregisterEventBinding(kEventName));

    // Given a proxy with a valid binding
    DummyProxy dummy_proxy(std::move(valid_proxy_binding),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // When creating a ProxyEvent with a valid binding
    auto service_element = std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::ServiceElementType>(
        dummy_proxy, std::move(valid_proxy_event_binding), kEventName);
}

TYPED_TEST(ProxyEventBaseCreationFixture,
           CreatingServiceElementWithInvalidProxyEventBindingWillNotRegisterOrUnregisterEventBinding)
{
    auto valid_proxy_binding = std::make_unique<mock_binding::Proxy>();

    auto* const mock_proxy_binding = valid_proxy_binding.get();

    // Expecting that the ProxyEvent will not register itself with the parent Proxy
    EXPECT_CALL(*mock_proxy_binding, RegisterEventBinding(_, _)).Times(0);
    EXPECT_CALL(*mock_proxy_binding, UnregisterEventBinding(_)).Times(0);

    // Given a proxy with a valid binding
    DummyProxy dummy_proxy(std::move(valid_proxy_binding),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // When creating a ProxyEvent with an invalid binding
    auto service_element = std::make_unique<typename ProxyEventBaseCreationFixture<TypeParam>::ServiceElementType>(
        dummy_proxy,
        std::unique_ptr<typename ProxyEventBaseCreationFixture<TypeParam>::MockServiceElementType>(nullptr),
        kEventName);
}

TYPED_TEST(ProxyEventBaseUnsubscribeFixture, CallingUnsubscribeWhileSubscribedCallsUnsubscribeOnBinding)
{
    this->RecordProperty("Verifies", "SCR-14033377, SCR-17292399, SCR-14137271, SCR-21286218");
    this->RecordProperty("Description", "Checks that unsubscribe will dispatch to binding if currently subscribed");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // and that GetSubscriptionState will be called once and indicate that the Proxy is already subscribed.
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // Expecting that the Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Unsubscribe());

    // When Unsubscribe is called on the ProxyBase
    this->service_element_->Unsubscribe();
}

TYPED_TEST(ProxyEventBaseUnsubscribeFixture, CallingUnsubscribeWhileSubscriptionIsPendingCallsUnsubscribeOnBinding)
{
    this->RecordProperty("Verifies", "SCR-14033377, SCR-17292399, SCR-14137271, SCR-21286218");
    this->RecordProperty("Description",
                         "Checks that unsubscribe will dispatch to binding if subscription is currently pending");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // and that GetSubscriptionState will be called once and indicate that the Proxy subscription is pending.
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscriptionPending));

    // Expecting that the Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Unsubscribe());

    // When Unsubscribe is called on the ProxyBase
    this->service_element_->Unsubscribe();
}

TYPED_TEST(ProxyEventBaseUnsubscribeFixture, CallingUnsubscribeWhileNotSubscribedDoesNothing)
{
    this->RecordProperty("Verifies", "SCR-14033377, SCR-17292399, SCR-14137271, SCR-21286218");
    this->RecordProperty("Description", "Checks that unsubscribe will do nothing if not already subscribed");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // and that GetSubscriptionState will be called once and indicate that the Proxy is not subscribed.
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // Expecting that the Unsubscribe will not be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Unsubscribe()).Times(0);

    // When Unsubscribe is called on the ProxyBase
    this->service_element_->Unsubscribe();
}

TYPED_TEST(ProxyEventBaseUnsubscribeDeathTest, CallingUnsubscribeWhileASampleIsStillReferencedTerminates)
{
    const auto CallUnsubscribeWhileSampleIsStillAllocated = [this]() {
        // Given a Service Element, that is connected to a mock binding
        this->CreateServiceElement();

        // Expecting that GetSubscriptionState will be called once and indicate that the Proxy is not subscribed.
        EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
            .WillOnce(Return(SubscriptionState::kSubscribed));

        // and that a sample has been allocated and not yet released
        auto& tracker = ProxyEventBaseAttorney{*this->service_element_}.GetSampleReferenceTracker();
        tracker.Reset(1);
        const auto allocation_result = tracker.Allocate(1);

        // and that the Unsubscribe will be called on the binding
        EXPECT_CALL(*this->mock_service_element_binding_, Unsubscribe());

        // Then we terminate when Unsubscribe is called on the ProxyBase
        this->service_element_->Unsubscribe();
    };
    EXPECT_DEATH(CallUnsubscribeWhileSampleIsStillAllocated(), ".*");
}

template <typename T>
class AServiceElement : public ::testing::Test
{
  public:
    using ServiceElementType = typename T::ServiceElementType;
    using MockServiceElementType = typename T::MockServiceElementType;

    AServiceElement()
    {
        CreateAServiceElement();
    }

    void CreateAServiceElement()
    {
        auto mock_service_element_binding_ptr = std::make_unique<MockServiceElementType>();
        mock_service_element_binding_ = mock_service_element_binding_ptr.get();

        service_element_ =
            std::make_unique<ServiceElementType>(empty_proxy_, std::move(mock_service_element_binding_ptr), kEventName);
        ASSERT_NE(service_element_, nullptr);

        ON_CALL(*mock_service_element_binding_, SetReceiveHandler(_)).WillByDefault(Return(score::ResultBlank{}));
    }

    AServiceElement& WithAReceiveHandler()
    {
        auto set_receive_handler_result = service_element_->SetReceiveHandler(EventReceiveHandler{[]() {}});
        EXPECT_TRUE(set_receive_handler_result.has_value());

        return *this;
    }

    AServiceElement& ThatCapturesTheReceiveHandlerGivenToBinding()
    {
        // and given that the receive handler is set by the binding
        ON_CALL(*this->mock_service_element_binding_, SetReceiveHandler(_))
            .WillByDefault(Invoke([this](std::weak_ptr<ScopedEventReceiveHandler> handler) -> ResultBlank {
                event_receive_handler_promise_->set_value(std::move(handler));
                return ResultBlank{};
            }));

        return *this;
    }

    std::weak_ptr<ScopedEventReceiveHandler> GetBindingDependentHandler()
    {
        auto event_receive_handler_future = this->event_receive_handler_promise_->get_future();
        EXPECT_TRUE(event_receive_handler_future.valid());
        return event_receive_handler_future.get();
    }

    // Used to save the ScopedEventReceiveHandler passed to the binding dependent SetReceiveHandler. Since gtest
    // currently seems to require that the lambdas provided to Invoke are copyable, we pass it in using a shared_ptr
    std::shared_ptr<std::promise<std::weak_ptr<ScopedEventReceiveHandler>>> event_receive_handler_promise_{
        std::make_shared<std::promise<std::weak_ptr<ScopedEventReceiveHandler>>>()};

    DummyProxy empty_proxy_{std::make_unique<mock_binding::Proxy>(),
                            make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment))};
    MockServiceElementType* mock_service_element_binding_{nullptr};
    std::unique_ptr<ServiceElementType> service_element_{nullptr};
};
TYPED_TEST_SUITE(AServiceElement, MyTypes, );

TYPED_TEST(AServiceElement, CanBeConstructed)
{
    // When a Service Element is created that is connected to a mock binding

    // Then we don't crash
}

TYPED_TEST(AServiceElement, CanBeConstructedWithAReceiveHandler)
{
    // Given a Service Element is created that is connected to a mock binding
    // When setting a receive handler
    this->WithAReceiveHandler();

    // Then we don't crash
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerThenItWillDispatchToTheBinding)
{
    this->RecordProperty("Verifies", "SCR-14034916, SCR-17292404, SCR-14137274");
    this->RecordProperty("Description", "Checks whether a set receive handler is correctly forwarded to the binding.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding

    // Expecting that the receive-handler is forwarded to the binding
    EXPECT_CALL(*this->mock_service_element_binding_, SetReceiveHandler(_)).WillOnce(Return(score::ResultBlank{}));

    // When setting a receive-handler
    score::cpp::ignore = this->service_element_->SetReceiveHandler(EventReceiveHandler{});
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerThenAValidResultWillBeReturned)
{
    this->RecordProperty("Verifies", "SCR-14034916, SCR-17292404, SCR-14137274");
    this->RecordProperty("Description", "Checks whether a set receive handler will return a valid result.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding

    // When setting a receive-handler
    const auto set_handler_result = this->service_element_->SetReceiveHandler(EventReceiveHandler{});

    // Then no error is returned
    EXPECT_TRUE(set_handler_result.has_value());
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerTwiceThenItWillDispatchToTheBindingTwice)
{
    this->RecordProperty("Verifies", "SCR-14034916, SCR-17292404, SCR-14137274");
    this->RecordProperty("Description", "Checks whether a set receive handler is correctly forwarded");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding

    // Expecting that the receive-handler is forwarded to the binding twice
    EXPECT_CALL(*this->mock_service_element_binding_, SetReceiveHandler(_))
        .Times(2)
        .WillRepeatedly(Return(score::ResultBlank{}));

    // When setting a receive-handler twice
    score::cpp::ignore = this->service_element_->SetReceiveHandler(EventReceiveHandler{});
    score::cpp::ignore = this->service_element_->SetReceiveHandler(EventReceiveHandler{});
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerTwiceThenAValidResultWillBeReturnedTwice)
{
    this->RecordProperty("Verifies", "SCR-14034916, SCR-17292404, SCR-14137274");
    this->RecordProperty("Description", "Checks whether a set receive handler is correctly forwarded");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding

    // When setting a receive-handler twice
    const auto set_handler_result = this->service_element_->SetReceiveHandler(EventReceiveHandler{});
    const auto set_handler_result_2 = this->service_element_->SetReceiveHandler(EventReceiveHandler{});

    // Then no errors are returned from either SetReceiveHandler call
    EXPECT_TRUE(set_handler_result.has_value());
    EXPECT_TRUE(set_handler_result_2.has_value());
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerThenItWillPropagateAnErrorFromTheBinding)
{
    this->RecordProperty("Verifies", "SCR-14034916, SCR-17292404, SCR-14137274");
    this->RecordProperty(
        "Description",
        "Checks that a set receive handler returns a kSetHandlerNotSet error code if binding returns any error.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    const auto returned_error_code = ComErrc::kSetHandlerNotSet;

    // Given a Service Element, that is connected to a mock binding

    // When SetReceiveHandler on the binding returns an error
    ON_CALL(*this->mock_service_element_binding_, SetReceiveHandler(_))
        .WillByDefault(Return(MakeUnexpected(binding_error_code)));

    // and when setting a receive-handler
    const auto set_handler_result = this->service_element_->SetReceiveHandler(EventReceiveHandler{});

    // Then an error is returned
    ASSERT_FALSE(set_handler_result.has_value());
    EXPECT_EQ(set_handler_result.error(), returned_error_code);
}

TYPED_TEST(AServiceElement, WhenUnsettingAReceiveHandlerAfterSettingThenItWillDispatchToBinding)
{
    this->RecordProperty("Verifies", "SCR-14035152, SCR-17292405, SCR-14137275");
    this->RecordProperty("Description", "Checks whether a unsetting receive handler is correctly forwarded");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding and has a set receive handler
    this->WithAReceiveHandler();

    // Expecting that the receive-handler is unset
    EXPECT_CALL(*this->mock_service_element_binding_, UnsetReceiveHandler()).WillOnce(Return(score::ResultBlank{}));

    // and un-setting the receive-handler
    score::cpp::ignore = this->service_element_->UnsetReceiveHandler();
}

TYPED_TEST(AServiceElement, WhenUnsettingReceiveHandlerAfterSettingThenItWillReturnAValidResult)
{
    this->RecordProperty("Verifies", "SCR-14035152, SCR-17292405, SCR-14137275");
    this->RecordProperty("Description", "Checks whether a unsetting receive handler is correctly forwarded");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding and has a set receive handler
    this->WithAReceiveHandler();

    // and un-setting the receive-handler
    const auto unset_handler_result = this->service_element_->UnsetReceiveHandler();

    // Then no error is returned
    EXPECT_TRUE(unset_handler_result.has_value());
}

TYPED_TEST(AServiceElement, WhenUnsettingAReceiveHandlerWithoutSettingThenItWillNotDispatchToBinding)
{
    // Given a Service Element, that is connected to a mock binding

    // Expecting that the UnsetReceiveHandler is NOT be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, UnsetReceiveHandler()).Times(0);

    // When un-setting the receive-handler without setting it
    const auto unset_handler_result = this->service_element_->UnsetReceiveHandler();

    // Then no error is returned
    EXPECT_TRUE(unset_handler_result.has_value());
}

TYPED_TEST(AServiceElement, WhenUnsettingAReceiveHandlerWithoutSettingThenItWillReturnAValidResult)
{
    // Given a Service Element, that is connected to a mock binding

    // When un-setting the receive-handler without setting it
    const auto unset_handler_result = this->service_element_->UnsetReceiveHandler();

    // Then no error is returned
    EXPECT_TRUE(unset_handler_result.has_value());
}

TYPED_TEST(AServiceElement, WhenUnsettingReceiveHandlerThenItWillPropagateErrorFromBinding)
{
    this->RecordProperty("Verifies", "SCR-14035152, SCR-17292405, SCR-14137275");
    this->RecordProperty(
        "Description",
        "Checks that an unset receive handler returns a kUnsetFailure error code if binding returns any error.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    const auto returned_error_code = ComErrc::kUnsetFailure;

    // Given a Service Element, that is connected to a mock binding and has a set receive handler
    this->WithAReceiveHandler();

    // When UnsetReceiveHandler on the binding returns an error
    ON_CALL(*this->mock_service_element_binding_, UnsetReceiveHandler())
        .WillByDefault(Return(MakeUnexpected(binding_error_code)));

    // and when un-setting the receive-handler
    const auto unset_handler_result = this->service_element_->UnsetReceiveHandler();

    // Then an error is returned
    ASSERT_FALSE(unset_handler_result.has_value());
    EXPECT_EQ(unset_handler_result.error(), returned_error_code);
}

TYPED_TEST(AServiceElement, WhenSettingAReceiveHandlerThenTheLifetimeOfTheScopedEventReceiveHandlerIsValid)
{
    this->RecordProperty("Verifies", "SCR-20236346");
    this->RecordProperty(
        "Description",
        "Checks that the lifetime of the ScopedEventReceiveHandler is active when SetReceiveHandler is called.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding that captures the binding dependent handler
    this->ThatCapturesTheReceiveHandlerGivenToBinding();

    // When setting the receive handler
    score::cpp::ignore = this->service_element_->SetReceiveHandler(EventReceiveHandler{[]() {}});

    // Then the lifetime of the scoped function should be valid
    auto binding_event_receive_handler_weak_ptr = this->GetBindingDependentHandler();
    auto binding_event_receive_handler = binding_event_receive_handler_weak_ptr.lock();
    ASSERT_TRUE(binding_event_receive_handler);
    EXPECT_TRUE((*binding_event_receive_handler)().has_value());
}

TYPED_TEST(AServiceElement,
           WhenUnSettingAReceiveHandlerAfterSettingThenTheLifetimeOfTheScopedEventReceiveHandlerIsInvalid)
{
    this->RecordProperty("Verifies", "SCR-20236346");
    this->RecordProperty("Description",
                         "Checks that the weak_ptr to the event receive handler given to the binding can't be locked "
                         "again, when UnsetReceiveHandler has been returned.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding with a receive handler and a captured binding
    // dependent handler
    this->ThatCapturesTheReceiveHandlerGivenToBinding().WithAReceiveHandler();

    // When un-setting the receive-handler
    this->service_element_->UnsetReceiveHandler();

    // Then the binding can't lock successfully its weak_ptr again.
    auto binding_event_receive_handler_weak_ptr = this->GetBindingDependentHandler();
    auto binding_event_receive_handler = binding_event_receive_handler_weak_ptr.lock();
    EXPECT_FALSE(binding_event_receive_handler);
}

TYPED_TEST(ProxyEventBaseSubscribeFixture, CallingSubscribeWhileUnsubscribedDispatchesToBinding)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty("Description", "Checks that Subscribe will dispatch to binding if not already subscribed");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::uint8_t max_sample_count{7U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetSubscriptionState is called once and indicates that the Service Element is currently not
    // subscribed
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // and that Subscribe will be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(max_sample_count));

    // When Subscribe is called on the Service Element
    const auto subscribe_result = this->service_element_->Subscribe(max_sample_count);

    // Then the result will not contain an error
    ASSERT_TRUE(subscribe_result.has_value());
}

TYPED_TEST(ProxyEventBaseSubscribeFixture, CallingSubscribeTwiceWithSameMaxSamplesOnlyPerformsSubscriptionOnce)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty("Description", "Checks that Subscribe will do nothing if subscription is pending");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};

    const std::uint8_t max_sample_count{7U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that Subscribe is called on the binding only once
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(7)).WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscriptionPending));

    // and that GetMaxSampleCount is called on the binding and returns the current max_sample_count
    EXPECT_CALL(*this->mock_service_element_binding_, GetMaxSampleCount()).WillOnce(Return(max_sample_count));

    // When subscribe is called on the Service Element twice with the same max samples
    // and then no errors are returned
    const auto subscribe_result_1 = this->service_element_->Subscribe(7U);
    ASSERT_TRUE(subscribe_result_1.has_value());

    const auto subscribe_result_2 = this->service_element_->Subscribe(7U);
    ASSERT_TRUE(subscribe_result_2.has_value());
}

TYPED_TEST(ProxyEventBaseSubscribeFixture, SubscribeShouldReturnErrorIfBindingReturnsError)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty("Description",
                         "Checks that Subscribe returns a kBindingFailure error code if binding returns any error.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    const auto returned_error_code = ComErrc::kBindingFailure;

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));
    // Expect that when Subscribe is called on the binding it returns an error
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(7))
        .WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When subscribe is called on the Service Element
    const auto subscribe_result = this->service_element_->Subscribe(7U);

    // Then the error from the binding is returned
    ASSERT_FALSE(subscribe_result.has_value());
    EXPECT_EQ(subscribe_result.error(), returned_error_code);
}

TYPED_TEST(ProxyEventBaseSubscribeFixture, CallingSubscribeWithSameMaxSampleCountWhileSubscriptionIsPendingDoesNothing)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty("Description",
                         "Checks that Subscribe will do nothing if subscribing again with same max sample count");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::uint8_t max_sample_count{7U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetSubscriptionState is called once and indicates that the Service Elementsubscription is pending
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscriptionPending));

    // and that GetMaxSampleCount is called on the binding and returns the current max_sample_count
    EXPECT_CALL(*this->mock_service_element_binding_, GetMaxSampleCount()).WillOnce(Return(max_sample_count));

    // and that Subscribe will not be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(_)).Times(0);

    // When Subscribe is called on the Service Element with the same max_sample_count
    const auto subscribe_result = this->service_element_->Subscribe(max_sample_count);

    // Then the result will not contain an error
    ASSERT_TRUE(subscribe_result.has_value());
}

TYPED_TEST(ProxyEventBaseSubscribeFixture, CallingSubscribeWithSameMaxSampleCountWhileAlreadySubscribedDoesNothing)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty("Description",
                         "Checks that Subscribe will do nothing if subscribing again with same max sample count");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::uint8_t max_sample_count{7U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetSubscriptionState is called once and indicates that the Service Element is already subscribed
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that GetMaxSampleCount is called on the binding and returns the current max_sample_count
    EXPECT_CALL(*this->mock_service_element_binding_, GetMaxSampleCount()).WillOnce(Return(max_sample_count));

    // and that Subscribe will not be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(_)).Times(0);

    // When Subscribe is called on the Service Element with the same max_sample_count
    const auto subscribe_result = this->service_element_->Subscribe(max_sample_count);

    // Then the result will not contain an error
    ASSERT_TRUE(subscribe_result.has_value());
}

TYPED_TEST(ProxyEventBaseSubscribeFixture,
           CallingSubscribeWithDifferentMaxSampleCountWhileSubscriptionIsPendingReturnsError)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty(
        "Description",
        "Checks that Subscribe will return an error if subscribing again with a different max sample count");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::uint8_t current_max_sample_count{7U};
    const auto new_max_sample_count = current_max_sample_count + 1U;

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetSubscriptionState is called once and indicates that the Service Element subscription is pending
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscriptionPending));

    // and that GetMaxSampleCount is called on the binding and returns the current max_sample_count
    EXPECT_CALL(*this->mock_service_element_binding_, GetMaxSampleCount()).WillOnce(Return(current_max_sample_count));

    // and that Subscribe will not be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(_)).Times(0);

    // When Subscribe is called on the Service Element with a different max_sample_count
    const auto subscribe_result = this->service_element_->Subscribe(new_max_sample_count);

    // Then the result will contain an error
    ASSERT_FALSE(subscribe_result.has_value());
    EXPECT_EQ(subscribe_result.error(), ComErrc::kMaxSampleCountNotRealizable);
}

TYPED_TEST(ProxyEventBaseSubscribeFixture,
           CallingSubscribeWithDifferentMaxSampleCountWhileAlreadySubscribedReturnsError)
{
    this->RecordProperty("Verifies", "SCR-14033248, SCR-17292398, SCR-14137270");
    this->RecordProperty(
        "Description",
        "Checks that Subscribe will return an error if subscribing again with a different max sample count");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::uint8_t current_max_sample_count{7U};
    const auto new_max_sample_count = current_max_sample_count + 1U;

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetSubscriptionState is called once and indicates that the Service Element is already subscribed
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that GetMaxSampleCount is called on the binding and returns the current max_sample_count
    EXPECT_CALL(*this->mock_service_element_binding_, GetMaxSampleCount()).WillOnce(Return(current_max_sample_count));

    // and that Subscribe will not be called on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, Subscribe(_)).Times(0);

    // When Subscribe is called on the Service Element with a different max_sample_count
    const auto subscribe_result = this->service_element_->Subscribe(new_max_sample_count);

    // Then the result will contain an error
    ASSERT_FALSE(subscribe_result.has_value());
    EXPECT_EQ(subscribe_result.error(), ComErrc::kMaxSampleCountNotRealizable);
}

TYPED_TEST(ProxyEventBaseGetSubscriptionStatefixture, GetSubscriptionStateDispatchesToBinding)
{
    this->RecordProperty("Verifies", "SCR-14034825, SCR-17292400, SCR-14137272");
    this->RecordProperty("Description",
                         "Checks that GetSubscriptionState will return the free sample count returned by the binding");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    InSequence sequence{};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expecting that GetSubscriptionState will be called on the binding 3 times returning the following values in this
    // order: kNotSubscribed, kSubscriptionPending, kSubscribed.
    const SubscriptionState expected_first_state{SubscriptionState::kNotSubscribed};
    const SubscriptionState expected_second_state{SubscriptionState::kSubscriptionPending};
    const SubscriptionState expected_third_state{SubscriptionState::kSubscribed};
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState()).WillOnce(Return(expected_first_state));
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState()).WillOnce(Return(expected_second_state));
    EXPECT_CALL(*this->mock_service_element_binding_, GetSubscriptionState()).WillOnce(Return(expected_third_state));

    // When calling GetSubscriptionState 3 times
    const auto first_state = this->service_element_->GetSubscriptionState();
    const auto second_state = this->service_element_->GetSubscriptionState();
    const auto third_state = this->service_element_->GetSubscriptionState();

    // Then each call will return the correct value that was returned by the binding
    EXPECT_EQ(first_state, expected_first_state);
    EXPECT_EQ(second_state, expected_second_state);
    EXPECT_EQ(third_state, expected_third_state);
}

TYPED_TEST(ProxyEventBaseGetFreeSampleCountFixture, GetFreeSampleCountReturnsCountFromReferenceTracker)
{
    this->RecordProperty("Verifies", "SCR-14035121, SCR-17292402, SCR-14137276, SCR-21293991");
    this->RecordProperty(
        "Description",
        "Checks that GetFreeSampleCount will return the free sample count returned by the reference tracker.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t free_sample_count{5U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // and that the underlying sample reference tracker has 5 free slots
    auto& tracker = ProxyEventBaseAttorney{*this->service_element_}.GetSampleReferenceTracker();
    tracker.Reset(free_sample_count);

    // When calling GetFreeSampleCount
    // Then the free sample count from the sample reference tracker will be returned
    const auto received_free_sample_count = this->service_element_->GetFreeSampleCount();
    EXPECT_EQ(received_free_sample_count, free_sample_count);

    // and when allocating 2 additional slots
    score::cpp::optional<TrackerGuardFactory> tracker_guard_factory{tracker.Allocate(2)};

    // and when calling GetFreeSampleCount
    // Then the free sample count should be updated
    const auto received_free_sample_count_2 = this->service_element_->GetFreeSampleCount();
    EXPECT_EQ(received_free_sample_count_2, free_sample_count - 2);

    // and when the slots are released
    tracker_guard_factory.reset();

    // and when calling GetFreeSampleCount
    // Then the free sample count should be updated
    const auto received_free_sample_count_3 = this->service_element_->GetFreeSampleCount();
    EXPECT_EQ(received_free_sample_count_3, free_sample_count);
}

TYPED_TEST(ProxyEventBaseGetNumNewSamplesAvailableFixture, GetNumNewSamplesAvailableDispatchesToBinding)
{
    this->RecordProperty("Verifies", "SCR-14035142, SCR-17292403, SCR-14137277");
    this->RecordProperty(
        "Description",
        "Checks that GetNumNewSamplesAvailable will return the number of samples available returned by the binding");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t expected_num_new_samples_available{10U};

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetNumNewSamplesAvailable is called once on the binding
    EXPECT_CALL(*this->mock_service_element_binding_, GetNumNewSamplesAvailable())
        .WillOnce(Return(expected_num_new_samples_available));

    // When GetNumNewSamplesAvailable is called on the Service Element
    const auto get_num_new_samples_available_result = this->service_element_->GetNumNewSamplesAvailable();

    // Then the result will contain the number of samples available returned by the binding
    ASSERT_TRUE(get_num_new_samples_available_result.has_value());
    EXPECT_EQ(get_num_new_samples_available_result.value(), expected_num_new_samples_available);
}

TYPED_TEST(ProxyEventBaseGetNumNewSamplesAvailableFixture, GetNumNewSamplesAvailableReturnsErrorIfNotSubscribed)
{
    this->RecordProperty("Verifies", "SCR-14035142, SCR-17292403, SCR-14137277");
    this->RecordProperty("Description",
                         "Checks that GetNumNewSamplesAvailable will forward an error kNotSubscribed from the binding");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetNumNewSamplesAvailable is called once on the binding and returns an error that there's no active
    // subscription
    EXPECT_CALL(*this->mock_service_element_binding_, GetNumNewSamplesAvailable())
        .WillOnce(Return(MakeUnexpected(ComErrc::kNotSubscribed)));

    // When GetNumNewSamplesAvailable is called on the Service Element
    const auto get_num_new_samples_available_result = this->service_element_->GetNumNewSamplesAvailable();

    // Then the result will contain an error that there's no active subscription
    ASSERT_FALSE(get_num_new_samples_available_result.has_value());
    EXPECT_EQ(get_num_new_samples_available_result.error(), ComErrc::kNotSubscribed);
}

TYPED_TEST(ProxyEventBaseGetNumNewSamplesAvailableFixture, GetNumNewSamplesAvailableReturnsErrorFromBinding)
{
    this->RecordProperty("Verifies", "SCR-14035142, SCR-17292403, SCR-14137277");
    this->RecordProperty(
        "Description",
        "Checks that GetNumNewSamplesAvailable will return kBindingFailure for a generic error code from the binding");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a Service Element, that is connected to a mock binding
    this->CreateServiceElement();

    // Expect that GetNumNewSamplesAvailable is called once on the binding and returns an error
    EXPECT_CALL(*this->mock_service_element_binding_, GetNumNewSamplesAvailable())
        .WillOnce(Return(MakeUnexpected(ComErrc::kServiceNotAvailable)));

    // When GetNumNewSamplesAvailable is called on the Service Element
    const auto get_num_new_samples_available_result = this->service_element_->GetNumNewSamplesAvailable();

    // Then the result will contain an error that there was a binding failure
    ASSERT_FALSE(get_num_new_samples_available_result.has_value());
    EXPECT_EQ(get_num_new_samples_available_result.error(), ComErrc::kBindingFailure);
}

TEST(ProxyEventBaseTest, MoveConstructingProxyEventDoesNotCrash)
{
    auto mock_proxy_event_binding_ptr = std::make_unique<StrictMock<mock_binding::ProxyEventBase>>();
    auto& mock_proxy_event_binding = *mock_proxy_event_binding_ptr;

    DummyProxy empty_proxy(std::make_unique<mock_binding::Proxy>(),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // Given a Service Element, that is connected to a mock binding
    ProxyEventBase dummy_event{
        empty_proxy, ProxyBaseView{empty_proxy}.GetBinding(), std::move(mock_proxy_event_binding_ptr), kEventName};

    // And a new Service Element is created with the move constructor
    ProxyEventBase dummy_event_2{std::move(dummy_event)};

    // Expect that Subscribe is called on the binding only once
    EXPECT_CALL(mock_proxy_event_binding, GetSubscriptionState()).WillOnce(Return(SubscriptionState::kNotSubscribed));
    EXPECT_CALL(mock_proxy_event_binding, Subscribe(7)).WillOnce(Return(score::ResultBlank{}));

    // When calling subscribe
    const auto subscribe_result = dummy_event_2.Subscribe(7U);

    // and then no errors are returned
    ASSERT_TRUE(subscribe_result.has_value());
}

TEST(ProxyEventBaseTest, MoveAssigningProxyEventDoesNotCrash)
{
    auto mock_proxy_event_binding_ptr = std::make_unique<StrictMock<mock_binding::ProxyEventBase>>();
    auto& mock_proxy_event_binding = *mock_proxy_event_binding_ptr;

    DummyProxy empty_proxy(std::make_unique<mock_binding::Proxy>(),
                           make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));

    // Given a Service Element, that is connected to a mock binding
    ProxyEventBase dummy_event{
        empty_proxy, ProxyBaseView{empty_proxy}.GetBinding(), std::move(mock_proxy_event_binding_ptr), kEventName};

    // And a new Service Element is created and move assigned
    ProxyEventBase dummy_event_2{empty_proxy,
                                 ProxyBaseView{empty_proxy}.GetBinding(),
                                 std::make_unique<StrictMock<mock_binding::ProxyEventBase>>(),
                                 kEventName2};
    dummy_event_2 = std::move(dummy_event);

    // Expect that Subscribe is called on the binding only once
    EXPECT_CALL(mock_proxy_event_binding, GetSubscriptionState()).WillOnce(Return(SubscriptionState::kNotSubscribed));
    EXPECT_CALL(mock_proxy_event_binding, Subscribe(7)).WillOnce(Return(score::ResultBlank{}));

    // When calling subscribe
    const auto subscribe_result = dummy_event_2.Subscribe(7U);

    // and then no errors are returned
    ASSERT_TRUE(subscribe_result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
