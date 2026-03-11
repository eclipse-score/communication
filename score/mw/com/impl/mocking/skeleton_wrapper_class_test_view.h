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
#ifndef SCORE_MW_COM_IMPL_MOCKING_SKELETON_WRAPPER_CLASS_TEST_VIEW_H
#define SCORE_MW_COM_IMPL_MOCKING_SKELETON_WRAPPER_CLASS_TEST_VIEW_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/mocking/skeleton_base_mock.h"
#include "score/mw/com/impl/mocking/skeleton_event_mock.h"
#include "score/mw/com/impl/mocking/skeleton_field_mock.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/traits.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <functional>
#include <queue>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl
{

/// @brief NamedEventMock / NamedFieldMock are constructed in test code and provided to
/// SkeletonWrapperClassTestView::Create to allow it to access the mocks based on their event/field names.
///
/// Using std::tuple of std::pairs fails when the tuple contains a single pair because accessing the tuple via
/// std::get<0>(tuple) returns the first element of the pair, instead of the pair itself.
template <typename EventType>
class NamedSkeletonEventMock
{
  public:
    NamedSkeletonEventMock(const std::string_view event_name_in) : event_name{event_name_in}, mock{} {}

    std::string_view event_name;
    SkeletonEventMock<EventType> mock;
};

template <typename FieldType>
class NamedSkeletonFieldMock
{
  public:
    NamedSkeletonFieldMock(const std::string_view field_name_in) : field_name{field_name_in}, mock{} {}

    std::string_view field_name;
    SkeletonFieldMock<FieldType> mock;
};

template <typename SkeletonWrapperClass>
class SkeletonWrapperClassTestView
{
  public:
    /// @brief Test-only creation function which creates a SkeletonWrapperClass containing a mocked skeleton, and mocked
    /// events / fields.
    ///
    /// SkeletonWrapperClass inherits from the user defined service interface and is the object that should be used by
    /// test / application code (it is the type returned by AsSkeleton<>).
    ///
    /// The template parameters EventTypes and FieldTypes will be deduced from the provided event_mocks and field_mocks,
    /// so they don't have to be explicitly specified:
    ///
    /// e.g. auto skeleton = SkeletonWrapperClassTestView<MySkeleton>::Create(skeleton_mock, events_tuple,
    /// fields_tuple);
    ///
    /// @tparam EventTypes Variadic template pack containing the types of ALL events specified in the interface
    /// @tparam FieldTypes Variadic template pack containing the types of ALL fields specified in the interface
    /// @param skeleton_mock SkeletonBaseMock which will be injected into the SkeletonBase of the constructed
    /// SkeletonWrapperClass
    /// @param event_mocks A tuple containing a NamedEventMock per event in the interface.
    /// @param field_mocks A tuple containing a NamedFieldMock per event in the interface.
    /// @return SkeletonWrapperClass containing mocked skeleton and mocked events / fields.
    template <typename... EventTypes, typename... FieldTypes>
    static SkeletonWrapperClass Create(SkeletonBaseMock& skeleton_mock,
                                       std::tuple<NamedSkeletonEventMock<EventTypes>...>& event_mocks,
                                       std::tuple<NamedSkeletonFieldMock<FieldTypes>...>& field_mocks)
    {
        // Create service element binding factory guards which inject mocks into the binding factories. We rely on the
        // default behaviour that calls to Create on the factories will return nullptrs. This is required since the real
        // factories try to parse required information from a config file.
        [[maybe_unused]] std::tuple<SkeletonEventBindingFactoryMockGuard<EventTypes>...> event_binding_factories{};
        [[maybe_unused]] std::tuple<SkeletonFieldBindingFactoryMockGuard<FieldTypes>...> field_binding_factories{};

        SkeletonWrapperClass skeleton{MakeFakeInstanceIdentifier(0U), nullptr};

        skeleton.InjectMock(skeleton_mock);
        InjectEventAndFieldMocks(skeleton, event_mocks, field_mocks);

        return skeleton;
    }

    /// @brief Test-only Create call which can be used when an interface does not contain any fields.
    template <typename... EventTypes>
    static SkeletonWrapperClass Create(SkeletonBaseMock& skeleton_mock,
                                       std::tuple<NamedSkeletonEventMock<EventTypes>...>& event_mocks)
    {
        // Since empty_field_mocks is passed to Create as a non-const reference, when this function returns, the
        // reference to empty_field_mocks will be dangling. However, since its empty, the tuple and its contents are not
        // referenced by the created SkeletonWrapperClass and therefore this code is safe.
        std::tuple<> empty_field_mocks{};
        return Create(skeleton_mock, event_mocks, empty_field_mocks);
    }

    /// @brief Test-only Create call which can be used when an interface does not contain any events.
    template <typename... FieldTypes>
    static SkeletonWrapperClass Create(SkeletonBaseMock& skeleton_mock,
                                       std::tuple<NamedSkeletonFieldMock<FieldTypes>...>& field_mocks)
    {
        // Since empty_event_mocks is passed to Create as a non-const reference, when this function returns, the
        // reference to empty_event_mocks will be dangling. However, since its empty, the tuple and its contents are not
        // referenced by the created SkeletonWrapperClass and therefore this code is safe.
        std::tuple<> empty_event_mocks{};
        return Create(skeleton_mock, empty_event_mocks, field_mocks);
    }

    /// @brief Injects creation results (which may be errors or mocked skeletons) into the SkeletonWrapperClass that is
    /// used to template SkeletonWrapperClassTestView.
    ///
    /// See section "Injecting errors or mocks into Skeleton" in mw/com/design/mocking/design.md for details.
    static void InjectCreationResults(std::unordered_map<InstanceSpecifier, std::queue<Result<SkeletonWrapperClass>>>
                                          instance_specifier_creation_results,
                                      std::unordered_map<InstanceIdentifier, std::queue<Result<SkeletonWrapperClass>>>
                                          instance_identifier_creation_results)
    {
        SkeletonWrapperClass::InjectCreationResults(std::move(instance_specifier_creation_results),
                                                    std::move(instance_identifier_creation_results));
    }

    static void ClearCreationResults()
    {
        SkeletonWrapperClass::ClearCreationResults();
    }

  private:
    template <typename SampleType>
    static void InjectEventMock(SkeletonEventBase& event_base, SkeletonEventMock<SampleType>& mock)
    {
        auto* typed_event = dynamic_cast<SkeletonEvent<SampleType>*>(&event_base);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(typed_event != nullptr,
                                     "event_base should always be a fully typed SkeletonEvent!");
        typed_event->InjectMock(mock);
    }

    template <typename SampleType>
    static void InjectFieldMock(SkeletonFieldBase& field_base, SkeletonFieldMock<SampleType>& mock)
    {
        auto* typed_field = dynamic_cast<SkeletonField<SampleType>*>(&field_base);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(typed_field != nullptr, "field_base should always be a fully typed SkeletonField");
        typed_field->InjectMock(mock);
    }

    template <typename... EventTypes, typename... FieldTypes>
    static void InjectEventAndFieldMocks(SkeletonWrapperClass& skeleton,
                                         std::tuple<NamedSkeletonEventMock<EventTypes>...>& event_mocks,
                                         std::tuple<NamedSkeletonFieldMock<FieldTypes>...>& field_mocks)
    {
        auto& events = SkeletonBaseView{skeleton}.GetEvents();
        auto& fields = SkeletonBaseView{skeleton}.GetFields();

        // Note. this assert only checks if there were additional EventTypes / FieldTypes provided (or if an there are
        // multiple EventTypes / FieldTypes of the same type, but at least one event / field of that type is not
        // provided). If the type of an Event or Field was not provided, then a Event/Field binding factory would not
        // have been created so the program likely would have crashed already when creating SkeletonWrapperClass.
        SCORE_LANGUAGE_FUTURECPP_ASSERT(events.size() == sizeof...(EventTypes));
        SCORE_LANGUAGE_FUTURECPP_ASSERT(fields.size() == sizeof...(FieldTypes));

        // std::apply takes a callable and a tuple. It calls the callable with the arguments from the unpacked tuple.
        // E.g. In this case, it will call the lambda, fn, with: `fn(get<0>(args), get<1>(args), ..., get<n>(args))`
        // The second (nested) lambda then takes these unpacked arguments as a parameter pack and uses a fold expression
        // to call the nested lambda, gn, on each argument with `gn(get<0>(args)), gn(get<1>(args)), ...,
        // gn(get<n>(args))`
        std::apply(
            [&events](auto&&... unpacked_tuple) {
                (
                    [&events](auto& event_mock_pair) {
                        auto event_base_it = events.find(std::string{event_mock_pair.event_name});
                        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(event_base_it != events.end());
                        auto& event_base = event_base_it->second.get();
                        InjectEventMock(event_base, event_mock_pair.mock);
                    }(unpacked_tuple),
                    ...);
            },
            event_mocks);

        std::apply(
            [&fields](auto&&... unpacked_tuple) {
                (
                    [&fields](auto& field_mock_pair) {
                        auto field_base_it = fields.find(std::string{field_mock_pair.field_name});
                        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(field_base_it != fields.end());
                        auto& field_base = field_base_it->second.get();
                        InjectFieldMock(field_base, field_mock_pair.mock);
                    }(unpacked_tuple),
                    ...);
            },
            field_mocks);
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_SKELETON_WRAPPER_CLASS_TEST_VIEW_H
