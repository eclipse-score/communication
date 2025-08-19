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
#include "score/mw/com/impl/tracing/skeleton_tracing.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/mw/com/impl/tracing/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include "score/result/result.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl::tracing
{
namespace
{

using namespace ::testing;

const auto kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
const ConfigurationStore kConfigStore{
    kInstanceSpecifier,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    LolaServiceId{1U},
    LolaServiceInstanceId{2U},
};
const auto& kInstanceIdentifier = kConfigStore.GetInstanceIdentifier();
constexpr std::string_view kEventName{"DummyEvent1"};
constexpr std::string_view kFieldName{"DummyField1"};

class MyDummyField : public SkeletonFieldBase
{
  public:
    MyDummyField(SkeletonBase& skeleton_base,
                 const std::string_view field_name,
                 std::unique_ptr<SkeletonEventBindingBase> skeleton_event_base)
        : SkeletonFieldBase{
              skeleton_base,
              field_name,
              std::make_unique<SkeletonEventBase>(skeleton_base, field_name, std::move(skeleton_event_base))}
    {
    }

    bool IsInitialValueSaved() const noexcept override
    {
        return true;
    }

    ResultBlank DoDeferredUpdate() noexcept override
    {
        return {};
    }
};

class SkeletonTracingFixture : public ::testing::Test
{
  public:
    using TestSampleType = std::uint32_t;

    SkeletonTracingFixture()
        : empty_skeleton_{std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier},
          mock_skeleton_binding_{
              *dynamic_cast<mock_binding::Skeleton*>(SkeletonBaseView{empty_skeleton_}.GetBinding())},
          skeleton_event_base_{empty_skeleton_,
                               kEventName,
                               std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>()},
          skeleton_field_base_{empty_skeleton_, kFieldName, std::make_unique<mock_binding::SkeletonEventBase>()},
          events_map_{{kEventName, skeleton_event_base_}},
          fields_map_{{kFieldName, skeleton_field_base_}}
    {
        ON_CALL(mock_skeleton_binding_, GetBindingType).WillByDefault(Return(BindingType::kFake));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    }

    SkeletonTracingFixture& WithTracingEnabledForOneEvent()
    {
        SkeletonEventTracingData tracing_data{};
        tracing_data.enable_send = true;
        SkeletonEventBaseView{skeleton_event_base_}.SetSkeletonEventTracing(tracing_data);
        return *this;
    }

    SkeletonTracingFixture& WithTracingEnabledForOneField()
    {
        SkeletonEventTracingData tracing_data{};
        tracing_data.enable_send = true;
        auto& skeleton_event_base = SkeletonFieldBaseView{skeleton_field_base_}.GetEventBase();
        SkeletonEventBaseView{skeleton_event_base}.SetSkeletonEventTracing(tracing_data);
        return *this;
    }

    TracingRuntimeMock tracing_runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{};

    SkeletonBase empty_skeleton_;
    mock_binding::Skeleton& mock_skeleton_binding_;
    SkeletonEventBase skeleton_event_base_;
    MyDummyField skeleton_field_base_;

    std::map<std::string_view, std::reference_wrapper<SkeletonEventBase>> events_map_;
    std::map<std::string_view, std::reference_wrapper<SkeletonFieldBase>> fields_map_;
};

using SkeletonTracingCreateRegisterShmCallbackFixture = SkeletonTracingFixture;
TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CreatingRegisterShmCallbackWithTracingEnabledForOneEventReturnsCallback)
{
    // Given a RegisterShmObjectCallback with tracing enabled for one event
    WithTracingEnabledForOneEvent();

    // When creating a register shm callback
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then a valid callback should be returned
    EXPECT_TRUE(callback.has_value());
}

TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CreatingRegisterShmCallbackWithTracingEnabledForOneFieldReturnsCallback)
{
    // Given a RegisterShmObjectCallback with tracing enabled for one field
    WithTracingEnabledForOneField();

    // When creating a register shm callback
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then a valid callback should be returned
    EXPECT_TRUE(callback.has_value());
}

TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CreatingRegisterShmCallbackWithTracingDisabledReturnsEmptyOptional)
{
    // Given a RegisterShmObjectCallback with tracing disabled for all service elements

    // When creating a register shm callback
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then an empty optional should be returned
    EXPECT_FALSE(callback.has_value());
}

TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CreatingRegisterShmCallbackWithNoTracingRuntimeReturnsEmptyOptional)
{
    // Given a RegisterShmObjectCallback with tracing enabled for one event
    WithTracingEnabledForOneEvent();

    // Expecting that the Runtime returns a nullptr for the TracingRuntime
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(nullptr));

    // When creating a register shm callback
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then an empty optional should be returned
    EXPECT_FALSE(callback.has_value());
}

TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CallingRegisterShmCallbackWithTracingEnabledForOneEventDispatchesToTracingRuntime)
{
    // Given a RegisterShmObjectCallback that was created with tracing enabled for one event
    WithTracingEnabledForOneEvent();
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Expecting that RegisterShmObject will be called on the TracingRuntime
    EXPECT_CALL(tracing_runtime_mock_, RegisterShmObject(_, _, _, _));

    // When calling the callback
    callback.value()(kEventName, ServiceElementType::EVENT, 1U, nullptr);
}

TEST_F(SkeletonTracingCreateRegisterShmCallbackFixture,
       CallingRegisterShmCallbackWithTracingEnabledForOneFieldDispatchesToTracingRuntime)
{
    // Given a RegisterShmObjectCallback that was created with tracing enabled for one field
    WithTracingEnabledForOneField();
    const auto callback =
        CreateRegisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Expecting that RegisterShmObject will be called on the TracingRuntime
    EXPECT_CALL(tracing_runtime_mock_, RegisterShmObject(_, _, _, _));

    // When calling the callback
    callback.value()(kEventName, ServiceElementType::FIELD, 1U, nullptr);
}

using SkeletonTracingCreateUnregisterShmCallbackFixture = SkeletonTracingFixture;
TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CreatingUnregisterShmCallbackWithTracingEnabledForOneEventReturnsCallback)
{
    // Given a UnregisterShmObjectCallback with tracing enabled for one event
    WithTracingEnabledForOneEvent();

    // When creating a register shm callback
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then a valid callback should be returned
    EXPECT_TRUE(callback.has_value());
}

TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CreatingUnregisterShmCallbackWithTracingEnabledForOneFieldReturnsCallback)
{
    // Given a UnregisterShmObjectCallback with tracing enabled for one field
    WithTracingEnabledForOneField();

    // When creating a register shm callback
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then a valid callback should be returned
    EXPECT_TRUE(callback.has_value());
}

TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CreatingUnregisterShmCallbackWithTracingDisabledReturnsEmptyOptional)
{
    // Given a UnregisterShmObjectCallback with tracing disabled for all service elements

    // When creating a register shm callback
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then an empty optional should be returned
    EXPECT_FALSE(callback.has_value());
}

TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CreatingUnregisterShmCallbackWithNoTracingRuntimeReturnsEmptyOptional)
{
    // Given a UnregisterShmObjectCallback with tracing enabled for one event
    WithTracingEnabledForOneEvent();

    // Expecting that the Runtime returns a nullptr for the TracingRuntime
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(nullptr));

    // When creating a register shm callback
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Then an empty optional should be returned
    EXPECT_FALSE(callback.has_value());
}

TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CallingUnregisterShmCallbackWithTracingEnabledForOneEventDispatchesToTracingRuntime)
{
    // Given a UnregisterShmObjectCallback that was created with tracing enabled for one event
    WithTracingEnabledForOneEvent();
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Expecting that UnregisterShmObject will be called on the TracingRuntime
    EXPECT_CALL(tracing_runtime_mock_, UnregisterShmObject(_, _));

    // When calling the callback
    callback.value()(kEventName, ServiceElementType::EVENT);
}

TEST_F(SkeletonTracingCreateUnregisterShmCallbackFixture,
       CallingUnregisterShmCallbackWithTracingEnabledForOneFieldDispatchesToTracingRuntime)
{
    // Given a UnregisterShmObjectCallback that was created with tracing enabled for one field
    WithTracingEnabledForOneField();
    const auto callback =
        CreateUnregisterShmObjectCallback(kInstanceIdentifier, events_map_, fields_map_, mock_skeleton_binding_);

    // Expecting that UnregisterShmObject will be called on the TracingRuntime
    EXPECT_CALL(tracing_runtime_mock_, UnregisterShmObject(_, _));

    // When calling the callback
    callback.value()(kEventName, ServiceElementType::FIELD);
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
