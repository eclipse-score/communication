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

#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/com_error.h"
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
struct has_set_subscription_state_change_handler : std::false_type
{
};
template <typename T>
struct has_set_subscription_state_change_handler<
    T,
    std::void_t<decltype(std::declval<T&>().SetSubscriptionStateChangeHandler(
        std::declval<SubscriptionStateChangeHandler>()))>> : std::true_type
{
};

template <typename, typename = void>
struct has_unset_subscription_state_change_handler : std::false_type
{
};
template <typename T>
struct has_unset_subscription_state_change_handler<
    T,
    std::void_t<decltype(std::declval<T&>().UnsetSubscriptionStateChangeHandler())>> : std::true_type
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
    static_assert(has_set_subscription_state_change_handler<NotifierField>::value);
    static_assert(has_unset_subscription_state_change_handler<NotifierField>::value);
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
    static_assert(!has_set_subscription_state_change_handler<GetterOnlyField>::value);
    static_assert(!has_unset_subscription_state_change_handler<GetterOnlyField>::value);
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

TEST(ProxyFieldNotifierGatingTest, OnlyNotifierApiExistsWhenNoTagsArePresent)
{
    // Given a ProxyField with the WithNotifier tag
    // When checking the notifier API
    // Then all notifier-related members should be present
    using DefaultField = ProxyField<TestSampleType>;
    static_assert(has_subscribe<DefaultField>::value);
    static_assert(has_unsubscribe<DefaultField>::value);
    static_assert(has_set_subscription_state_change_handler<DefaultField>::value);
    static_assert(has_unset_subscription_state_change_handler<DefaultField>::value);
    static_assert(has_get_subscription_state<DefaultField>::value);
    static_assert(has_get_free_sample_count<DefaultField>::value);
    static_assert(has_get_num_new_samples_available<DefaultField>::value);
    static_assert(has_set_receive_handler<DefaultField>::value);
    static_assert(has_unset_receive_handler<DefaultField>::value);
    static_assert(has_get_new_samples<DefaultField>::value);
    static_assert(has_inject_mock<DefaultField>::value);
    static_assert(!has_get<DefaultField>::value);
    static_assert(!has_set<DefaultField>::value);
}

/// \brief Test fixture for the runtime behavior of ProxyField's Get / Set API.
class ProxyFieldGetSetFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        ON_CALL(get_method_binding_mock_, GetReturnValueBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{return_type_buffer_.data(), return_type_buffer_.size()}}));
        ON_CALL(get_method_binding_mock_, GetInArgsBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{in_args_buffer_.data(), in_args_buffer_.size()}}));
        ON_CALL(get_method_binding_mock_, DoCall(0)).WillByDefault(Return(score::ResultBlank{}));

        ON_CALL(set_method_binding_mock_, GetReturnValueBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{return_type_buffer_.data(), return_type_buffer_.size()}}));
        ON_CALL(set_method_binding_mock_, GetInArgsBuffer(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{in_args_buffer_.data(), in_args_buffer_.size()}}));
        ON_CALL(set_method_binding_mock_, DoCall(0)).WillByDefault(Return(score::ResultBlank{}));
    }

    ProxyField<TestSampleType, WithGetter> CreateFieldWithGetOnly(const std::string_view name = "TestField")
    {
        return ProxyField<TestSampleType, WithGetter>{
            name,
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_mock_),
            nullptr,
            std::make_unique<mock_binding::ProxyMethodFacade>(get_method_binding_mock_)};
    }

    ProxyField<TestSampleType, WithSetter, WithNotifier> CreateFieldWithSetAndNotifier(
        const std::string_view name = "TestField")
    {
        return ProxyField<TestSampleType, WithSetter, WithNotifier>{
            name,
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_mock_),
            std::make_unique<mock_binding::ProxyMethodFacade>(set_method_binding_mock_)};
    }

    alignas(8) std::array<std::byte, 1024U> return_type_buffer_{};
    alignas(8) std::array<std::byte, 1024U> in_args_buffer_{};
    RuntimeMockGuard runtime_mock_guard_{};
    mock_binding::ProxyEvent<TestSampleType> proxy_event_mock_{};
    mock_binding::ProxyMethod get_method_binding_mock_{};
    mock_binding::ProxyMethod set_method_binding_mock_{};
};

TEST_F(ProxyFieldGetSetFixture, GetDelegatesToProxyMethodBinding)
{
    // Given a Get-enabled ProxyField and a mock method binding with a value pre-written into the return buffer
    auto field = CreateFieldWithGetOnly();
    const TestSampleType expected_value{123U};
    return_type_buffer_[0] = static_cast<std::byte>(expected_value);

    // Expecting that the binding returns a buffer for the return value and then calls the method
    EXPECT_CALL(get_method_binding_mock_, GetReturnValueBuffer(0U));
    EXPECT_CALL(get_method_binding_mock_, DoCall(0U));

    // When calling Get()
    auto result = field.Get();

    // Then the call succeeds and returns the buffered value
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), expected_value);
}

TEST_F(ProxyFieldGetSetFixture, SetDelegatesToProxyMethodBindingAndCopiesValueIntoInArgsBuffer)
{
    // Given a ProxyField with a setter (WithSetter, WithNotifier)
    auto field = CreateFieldWithSetAndNotifier();
    const TestSampleType value{42U};

    // Expecting that the binding returns buffers for in-args and return value and then calls the method
    EXPECT_CALL(set_method_binding_mock_, GetInArgsBuffer(0U));
    EXPECT_CALL(set_method_binding_mock_, GetReturnValueBuffer(0U));
    EXPECT_CALL(set_method_binding_mock_, DoCall(0U));

    // When Set is called
    auto result = field.Set(value);

    // Then the call succeeds and the value lands in the in-args buffer
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(static_cast<TestSampleType>(in_args_buffer_[0]), value);
}

TEST_F(ProxyFieldGetSetFixture, GetPropagatesBindingError)
{
    // Given a Get-enabled ProxyField (WithGetter)
    auto field = CreateFieldWithGetOnly();

    // Expecting that the binding returns an error when asked for the return value buffer
    EXPECT_CALL(get_method_binding_mock_, GetReturnValueBuffer(0U))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // When calling Get()
    auto result = field.Get();

    // Then the error is propagated back to the caller
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(ProxyFieldGetSetFixture, SetPropagatesBindingError)
{
    // Given a Set-enabled ProxyField (WithSetter, WithNotifier)
    auto field = CreateFieldWithSetAndNotifier();
    const TestSampleType value{42U};

    // Expecting that the binding returns an error when asked for the in-args buffer
    EXPECT_CALL(set_method_binding_mock_, GetInArgsBuffer(0U))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // When Set is called
    auto result = field.Set(value);

    // Then the error is propagated back to the caller
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl
