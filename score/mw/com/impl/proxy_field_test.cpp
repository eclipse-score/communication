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
/// This file contains unit tests for functionality that is unique to fields.
/// There is additional test functionality in the following locations:
///     - score/mw/com/impl/proxy_event_test.cpp contains unit tests which test the event-like functionality of
///     fields.
///     - score/mw/com/impl/bindings/lola/test/proxy_field_component_test.cpp contains component tests which test
///     binding specific field functionality.

#include "score/mw/com/impl/proxy_field.h"

#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/proxy_resources.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestSampleType = std::uint8_t;

class TestProxyBase : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;
    const ProxyBase::ProxyMethods& GetMethods()
    {
        return methods_;
    }
    const ProxyBase::ProxyFields& GetFields()
    {
        return fields_;
    }
};

namespace
{
// SFINAE detection traits used to verify that the notifier API surfafce is gated on the WithNotifier tag.
// Each trait checks whether the corresponding member call is well-formed on the field type.

template <typename, typename = void>
struct has_subscribe : std::false_type
{
};
template <typename T>
struct has_subscribe<T, std::void_t<decltype(std::declval<T&>().Subscribe(std::declval<std::size_t>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_unsubscribe : std::false_type
{
};
template <typename T>
struct has_unsubscribe<T, std::void_t<decltype(std::declval<T&>().Unsubscribe())>> : std::true_type
{
};

template <typename, typename = void>
struct has_get_subscription_state : std::false_type
{
};
template <typename T>
struct has_get_subscription_state<T, std::void_t<decltype(std::declval<T&>().GetSubscriptionState())>> : std::true_type
{
};

template <typename, typename = void>
struct has_get_free_sample_count : std::false_type
{
};
template <typename T>
struct has_get_free_sample_count<T, std::void_t<decltype(std::declval<T&>().GetFreeSampleCount())>> : std::true_type
{
};

template <typename, typename = void>
struct has_get_num_new_samples_available : std::false_type
{
};
template <typename T>
struct has_get_num_new_samples_available<T, std::void_t<decltype(std::declval<T&>().GetNumNewSamplesAvailable())>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_set_receive_handler : std::false_type
{
};
template <typename T>
struct has_set_receive_handler<
    T,
    std::void_t<decltype(std::declval<T&>().SetReceiveHandler(std::declval<EventReceiveHandler>()))>> : std::true_type
{
};

template <typename, typename = void>
struct has_unset_receive_handler : std::false_type
{
};
template <typename T>
struct has_unset_receive_handler<T, std::void_t<decltype(std::declval<T&>().UnsetReceiveHandler())>> : std::true_type
{
};

struct DummySampleReceiver
{
    template <typename SamplePtrT>
    void operator()(SamplePtrT) noexcept
    {
    }
};

template <typename, typename = void>
struct has_get_new_samples : std::false_type
{
};
template <typename T>
struct has_get_new_samples<T,
                           std::void_t<decltype(std::declval<T&>().GetNewSamples(std::declval<DummySampleReceiver>(),
                                                                                 std::declval<std::size_t>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_inject_mock : std::false_type
{
};
template <typename T>
struct has_inject_mock<
    T,
    std::void_t<decltype(std::declval<T&>().InjectMock(std::declval<IProxyEvent<typename T::FieldType>&>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_get : std::false_type
{
};
template <typename T>
struct has_get<T, std::void_t<decltype(std::declval<T&>().Get())>> : std::true_type
{
};

template <typename, typename = void>
struct has_set : std::false_type
{
};
template <typename T>
struct has_set<T, std::void_t<decltype(std::declval<T&>().Set(std::declval<typename T::FieldType&>()))>>
    : std::true_type
{
};
}  // namespace

TEST(ProxyFieldTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-17397027");
    RecordProperty("Description", "Checks copy semantics for ProxyField");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<ProxyField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
                  "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<ProxyField<TestSampleType, WithGetter, WithNotifier, WithSetter>>::value,
                  "Is wrongly copyable");
}

TEST(ProxyFieldTest, IsMoveable)
{
    static_assert(std::is_move_constructible<ProxyField<TestSampleType, WithGetter, WithSetter>>::value,
                  "Is not move constructible");
    static_assert(std::is_move_assignable<ProxyField<TestSampleType, WithGetter, WithSetter>>::value,
                  "Is not move assignable");
}

TEST(ProxyFieldTest, ClassTypeDependsOnFieldDataType)
{
    RecordProperty("Verifies", "SCR-29235459");
    RecordProperty("Description", "ProxyFields with different field data types should be different classes.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using FirstProxyFieldType = ProxyField<bool, WithGetter, WithNotifier, WithSetter>;
    using SecondProxyFieldType = ProxyField<std::uint16_t, WithGetter, WithNotifier, WithSetter>;
    static_assert(!std::is_same_v<FirstProxyFieldType, SecondProxyFieldType>,
                  "Class type does not depend on field data type");
}

TEST(ProxyFieldTest, ProxyFieldContainsPublicFieldType)
{
    RecordProperty("Verifies", "SCR-17291997");
    RecordProperty("Description",
                   "A ProxyField contains a public member type FieldType which denotes the type of the field.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using CustomFieldType = std::uint16_t;
    static_assert(std::is_same<typename ProxyField<CustomFieldType, WithGetter, WithNotifier, WithSetter>::FieldType,
                               CustomFieldType>::value,
                  "Incorrect FieldType.");
}

TEST(ProxyFieldNotifierGatingTest, NotifierApiExistsWhenWithNotifierTagIsPresent)
{
    // Given a ProxyField with the WithNotifier tag
    // When checking the notifier API
    // Then all notifier-related members should be present
    using NotifierField = ProxyField<TestSampleType, WithNotifier>;
    static_assert(has_subscribe<NotifierField>::value);
    static_assert(has_unsubscribe<NotifierField>::value);
    static_assert(has_get_subscription_state<NotifierField>::value);
    static_assert(has_get_free_sample_count<NotifierField>::value);
    static_assert(has_get_num_new_samples_available<NotifierField>::value);
    static_assert(has_set_receive_handler<NotifierField>::value);
    static_assert(has_unset_receive_handler<NotifierField>::value);
    static_assert(has_get_new_samples<NotifierField>::value);
    static_assert(has_inject_mock<NotifierField>::value);
}

TEST(ProxyFieldNotifierGatingTest, NotifierApiIsAbsentWhenWithNotifierTagIsMissing)
{
    // Given a ProxyField without the WithNotifier tag
    // When checking the notifier API
    // Then all notifier-related members should be absent
    using GetterOnlyField = ProxyField<TestSampleType, WithGetter>;
    static_assert(!has_subscribe<GetterOnlyField>::value);
    static_assert(!has_unsubscribe<GetterOnlyField>::value);
    static_assert(!has_get_subscription_state<GetterOnlyField>::value);
    static_assert(!has_get_free_sample_count<GetterOnlyField>::value);
    static_assert(!has_get_num_new_samples_available<GetterOnlyField>::value);
    static_assert(!has_set_receive_handler<GetterOnlyField>::value);
    static_assert(!has_unset_receive_handler<GetterOnlyField>::value);
    static_assert(!has_get_new_samples<GetterOnlyField>::value);
    static_assert(!has_inject_mock<GetterOnlyField>::value);
}

TEST(ProxyFieldGetterGatingTest, GetExistsWhenWithGetterTagIsPresent)
{
    // Given a ProxyField with the WithGetter tag
    // When checking Get
    // Then Get should be present
    using GetterField = ProxyField<TestSampleType, WithGetter>;
    static_assert(has_get<GetterField>::value);
}

TEST(ProxyFieldGetterGatingTest, GetIsAbsentWhenWithGetterTagIsMissing)
{
    // Given a ProxyField without the WithGetter tag
    // When checking Get
    // Then Get should be absent
    using NotifierOnlyField = ProxyField<TestSampleType, WithNotifier>;
    static_assert(!has_get<NotifierOnlyField>::value);
}

TEST(ProxyFieldSetterGatingTest, SetExistsWhenWithSetterTagIsPresent)
{
    // Given a ProxyField with the WithSetter tag
    // When checking Set
    // Then Set should be present
    using SetterField = ProxyField<TestSampleType, WithGetter, WithSetter>;
    static_assert(has_set<SetterField>::value);
}

TEST(ProxyFieldSetterGatingTest, SetIsAbsentWhenWithSetterTagIsMissing)
{
    // Given a ProxyField without the WithSetter tag
    // When checking Set
    // Then Set should be absent
    using GetterOnlyField = ProxyField<TestSampleType, WithGetter>;
    static_assert(!has_set<GetterOnlyField>::value);
}

/// \brief Test fixture for the runtime behavior of ProxyField's Get / Set / notifier surface.
class ProxyFieldGetSetFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        ON_CALL(proxy_method_binding_mock_, GetReturnValueBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{return_type_buffer_.data(), return_type_buffer_.size()}}));
        ON_CALL(proxy_method_binding_mock_, GetInArgsBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{in_args_buffer_.data(), in_args_buffer_.size()}}));
        ON_CALL(proxy_method_binding_mock_, DoCall(0)).WillByDefault(Return(score::ResultBlank{}));

        ON_CALL(set_method_binding_mock_, GetReturnValueBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{return_type_buffer_.data(), return_type_buffer_.size()}}));
        ON_CALL(set_method_binding_mock_, GetInArgsBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{in_args_buffer_.data(), in_args_buffer_.size()}}));
        ON_CALL(set_method_binding_mock_, DoCall(0)).WillByDefault(Return(score::ResultBlank{}));
    }

    ProxyField<TestSampleType, WithGetter> MakeFieldWithGetOnly(const std::string_view name = "TestField")
    {
        return ProxyField<TestSampleType, WithGetter>{
            proxy_base_,
            name,
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_mock_),
            nullptr,
            std::make_unique<mock_binding::ProxyMethodFacade>(proxy_method_binding_mock_)};
    }

    ProxyField<TestSampleType, WithSetter, WithNotifier> MakeFieldWithSetAndNotifier(
        const std::string_view name = "TestField")
    {
        return ProxyField<TestSampleType, WithSetter, WithNotifier>{
            proxy_base_,
            name,
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_mock_),
            std::make_unique<mock_binding::ProxyMethodFacade>(set_method_binding_mock_)};
    }

    ProxyField<TestSampleType, WithNotifier> MakeFieldWithNotifierOnly(const std::string_view name = "TestField")
    {
        return ProxyField<TestSampleType, WithNotifier>{
            proxy_base_, name, std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_mock_)};
    }

    alignas(8) std::array<std::byte, 1024> return_type_buffer_{};
    alignas(8) std::array<std::byte, 1024> in_args_buffer_{};
    RuntimeMockGuard runtime_mock_guard_{};
    ConfigurationStore config_store_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
                                     make_ServiceIdentifierType("foo"),
                                     QualityType::kASIL_QM,
                                     LolaServiceTypeDeployment{42U},
                                     LolaServiceInstanceDeployment{1U}};
    TestProxyBase proxy_base_{std::make_unique<mock_binding::Proxy>(), config_store_.GetHandle()};
    mock_binding::ProxyEvent<TestSampleType> proxy_event_mock_{};
    mock_binding::ProxyMethod proxy_method_binding_mock_{};
    mock_binding::ProxyMethod set_method_binding_mock_{};
};

TEST_F(ProxyFieldGetSetFixture, GetDelegatesToProxyMethodBindingAndReturnsValueFromReturnBuffer)
{
    // Given a Get-enabled ProxyField (WithGetter) whose return buffer already holds a known field value
    auto field = MakeFieldWithGetOnly();
    constexpr TestSampleType expected_value{42U};
    return_type_buffer_[0] = static_cast<std::byte>(expected_value);

    // When calling Get()
    EXPECT_CALL(proxy_method_binding_mock_, GetReturnValueBuffer(0));
    EXPECT_CALL(proxy_method_binding_mock_, DoCall(0));
    const auto result = field.Get();

    // Then the call is delegated to the binding and the returned pointer exposes the value from the return buffer
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), expected_value);
}

TEST_F(ProxyFieldGetSetFixture, SetDelegatesToProxyMethodBindingAndCopiesValueIntoInArgsBuffer)
{
    // Given a ProxyField with a setter (WithSetter, WithNotifier)
    auto field = MakeFieldWithSetAndNotifier();
    TestSampleType value{42U};
    EXPECT_CALL(set_method_binding_mock_, GetInArgsBuffer(0));
    EXPECT_CALL(set_method_binding_mock_, GetReturnValueBuffer(0));
    EXPECT_CALL(set_method_binding_mock_, DoCall(0));

    // When Set is called
    auto result = field.Set(value);

    // Then the call delegates to the binding and the value lands in the in-args buffer
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(static_cast<TestSampleType>(in_args_buffer_[0]), value);
}

TEST_F(ProxyFieldGetSetFixture, GetPropagatesBindingError)
{
    // Given a Get-enabled ProxyField (WithGetter) where the binding returns an error
    auto field = MakeFieldWithGetOnly();

    // When calling Get() and the binding reports an error
    EXPECT_CALL(proxy_method_binding_mock_, GetReturnValueBuffer(0))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));
    auto result = field.Get();

    // Then the error is propagated back to the caller
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(ProxyFieldGetSetFixture, SetPropagatesBindingError)
{
    // Given a Set-enabled ProxyField (WithSetter, WithNotifier) where the binding returns an error
    auto field = MakeFieldWithSetAndNotifier();

    // When calling Set() and the binding reports an error
    TestSampleType value{42U};
    EXPECT_CALL(set_method_binding_mock_, GetInArgsBuffer(0))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));
    auto result = field.Set(value);

    // Then the error is propagated back to the caller
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(ProxyFieldGetSetFixture, GetMethodIsNotRegisteredAsRegularMethod)
{
    // When constructing a Get-only ProxyField
    auto field = MakeFieldWithGetOnly();

    // Then the field shows up on the parent proxy, but the Get method does not get registered as a regular method
    EXPECT_EQ(proxy_base_.GetFields().size(), 1U);
    EXPECT_TRUE(proxy_base_.GetMethods().empty());
}

TEST_F(ProxyFieldGetSetFixture, SetMethodIsNotRegisteredAsRegularMethod)
{
    // When constructing a Set+Notifier ProxyField
    auto field = MakeFieldWithSetAndNotifier();

    // Then the field shows up on the parent proxy, but the Set method does not get registered as a regular method
    EXPECT_EQ(proxy_base_.GetFields().size(), 1U);
    EXPECT_TRUE(proxy_base_.GetMethods().empty());
}

TEST_F(ProxyFieldGetSetFixture, NotifierOnlyFieldRegistersAsField)
{
    // When constructing a ProxyField with WithNotifier only (no Get, no Set)
    auto field = MakeFieldWithNotifierOnly();

    // Then the field is still registered on the parent proxy.
    EXPECT_EQ(proxy_base_.GetFields().size(), 1U);
}

}  // namespace
}  // namespace score::mw::com::impl
