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
#ifndef SCORE_MW_COM_IMPL_MOCKING_PROXY_WRAPPER_CLASS_TEST_VIEW_H
#define SCORE_MW_COM_IMPL_MOCKING_PROXY_WRAPPER_CLASS_TEST_VIEW_H

#include "score/mw/com/impl/traits.h"

#include "score/mw/com/impl/mocking/proxy_event_mock.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"

#include "score/result/result.h"

#include <functional>
#include <queue>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl
{

/// @brief NamedEventMock / NamedFieldMock are constructed in test code and provided to
/// ProxyWrapperClassTestView::Create to allow it to access the mocks based on their event/field names.
///
/// Using std::tuple of std::pairs fails when the tuple contains a single pair because accessing the tuple via
/// std::get<0>(tuple) returns the first element of the pair, instead of the pair itself.
template <typename EventType>
struct NamedProxyEventMock
{
    NamedProxyEventMock(std::string_view event_name_in) : event_name{event_name_in}, mock{} {}

    std::string_view event_name;
    ProxyEventMock<EventType> mock;
};

template <typename FieldType>
struct NamedProxyFieldMock
{
    NamedProxyFieldMock(std::string_view field_name_in) : field_name{field_name_in}, mock{} {}

    std::string_view field_name;
    ProxyEventMock<FieldType> mock;
};

template <typename ProxyWrapperClass>
class ProxyWrapperClassTestView
{
  public:
    /// @brief Test-only creation function which creates a ProxyWrapperClass containing mocked
    /// events / fields.
    ///
    /// ProxyWrapperClass inherits from the user defined service interface and is the object that should be used by
    /// test / application code (it is the type returned by AsProxy<>).
    ///
    /// The template parameters EventTypes and FieldTypes will be deduced from the provided event_mocks and field_mocks,
    /// so they don't have to be explicitly specified:
    ///
    /// e.g. auto proxy = ProxyWrapperClassTestView<MyProxy>::Create(events_tuple, fields_tuple);
    ///
    /// Note. Currently, the mocks are not injected in the Create function (to be implemented in Ticket-218575). However,
    /// event_mocks and field_mocks should still be provided to the Create function so that EventTypes and FieldTypes
    /// can still be deduced by Create which is needed for creating the binding factories. This note can be removed once
    /// Ticket-218575 has been implemented.
    ///
    /// @tparam EventTypes Variadic template pack containing the types of ALL events specified in the interface
    /// @tparam FieldTypes Variadic template pack containing the types of ALL fields specified in the interface
    /// @param event_mocks A tuple containing a NamedEventMock per event in the interface.
    /// @param field_mocks A tuple containing a NamedFieldMock per event in the interface.
    /// @return ProxyWrapperClass containing mocked proxy and mocked events / fields.
    template <typename... EventTypes, typename... FieldTypes>
    static ProxyWrapperClass Create(std::tuple<NamedProxyEventMock<EventTypes>...>& /* event_mocks */,
                                    std::tuple<NamedProxyFieldMock<FieldTypes>...>& /* field_mocks */)
    {
        // Create service element binding factory guards which inject mocks into the binding factories. We rely on the
        // default behaviour that calls to Create on the factories will return nullptrs. This is required since the real
        // factories try to parse required information from a config file.
        [[maybe_unused]] std::tuple<ProxyEventBindingFactoryMockGuard<EventTypes>...> event_binding_factories{};
        [[maybe_unused]] std::tuple<ProxyFieldBindingFactoryMockGuard<FieldTypes>...> field_binding_factories{};

        ProxyWrapperClass proxy{MakeFakeHandle(0U), nullptr};
        /// TODO: We should inject the event and field mocks that were provided to Create within this function (similar
        /// to what is done in SkeletonWrapperClassTestView::Create. This would require that ProxyBase stores a map of
        /// ProxyEventBases / ProxyFieldBases. To be implemented in Ticket-218575.
        return proxy;
    }

    /// @brief Test-only Create call which can be used when an interface does not contain any fields.
    template <typename... EventTypes>
    static ProxyWrapperClass Create(std::tuple<NamedProxyEventMock<EventTypes>...>& event_mocks)
    {
        // Since empty_field_mocks is passed to Create as a non-const reference, when this function returns, the
        // reference to empty_field_mocks will be dangling. However, since its empty, the tuple and its contents are not
        // referenced by the created ProxyWrapperClass and therefore this code is safe.
        std::tuple<> empty_field_mocks{};
        return Create(event_mocks, empty_field_mocks);
    }

    /// @brief Test-only Create call which can be used when an interface does not contain any events.
    template <typename... FieldTypes>
    static ProxyWrapperClass Create(std::tuple<NamedProxyFieldMock<FieldTypes>...>& field_mocks)
    {
        // Since empty_event_mocks is passed to Create as a non-const reference, when this function returns, the
        // reference to empty_event_mocks will be dangling. However, since its empty, the tuple and its contents are not
        // referenced by the created ProxyWrapperClass and therefore this code is safe.
        std::tuple<> empty_event_mocks{};
        return Create(empty_event_mocks, field_mocks);
    }

    /// @brief Injects creation results (which may be errors or mocked proxies) into the ProxyWrapperClass that is
    /// used to template ProxyWrapperClassTestView.
    ///
    /// See section "Injecting errors or mocks into Proxy" in mw/com/design/mocking/design.md for details.
    static void InjectCreationResults(
        std::unordered_map<HandleType, std::queue<Result<ProxyWrapperClass>>> creation_results)
    {
        ProxyWrapperClass::InjectCreationResults(std::move(creation_results));
    }

    static void ClearCreationResults()
    {
        ProxyWrapperClass::ClearCreationResults();
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_PROXY_WRAPPER_CLASS_TEST_VIEW_H
