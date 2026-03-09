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
#include "score/mw/com/impl/mocking/proxy_wrapper_class_test_view.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/traits.h"

#include "score/result/result.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <queue>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestEventType = std::int32_t;
using TestEventType2 = float;
using TestFieldType = std::uint64_t;

const std::string_view kEventName{"SomeEventName"};
const std::string_view kEventName2{"SomeEventName2"};
const std::string_view kFieldName{"SomeFieldName"};

template <typename InterfaceTrait>
class MyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Event<TestEventType> some_event{*this, kEventName};
    typename InterfaceTrait::template Event<TestEventType2> some_event_2{*this, kEventName2};
    typename InterfaceTrait::template Field<TestFieldType> some_field{*this, kFieldName};
};
using MyProxy = AsProxy<MyInterface>;

class ProxyWrapperTestClassFixture : public ::testing::Test
{
  public:
    ~ProxyWrapperTestClassFixture()
    {
        ClearCreationResults();
    }

    void AddErrorCode(const HandleType& handle_type, std::vector<ComErrc> error_codes)
    {
        std::queue<Result<MyProxy>> error_results{};

        for (auto error_code : error_codes)
        {
            error_results.push(MakeUnexpected(error_code));
        }
        creation_results_.insert({handle_type, std::move(error_results)});
    }

    void InjectCreationResults()
    {
        ProxyWrapperClassTestView<MyProxy>::InjectCreationResults(std::move(creation_results_));
    }

    void ClearCreationResults()
    {
        ProxyWrapperClassTestView<MyProxy>::ClearCreationResults();
    }

    HandleType handle_1_{MakeFakeHandle(1U)};
    HandleType handle_2_{MakeFakeHandle(2U)};

    std::unordered_map<HandleType, std::queue<Result<MyProxy>>> creation_results_{};
};

using ProxyWrapperTestClassInjectionFixture = ProxyWrapperTestClassFixture;
TEST_F(ProxyWrapperTestClassInjectionFixture, CreateDispatchesToCreationResults)
{
    // Given that multiple creation results were injected for two handles
    AddErrorCode(handle_1_, {ComErrc::kBindingFailure, ComErrc::kServiceNotAvailable});
    AddErrorCode(handle_2_, {ComErrc::kCommunicationStackError});
    InjectCreationResults();

    // When creating the proxys
    auto result_first_handle = MyProxy::Create(handle_1_);
    auto result_second_handle = MyProxy::Create(handle_2_);
    auto result_first_handle_2 = MyProxy::Create(handle_1_);

    // Then the results correspond to the injected creation results
    ASSERT_FALSE(result_first_handle.has_value());
    EXPECT_EQ(result_first_handle.error(), ComErrc::kBindingFailure);

    ASSERT_FALSE(result_first_handle_2.has_value());
    EXPECT_EQ(result_first_handle_2.error(), ComErrc::kServiceNotAvailable);

    ASSERT_FALSE(result_second_handle.has_value());
    EXPECT_EQ(result_second_handle.error(), ComErrc::kCommunicationStackError);
}

TEST_F(ProxyWrapperTestClassInjectionFixture, CallingCreateWithHandleThatWasNotInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance specifier
    AddErrorCode(handle_1_, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // When creating a proxy with a different specifier
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MyProxy::Create(handle_2_), ".*");
}

TEST_F(ProxyWrapperTestClassInjectionFixture, CallingCreateWithHandleMoreTimesThanResultWasInjectedTerminates)
{
    // Given that multiple creation results were injected for an instance specifier
    AddErrorCode(handle_1_, {ComErrc::kBindingFailure});
    InjectCreationResults();

    // and given that a proxy was created with the handle once
    score::cpp::ignore = MyProxy::Create(handle_1_);

    // When creating a proxy with the handle again
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = MyProxy::Create(handle_1_), ".*");
}

using ProxyWrapperTestClassCreateFixture = ProxyWrapperTestClassFixture;
TEST_F(ProxyWrapperTestClassCreateFixture, CreatingMockProxyWithAllEventsAndFieldsReturnsProxy)
{
    // Given a mock per event and field.
    std::tuple<NamedProxyEventMock<TestEventType>, NamedProxyEventMock<TestEventType2>> events_tuple{kEventName,
                                                                                                     kEventName2};
    std::tuple<NamedProxyFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // When creating a mocked proxy
    auto proxy = ProxyWrapperClassTestView<MyProxy>::Create(events_tuple, fields_tuple);

    // Then a proxy is succesfully constructed (i.e. we don't crash during construction)
}

TEST_F(ProxyWrapperTestClassCreateFixture, CallingFunctionsOnMockProxyDispatchesToMocks)
{
    // Given a mock per event and field.
    std::tuple<NamedProxyEventMock<TestEventType>, NamedProxyEventMock<TestEventType2>> events_tuple{kEventName,
                                                                                                     kEventName2};
    std::tuple<NamedProxyFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // and a mocked proxy
    auto proxy = ProxyWrapperClassTestView<MyProxy>::Create(events_tuple, fields_tuple);
    auto& proxy_event_mock = std::get<0>(events_tuple).mock;
    auto& proxy_event_mock_2 = std::get<1>(events_tuple).mock;
    auto& proxy_field_mock = std::get<0>(fields_tuple).mock;
    proxy.some_event.InjectMock(proxy_event_mock);
    proxy.some_event_2.InjectMock(proxy_event_mock_2);
    proxy.some_field.InjectEventMock(proxy_field_mock);

    // Expecting that OfferService will be called on the Proxy mock and Unsubscribe on the event and field mocks
    EXPECT_CALL(proxy_event_mock, Unsubscribe());
    EXPECT_CALL(proxy_event_mock_2, Unsubscribe());
    EXPECT_CALL(proxy_field_mock, Unsubscribe());

    // When calling Unsubscribe on the events and fields.
    proxy.some_event.Unsubscribe();
    proxy.some_event_2.Unsubscribe();
    proxy.some_field.Unsubscribe();
}

template <typename InterfaceTrait>
class EventOnlyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Event<TestEventType> some_event{*this, kEventName};
};
using EventOnlyProxy = AsProxy<EventOnlyInterface>;

using ProxyWrapperTestClassEventsOnlyCreateFixture = ProxyWrapperTestClassCreateFixture;
TEST_F(ProxyWrapperTestClassCreateFixture, CreatingMockProxyWithAllEventsReturnsProxy)
{
    // Given a ProxyMock and a mock per Event
    std::tuple<NamedProxyEventMock<TestEventType>> events_tuple{kEventName};

    // When creating a mocked proxy
    auto proxy = ProxyWrapperClassTestView<EventOnlyProxy>::Create(events_tuple);
    auto& proxy_event_mock = std::get<0>(events_tuple).mock;
    proxy.some_event.InjectMock(proxy_event_mock);

    // Then a proxy is succesfully constructed (i.e. we don't crash during construction)
}

TEST_F(ProxyWrapperTestClassEventsOnlyCreateFixture, CallingFunctionsOnMockProxyDispatchesToMocks)
{
    // Given a ProxyMock and a mock per Event
    std::tuple<NamedProxyEventMock<TestEventType>> events_tuple{kEventName};

    // and given a mocked proxy was created
    auto proxy = ProxyWrapperClassTestView<EventOnlyProxy>::Create(events_tuple);
    auto& proxy_event_mock = std::get<0>(events_tuple).mock;
    proxy.some_event.InjectMock(proxy_event_mock);

    // Expecting that Unsubscribe is called on the event
    EXPECT_CALL(proxy_event_mock, Unsubscribe());

    // When calling Unsubscribe on the event
    proxy.some_event.Unsubscribe();
}

template <typename InterfaceTrait>
class FieldOnlyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Field<TestFieldType> some_field{*this, kFieldName};
};
using FieldOnlyProxy = AsProxy<FieldOnlyInterface>;

using ProxyWrapperTestClassFieldsOnlyCreateFixture = ProxyWrapperTestClassCreateFixture;
TEST_F(ProxyWrapperTestClassFieldsOnlyCreateFixture, CreatingMockProxyWithAllFieldsReturnsProxy)
{
    // Given a ProxyMock and a mock per Field
    std::tuple<NamedProxyFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // When creating a mocked proxy
    auto proxy = ProxyWrapperClassTestView<FieldOnlyProxy>::Create(fields_tuple);

    // Then a proxy is successfully constructed (i.e. we don't crash during construction)
}

TEST_F(ProxyWrapperTestClassFieldsOnlyCreateFixture, CallingFunctionsOnMockProxyDispatchesToMocks)
{
    // Given a ProxyMock and a mock per Field
    std::tuple<NamedProxyFieldMock<TestFieldType>> fields_tuple{kFieldName};

    // and given a mocked proxy was created
    auto proxy = ProxyWrapperClassTestView<FieldOnlyProxy>::Create(fields_tuple);
    auto& proxy_field_mock = (std::get<0>(fields_tuple).mock);
    proxy.some_field.InjectEventMock(proxy_field_mock);

    // Expecting that Unsubscribe is called on the field
    EXPECT_CALL(proxy_field_mock, Unsubscribe());

    // When calling Unsubscribe on the field
    proxy.some_field.Unsubscribe();
}

}  // namespace
}  // namespace score::mw::com::impl
