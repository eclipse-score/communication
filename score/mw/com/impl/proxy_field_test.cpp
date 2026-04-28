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

#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/proxy_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestSampleType = std::uint8_t;

namespace
{
// SFINAE detection traits used to verify that the notifier surface is gated on the WithNotifier tag.
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
}  // namespace

TEST(ProxyFieldTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-17397027");
    RecordProperty("Description", "Checks copy semantics for ProxyField");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<ProxyField<TestSampleType, WithGetter, WithNotifier>>::value,
                  "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<ProxyField<TestSampleType, WithGetter, WithNotifier>>::value,
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

    using FirstProxyFieldType = ProxyField<bool, WithGetter, WithNotifier>;
    using SecondProxyFieldType = ProxyField<std::uint16_t, WithGetter, WithNotifier>;
    static_assert(!std::is_same_v<FirstProxyFieldType, SecondProxyFieldType>,
                  "Class type does not depend on field data type");
}

TEST(ProxyFieldTest, ClassTypeDependsOnFieldTagType)
{
    using FirstProxyFieldType = ProxyField<bool, WithGetter, WithNotifier>;
    using SecondProxyFieldType = ProxyField<bool, WithGetter, WithSetter>;
    static_assert(!std::is_same_v<FirstProxyFieldType, SecondProxyFieldType>,
                  "Class type does not depend on field tag type");
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
    static_assert(
        std::is_same<typename ProxyField<CustomFieldType, WithGetter, WithNotifier>::FieldType, CustomFieldType>::value,
        "Incorrect FieldType.");
}

TEST(ProxyFieldNotifierGatingTest, NotifierSurfaceExistsWhenWithNotifierTagIsPresent)
{

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

TEST(ProxyFieldNotifierGatingTest, NotifierSurfaceIsAbsentWhenWithNotifierTagIsMissing)
{

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

}  // namespace
}  // namespace score::mw::com::impl
