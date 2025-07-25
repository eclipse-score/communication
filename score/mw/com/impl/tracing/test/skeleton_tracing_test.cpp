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
#include "score/mw/com/impl/skeleton_base.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_field.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_mock.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/mw/com/impl/tracing/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>
#include <vector>

namespace score
{
namespace mw
{
namespace com
{
namespace impl
{
namespace
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::WithArg;

using TestSampleType = std::uint8_t;

const auto kDummyEventName{"DummyEvent"};

const ServiceTypeDeployment kTypeDeployment{score::cpp::blank{}};

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const std::string kServiceTypeName{"foo"};
const auto kServiceIdentifier = make_ServiceIdentifierType(kServiceTypeName, 13, 37);

const ServiceInstanceDeployment kValidInstanceDeployment{kServiceIdentifier,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{0U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};

class MyDummyEvent final : public SkeletonEvent<TestSampleType>
{
  public:
    MyDummyEvent(SkeletonBase& skeleton_base, const score::cpp::string_view event_name)
        : SkeletonEvent<TestSampleType>{skeleton_base, event_name}
    {
    }
};

class MyDummySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    MyDummyEvent dummy_event{*this, kDummyEventName};
};

mock_binding::Skeleton* GetMockBinding(MyDummySkeleton& skeleton) noexcept
{
    auto* const binding_mock_raw = SkeletonBaseView{skeleton}.GetBinding();
    return dynamic_cast<mock_binding::Skeleton*>(binding_mock_raw);
}

class SkeletonBaseTracingFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(runtime_mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));
    }

    InstanceIdentifier GetInstanceIdentifierWithValidBinding()
    {
        return make_InstanceIdentifier(kValidInstanceDeployment, kTypeDeployment);
    }

    void ExpectEventCreation(const InstanceIdentifier& instance_identifier) noexcept
    {
        auto skeleton_event_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();

        event_binding_mock_ = skeleton_event_mock_ptr.get();

        EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                    Create(instance_identifier, _, score::cpp::string_view{kDummyEventName}))
            .WillOnce(Return(ByMove(std::move(skeleton_event_mock_ptr))));
    }

    void CreateSkeleton(const InstanceIdentifier& instance_identifier) noexcept
    {
        ON_CALL(runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        // Expect that both events and the field are created with mock bindings
        ExpectEventCreation(instance_identifier);

        EXPECT_CALL(*event_binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
        skeleton_ = std::make_unique<MyDummySkeleton>(std::make_unique<mock_binding::Skeleton>(), instance_identifier);

        binding_mock_ = GetMockBinding(*skeleton_);
        ASSERT_NE(binding_mock_, nullptr);
    }

    tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView() const noexcept
    {
        const tracing::ServiceElementIdentifierView service_element_identifier_view{
            kServiceTypeName, kDummyEventName, ServiceElementType::EVENT};
        const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view, score::cpp::string_view{kInstanceSpecifier.ToString()}};
        return expected_service_element_instance_identifier_view;
    }

    void ExpectIsTracePointEnabledCalls(const tracing::SkeletonEventTracingData& expected_enabled_trace_points,
                                        const score::cpp::string_view service_type,
                                        const score::cpp::string_view event_name,
                                        const score::cpp::string_view instance_specifier_view) const noexcept
    {
        const std::pair<tracing::SkeletonEventTracePointType, bool> trace_points[] = {
            {tracing::SkeletonEventTracePointType::SEND, expected_enabled_trace_points.enable_send},
            {tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE,
             expected_enabled_trace_points.enable_send_with_allocate}};
        for (const auto& items : trace_points)
        {
            EXPECT_CALL(tracing_filter_config_mock_,
                        IsTracePointEnabled(service_type, event_name, instance_specifier_view, items.first))
                .WillOnce(Return(items.second));
        }
    }

    RuntimeMock runtime_mock_{};
    tracing::RuntimeMockGuard runtime_mock_guard_{runtime_mock_};
    ServiceDiscoveryMock service_discovery_mock_{};

    mock_binding::Skeleton* binding_mock_{nullptr};
    mock_binding::SkeletonEvent<TestSampleType>* event_binding_mock_{nullptr};

    std::unique_ptr<MyDummySkeleton> skeleton_{nullptr};
    tracing::TracingRuntimeMock tracing_runtime_mock_{};
    tracing::TracingFilterConfigMock tracing_filter_config_mock_{};

    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard_{};
};

using SkeletonBaseRegisterShmTracingFixture = SkeletonBaseTracingFixture;
TEST_F(SkeletonBaseRegisterShmTracingFixture, RegisterShmObjectIsTracedIfTracingForSkeletonIsEnabled)
{
    const memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd{10U};
    auto* const shm_memory_start_address = reinterpret_cast<void*>(0x100);

    tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with at least one trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa));

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that GetBindingType is called on the skeleton binding on creation
    EXPECT_CALL(*binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareOffer will be called on the binding with the wrapped handler containing the register shm object
    // trace call
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback_result{};
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _))
        .WillOnce(WithArg<2>(Invoke([&register_shm_object_trace_callback_result](
                                        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback>
                                            provided_register_shm_object_trace_callback_result) -> ResultBlank {
            register_shm_object_trace_callback_result = std::move(provided_register_shm_object_trace_callback_result);
            return {};
        })));

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // Then a shm object is registered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                RegisterShmObject(BindingType::kLoLa,
                                  expected_service_element_instance_identifier_view,
                                  shm_object_fd,
                                  shm_memory_start_address));

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_));
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and the register shm object tracing callback is called
    ASSERT_TRUE(register_shm_object_trace_callback_result.has_value());
    (*register_shm_object_trace_callback_result)(
        kDummyEventName, ServiceElementType::EVENT, shm_object_fd, shm_memory_start_address);
}

TEST_F(SkeletonBaseRegisterShmTracingFixture, RegisterShmObjectIsNotTracedIfTracingForSkeletonIsDisabled)
{
    const memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd{10U};
    auto* const shm_memory_start_address = reinterpret_cast<void*>(0x100);

    tracing::SkeletonEventTracingData expected_enabled_trace_points{};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with no trace points enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that PrepareOffer will be called on the binding with the wrapped handler containing the register shm object
    // trace call
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback_result{};
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _))
        .WillOnce(WithArg<2>(Invoke([&register_shm_object_trace_callback_result](
                                        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback>
                                            provided_register_shm_object_trace_callback_result) -> ResultBlank {
            register_shm_object_trace_callback_result = std::move(provided_register_shm_object_trace_callback_result);
            return {};
        })));

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // Then a shm object is not registered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                RegisterShmObject(BindingType::kLoLa,
                                  expected_service_element_instance_identifier_view,
                                  shm_object_fd,
                                  shm_memory_start_address))
        .Times(0);

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_));
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and the register shm object tracing callback is empty
    ASSERT_FALSE(register_shm_object_trace_callback_result.has_value());
}

using SkeletonBaseUnregisterShmTracingFixture = SkeletonBaseTracingFixture;
TEST_F(SkeletonBaseUnregisterShmTracingFixture, UnregisterShmObjectIsTracedIfTracingForSkeletonIsEnabled)
{
    tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillRepeatedly(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with at least one trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa));

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that GetBindingType is called on the skeleton binding on creation
    EXPECT_CALL(*binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // and that PrepareStopOffer will be called on the binding with the wrapped handler containing the unregister shm
    // object trace call
    score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback> unregister_shm_object_trace_callback_result{};
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_))
        .WillOnce(Invoke([&unregister_shm_object_trace_callback_result](
                             score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback>
                                 provided_unregister_shm_object_trace_callback_result) -> ResultBlank {
            unregister_shm_object_trace_callback_result =
                std::move(provided_unregister_shm_object_trace_callback_result);
            return {};
        }));

    // Then a shm object is unregistered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                UnregisterShmObject(BindingType::kLoLa, expected_service_element_instance_identifier_view));

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and then when stopping the service offering
    skeleton_->StopOfferService();

    // and the unregister shm object tracing callback is called
    ASSERT_TRUE(unregister_shm_object_trace_callback_result.has_value());
    (*unregister_shm_object_trace_callback_result)(kDummyEventName, ServiceElementType::EVENT);
}

TEST_F(SkeletonBaseUnregisterShmTracingFixture, UnregisterShmObjectIsNotTracedIfTracingForSkeletonIsDisabled)
{
    tracing::SkeletonEventTracingData expected_enabled_trace_points{};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillRepeatedly(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with no trace points enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // and that PrepareStopOffer will be called on the binding with the wrapped handler containing the unregister shm
    // object trace call
    score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback> unregister_shm_object_trace_callback_result{};
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_))
        .WillOnce(Invoke([&unregister_shm_object_trace_callback_result](
                             score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback>
                                 provided_unregister_shm_object_trace_callback_result) -> ResultBlank {
            unregister_shm_object_trace_callback_result =
                std::move(provided_unregister_shm_object_trace_callback_result);
            return {};
        }));

    // Then a shm object is not unregistered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                UnregisterShmObject(BindingType::kLoLa, expected_service_element_instance_identifier_view))
        .Times(0);

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // and then when stopping the service offering
    skeleton_->StopOfferService();

    // and the unregister shm object tracing callback is empty
    ASSERT_FALSE(unregister_shm_object_trace_callback_result.has_value());
}

TEST_F(SkeletonBaseUnregisterShmTracingFixture, UnregisterShmObjectIsTracedOnDestructionIfTracingForSkeletonIsEnabled)
{
    tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillRepeatedly(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with at least one trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that GetBindingType is called on the skeleton binding on creation
    EXPECT_CALL(*binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // and that PrepareStopOffer will be called on the binding with the wrapped handler containing the unregister shm
    // object trace call
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_))
        .WillOnce(Invoke([](score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback>
                                provided_unregister_shm_object_trace_callback_result) -> ResultBlank {
            // Expect that the unregister shm object tracing callback is not empty
            EXPECT_TRUE(provided_unregister_shm_object_trace_callback_result.has_value());

            // And we call it after PrepareStopOffer
            provided_unregister_shm_object_trace_callback_result.value()(kDummyEventName, ServiceElementType::EVENT);
            return {};
        }));

    // Then a shm object is unregistered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                UnregisterShmObject(BindingType::kLoLa, expected_service_element_instance_identifier_view));

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // When the skeleton is destroyed
    skeleton_.reset();
}

TEST_F(SkeletonBaseUnregisterShmTracingFixture,
       UnregisterShmObjectIsNotTracedOnDestructionIfTracingForSkeletonIsDisabled)
{
    tracing::SkeletonEventTracingData expected_enabled_trace_points{};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig.
    EXPECT_CALL(runtime_mock_, GetTracingRuntime()).WillRepeatedly(Return(&tracing_runtime_mock_));
    EXPECT_CALL(runtime_mock_, GetTracingFilterConfig()).WillRepeatedly(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with no trace points enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kDummyEventName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // Given a constructed Skeleton with a valid identifier with two events and a field registered with the skeleton
    CreateSkeleton(GetInstanceIdentifierWithValidBinding());

    // and that PrepareOffer gets called on the event binding
    EXPECT_CALL(*binding_mock_, PrepareOffer(_, _, _));
    EXPECT_CALL(*event_binding_mock_, PrepareOffer());

    // and that PrepareStopOffer will be called on the binding with the wrapped handler containing the unregister shm
    // object trace call
    EXPECT_CALL(*binding_mock_, PrepareStopOffer(_))
        .WillOnce(Invoke([](score::cpp::optional<SkeletonBinding::UnregisterShmObjectTraceCallback>
                                provided_unregister_shm_object_trace_callback_result) -> ResultBlank {
            // Expect that the unregister shm object tracing callback is empty
            EXPECT_FALSE(provided_unregister_shm_object_trace_callback_result.has_value());
            return {};
        }));

    // Then a shm object is not unregistered with the tracing runtime
    EXPECT_CALL(tracing_runtime_mock_,
                UnregisterShmObject(BindingType::kLoLa, expected_service_element_instance_identifier_view))
        .Times(0);

    // PrepareStopOffer is called on the skeleton binding and event
    EXPECT_CALL(*event_binding_mock_, PrepareStopOffer());

    // When offering a Service
    const auto offer_result = skeleton_->OfferService();

    // Then no error is returned
    ASSERT_TRUE(offer_result.has_value());

    // When the skeleton is destroyed
    skeleton_.reset();
}

}  // namespace
}  // namespace impl
}  // namespace com
}  // namespace mw
}  // namespace score
