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
#include "score/mw/com/impl/traits.h"

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"

#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

using TestSampleType = std::uint32_t;

const auto kEventName{"SomeEventName"};
const auto kFieldName{"SomeFieldName"};

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();

const auto kDummyService = make_ServiceIdentifierType("foo", 1U, 0U);
const auto kTestTypeDeployment = ServiceTypeDeployment{score::cpp::blank{}};
const auto kInstanceId{0U};
auto kValidInstanceDeployment =
    ServiceInstanceDeployment{kDummyService,
                              LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                              QualityType::kASIL_QM,
                              kInstanceSpecifier};

template <typename InterfaceTrait>
class MyInterface : public InterfaceTrait::Base
{
  public:
    using InterfaceTrait::Base::Base;

    typename InterfaceTrait::template Event<TestSampleType> some_event{*this, kEventName};
    typename InterfaceTrait::template Field<TestSampleType> some_field{*this, kFieldName};
};

using MyProxy = AsProxy<MyInterface>;
using MySkeleton = AsSkeleton<MyInterface>;

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        Runtime::InjectMock(&runtime_mock_);
        ON_CALL(runtime_mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));
    }
    ~RuntimeMockGuard()
    {
        Runtime::InjectMock(nullptr);
    }

    RuntimeMock runtime_mock_;
    ServiceDiscoveryMock service_discovery_mock_{};
};

class ProxyCreationFixture : public ::testing::Test
{

  public:
    void SetUp() override
    {
        auto proxy_binding_mock_ptr = std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock_);
        auto proxy_event_binding_mock_ptr =
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_binding_mock_);
        auto proxy_field_binding_mock_ptr =
            std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_field_binding_mock_);

        auto& runtime_mock = runtime_mock_guard_.runtime_mock_;
        // By default the runtime configuration has no GetTracingFilterConfig
        ON_CALL(runtime_mock, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        // By default the Create call on the ProxyBindingFactory returns a valid binding.
        ON_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(handle_))
            .WillByDefault(Return(ByMove(std::move(proxy_binding_mock_ptr))));

        // By default the Create call on the ProxyEventBindingFactory returns valid bindings.
        ON_CALL(proxy_event_binding_factory_mock_guard_.factory_mock_, Create(_, kEventName))
            .WillByDefault(Return(ByMove(std::move(proxy_event_binding_mock_ptr))));

        // By default the Create call on the ProxyFieldBindingFactory returns valid bindings.
        ON_CALL(proxy_field_binding_factory_mock_guard_.factory_mock_, CreateEventBinding(_, kFieldName))
            .WillByDefault(Return(ByMove(std::move(proxy_field_binding_mock_ptr))));

        // By default the runtime configuration resolves instance identifiers
        resolved_instance_identifiers_.push_back(identifier_with_valid_binding_);
        ON_CALL(runtime_mock, resolve(kInstanceSpecifier)).WillByDefault(Return(resolved_instance_identifiers_));
    }

    std::vector<InstanceIdentifier> resolved_instance_identifiers_{};
    const InstanceIdentifier identifier_with_valid_binding_{
        make_InstanceIdentifier(kValidInstanceDeployment, kTestTypeDeployment)};
    const HandleType handle_{make_HandleType(identifier_with_valid_binding_)};
    RuntimeMockGuard runtime_mock_guard_{};
    ProxyBindingFactoryMockGuard proxy_binding_factory_mock_guard_{};
    ProxyEventBindingFactoryMockGuard<TestSampleType> proxy_event_binding_factory_mock_guard_{};
    ProxyFieldBindingFactoryMockGuard<TestSampleType> proxy_field_binding_factory_mock_guard_{};
    mock_binding::Proxy proxy_binding_mock_{};
    mock_binding::ProxyEvent<TestSampleType> proxy_event_binding_mock_{};
    mock_binding::ProxyEvent<TestSampleType> proxy_field_binding_mock_{};
};

TEST(GeneratedProxyTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-21290780");
    RecordProperty("Description", "Checks copy semantics for proxies");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<MyProxy>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MyProxy>::value, "Is wrongly copyable");
}

TEST(GeneratedProxyTest, IsMoveable)
{
    RecordProperty("Verifies", "SCR-21290799");
    RecordProperty("Description", "Checks move semantics for proxies");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_constructible<MyProxy>::value, "Is not moveable");
    static_assert(std::is_move_assignable<MyProxy>::value, "Is not moveable");
}

using GeneratedProxyCreationTestFixture = ProxyCreationFixture;
TEST_F(GeneratedProxyCreationTestFixture, ReturnGeneratedProxyWhenSuccessfullyCreatingProxyWithValidBindings)
{
    RecordProperty("Verifies", "SCR-14108458");
    RecordProperty("Description", "Proxy shall be created with Create function.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto proxy_binding_mock_ptr = std::make_unique<mock_binding::ProxyFacade>(proxy_binding_mock_);
    auto proxy_event_binding_mock_ptr =
        std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_event_binding_mock_);
    auto proxy_field_binding_mock_ptr =
        std::make_unique<mock_binding::ProxyEventFacade<TestSampleType>>(proxy_field_binding_mock_);

    // Expecting that valid bindings are created for the Proxy, ProxyEvent and ProxyField
    EXPECT_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(handle_))
        .WillRepeatedly(Return(ByMove(std::move(proxy_binding_mock_ptr))));
    EXPECT_CALL(proxy_event_binding_factory_mock_guard_.factory_mock_, Create(_, kEventName))
        .WillRepeatedly(Return(ByMove(std::move(proxy_event_binding_mock_ptr))));
    EXPECT_CALL(proxy_field_binding_factory_mock_guard_.factory_mock_, CreateEventBinding(_, kFieldName))
        .WillRepeatedly(Return(ByMove(std::move(proxy_field_binding_mock_ptr))));

    // When creating a MyProxy
    auto dummy_proxy_result = MyProxy::Create(std::move(handle_));

    // Then the result should be contain a valid proxy
    ASSERT_TRUE(dummy_proxy_result.has_value());
}

TEST_F(GeneratedProxyCreationTestFixture, ReturnErrorWhenCreatingProxyWithNoProxyBinding)
{
    RecordProperty("Verifies", "SCR-14108458, SCR-31295722, SCR-32158471, SCR-32158442, SCR-33047276");
    RecordProperty(
        "Description",
        "Proxy shall be created with Create function which returns an error if the Proxy binding cannot be created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the ProxyBindingFactory returns a nullptr.
    EXPECT_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(handle_)).WillOnce(Return(ByMove(nullptr)));

    // When creating a MyProxy
    auto dummy_proxy_result = MyProxy::Create(std::move(handle_));

    // Then the result should contain an error
    ASSERT_FALSE(dummy_proxy_result.has_value());
    EXPECT_EQ(dummy_proxy_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedProxyCreationTestFixture, ReturnErrorWhenCreatingProxyWithNoProxyEventBinding)
{
    RecordProperty("Verifies", "SCR-14108458");
    RecordProperty("Description",
                   "Proxy shall be created with Create function which returns an error if a ProxyEvent binding cannot "
                   "be created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the ProxyEventBindingFactory returns an invalid binding for the event.
    EXPECT_CALL(proxy_event_binding_factory_mock_guard_.factory_mock_, Create(_, kEventName))
        .WillOnce(Return(ByMove(nullptr)));

    // When creating a MyProxy
    auto dummy_proxy_result = MyProxy::Create(std::move(handle_));

    // Then the result should contain an error
    ASSERT_FALSE(dummy_proxy_result.has_value());
    EXPECT_EQ(dummy_proxy_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedProxyCreationTestFixture, ReturnErrorWhenCreatingProxyWithNoProxyFieldBinding)
{
    RecordProperty("Verifies", "SCR-14108458");
    RecordProperty("Description",
                   "Proxy shall be created with Create function which returns an error if a ProxyEvent binding cannot "
                   "be created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the ProxyFieldBindingFactory returns an invalid binding for the field
    EXPECT_CALL(proxy_field_binding_factory_mock_guard_.factory_mock_, CreateEventBinding(_, kFieldName))
        .WillOnce(Return(ByMove(nullptr)));

    // When creating a MyProxy
    auto dummy_proxy_result = MyProxy::Create(std::move(handle_));

    // Then the result should contain an error
    ASSERT_FALSE(dummy_proxy_result.has_value());
    EXPECT_EQ(dummy_proxy_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedProxyCreationTestFixture, CallingSubscribeOnServiceElementsDispatchesToBindings)
{

    ON_CALL(proxy_event_binding_mock_, GetSubscriptionState()).WillByDefault(Return(SubscriptionState::kNotSubscribed));
    ON_CALL(proxy_field_binding_mock_, GetSubscriptionState()).WillByDefault(Return(SubscriptionState::kNotSubscribed));

    // Expect that Subscribe is called on each event binding
    EXPECT_CALL(proxy_event_binding_mock_, Subscribe(_));
    EXPECT_CALL(proxy_field_binding_mock_, Subscribe(_));

    // Given a proxy is created from a valid handle
    auto proxy_result = MyProxy::Create(std::move(handle_));
    ASSERT_TRUE(proxy_result.has_value());
    auto& unit = proxy_result.value();

    // When calling subscribe on each event / field
    unit.some_event.Subscribe(1);
    unit.some_field.Subscribe(1);
}

TEST(GeneratedProxyFindServiceTest, GeneratedProxyUsesProxyBaseFindServiceWithInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-14110930");
    RecordProperty("Description", "Checks that a generated proxy uses FindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceSpecifier)>(&ProxyBase::FindService);
    constexpr auto generated_proxy_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceSpecifier)>(&MyProxy::FindService);
    static_assert(proxy_base_find_service == generated_proxy_find_service,
                  "Generated Proxy not using ProxyBase::FindService");
}

TEST(GeneratedProxyFindServiceTest, GeneratedProxyUsesProxyBaseFindServiceWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-14110933");
    RecordProperty("Description", "Checks that a generated proxy uses FindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceIdentifier)>(&ProxyBase::FindService);
    constexpr auto generated_proxy_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceIdentifier)>(&MyProxy::FindService);
    static_assert(proxy_base_find_service == generated_proxy_find_service,
                  "Generated Proxy not using ProxyBase::FindService");
}

TEST(GeneratedProxyStartFindServiceTest, GeneratedProxyUsesProxyBaseStartFindServiceWithInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-21792392");
    RecordProperty("Description",
                   "Checks that a generated proxy uses StartFindService with InstanceSpecifier in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_start_find_service =
        static_cast<score::Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceSpecifier)>(
            &ProxyBase::StartFindService);
    constexpr auto generated_proxy_start_find_service =
        static_cast<Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceSpecifier)>(
            &MyProxy::StartFindService);
    static_assert(proxy_base_start_find_service == generated_proxy_start_find_service,
                  "Generated Proxy not using ProxyBase::StartFindService with InstanceSpecifer");
}

TEST(GeneratedProxyStartFindServiceTest, GeneratedProxyUsesProxyBaseStartFindServiceWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-21792393");
    RecordProperty("Description",
                   "Checks that a generated proxy uses StartFindService with InstanceIdentifier in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_start_find_service =
        static_cast<score::Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceIdentifier)>(
            &ProxyBase::StartFindService);
    constexpr auto generated_proxy_start_find_service =
        static_cast<Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceIdentifier)>(
            &MyProxy::StartFindService);
    static_assert(proxy_base_start_find_service == generated_proxy_start_find_service,
                  "Generated Proxy not using ProxyBase::StartFindService with InstanceIdentifier");
}

TEST(GeneratedProxyStopFindServiceTest, GeneratedProxyUsesProxyBaseStopFindServiceWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-21792394");
    RecordProperty("Description", "Checks that a generated proxy uses StopFindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto* const proxy_base_stop_find_service = &ProxyBase::StopFindService;
    constexpr auto* const generated_proxy_stop_find_service = &MyProxy::StopFindService;
    static_assert(proxy_base_stop_find_service == generated_proxy_stop_find_service,
                  "Generated Proxy not using ProxyBase::StopFindService");
}

TEST(GeneratedProxyHandleTest, GeneratedProxyUsesProxyBaseGetHandle)
{
    RecordProperty("Verifies", "SCR-14110935");
    RecordProperty("Description", "Checks that a generated proxy uses GetHandle in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(&ProxyBase::GetHandle == &MyProxy::GetHandle, "Generated Proxy not using ProxyBase::GetHandle");
}

TEST(GeneratedProxyHandleTest, GeneratedProxyContainsPublicHandleTypeAlias)
{
    RecordProperty("Verifies", "SCR-14110936");
    RecordProperty("Description",
                   "Checks that a generated proxy contains a public alias to our implementation of HandleType.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<typename MyProxy::HandleType, impl::HandleType>::value, "Incorrect HandleType.");
}

TEST(GeneratedSkeletonTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-5897862, SCR-17432387");  // SWS_CM_00134
    RecordProperty("Description", "Checks copy semantics for Skeletons");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<MySkeleton>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<MySkeleton>::value, "Is wrongly copyable");
}

TEST(GeneratedSkeletonTest, IsMoveable)
{
    RecordProperty("Verifies", "SCR-5897869, SCR-17432438");  // SWS_CM_00135
    RecordProperty("Description", "Checks move semantics for Skeletons");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_constructible<MySkeleton>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<MySkeleton>::value, "Is not move assignable");
}

class SkeletonCreationFixture : public ::testing::Test
{

  public:
    void SetUp() override
    {
        auto skeleton_binding_mock_ptr = std::make_unique<mock_binding::SkeletonFacade>(skeleton_binding_mock_);
        auto skeleton_event_binding_mock_ptr =
            std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_event_binding_mock_);
        auto skeleton_field_binding_mock_ptr =
            std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_field_binding_mock_);

        auto& runtime_mock = runtime_mock_guard_.runtime_mock_;
        // By default the runtime configuration has no GetTracingFilterConfig
        ON_CALL(runtime_mock, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

        // By default the Create call on the SkeletonBindingFactory returns a valid binding.
        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(identifier_with_valid_binding_))
            .WillByDefault(Return(ByMove(std::move(skeleton_binding_mock_ptr))));

        // By default the Create call on the SkeletonEventBindingFactory returns valid bindings.
        ON_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(identifier_with_valid_binding_, _, kEventName))
            .WillByDefault(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));

        // By default the Create call on the SkeletonFieldBindingFactory returns valid bindings.
        ON_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(identifier_with_valid_binding_, _, kFieldName))
            .WillByDefault(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

        // By default the runtime configuration resolves instance identifiers
        resolved_instance_identifiers_.push_back(identifier_with_valid_binding_);
        ON_CALL(runtime_mock, resolve(kInstanceSpecifier)).WillByDefault(Return(resolved_instance_identifiers_));
    }

    std::vector<InstanceIdentifier> resolved_instance_identifiers_{};
    const InstanceIdentifier identifier_with_valid_binding_{
        make_InstanceIdentifier(kValidInstanceDeployment, kTestTypeDeployment)};
    RuntimeMockGuard runtime_mock_guard_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard_{};
    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard_{};
    mock_binding::Skeleton skeleton_binding_mock_{};
    mock_binding::SkeletonEvent<TestSampleType> skeleton_event_binding_mock_{};
    mock_binding::SkeletonEvent<TestSampleType> skeleton_field_binding_mock_{};
};

using GeneratedSkeletonCreationInstanceSpecifierTestFixture = SkeletonCreationFixture;
TEST_F(GeneratedSkeletonCreationInstanceSpecifierTestFixture,
       ReturnGeneratedSkeletonWhenSuccessfullyCreatingSkeletonWithValidBindings)
{
    RecordProperty("Verifies", "SCR-17434559");
    RecordProperty("Description", "Checks exception-less creation of skeleton");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto skeleton_binding_mock_ptr = std::make_unique<mock_binding::SkeletonFacade>(skeleton_binding_mock_);
    auto skeleton_event_binding_mock_ptr =
        std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_event_binding_mock_);
    auto skeleton_field_binding_mock_ptr =
        std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_field_binding_mock_);

    // Expecting that valid bindings are created for the Skeleton, SkeletonEvent and SkeletonField
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(identifier_with_valid_binding_))
        .WillOnce(Return(ByMove(std::move(skeleton_binding_mock_ptr))));
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(identifier_with_valid_binding_, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(identifier_with_valid_binding_, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // When constructing a skeleton with an InstanceSpecifier
    const auto unit = MySkeleton::Create(kInstanceSpecifier);

    // Then it is possible to construct a skeleton
    ASSERT_TRUE(unit.has_value());
}

TEST_F(GeneratedSkeletonCreationInstanceSpecifierTestFixture, ReturnErrorWhenCreatingSkeletonWithNoSkeletonBinding)
{
    RecordProperty("Verifies", "SCR-17434559");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the SkeletonBindingFactory returns an invalid binding.
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(identifier_with_valid_binding_))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceSpecifier
    const auto unit = MySkeleton::Create(kInstanceSpecifier);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedSkeletonCreationInstanceSpecifierTestFixture, ReturnErrorWhenCreatingSkeletonWithNoSkeletonEventBinding)
{
    RecordProperty("Verifies", "SCR-17434559");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the
    // SkeletonEventBindingFactory returns an invalid binding for the event.
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(identifier_with_valid_binding_, _, kEventName))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceSpecifier
    const auto unit = MySkeleton::Create(kInstanceSpecifier);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedSkeletonCreationInstanceSpecifierTestFixture, ReturnErrorWhenCreatingSkeletonWithNoSkeletonFieldBinding)
{
    RecordProperty("Verifies", "SCR-17434559");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the SkeletonFieldBindingFactory returns an invalid binding for the field.
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(identifier_with_valid_binding_, _, kFieldName))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceSpecifier
    const auto unit = MySkeleton::Create(kInstanceSpecifier);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST(GeneratedSkeletonCreationInstanceSpecifierDeathTest, ConstructingFromNonexistingSpecifierTerminates)
{
    const auto constructing_from_non_existing_specifier = [] {
        RuntimeMockGuard runtime_mock_guard{};
        auto& runtime_mock = runtime_mock_guard.runtime_mock_;

        // Given a runtime resolving no configuration
        std::vector<InstanceIdentifier> resolved_instance_identifiers{};
        EXPECT_CALL(runtime_mock, resolve(kInstanceSpecifier)).WillOnce(Return(resolved_instance_identifiers));

        // Then when constructing a skeleton with an InstanceSpecifier that doesn't correspond to an existing
        // instance_identifier we terminate
        score::cpp::ignore = MySkeleton::Create(kInstanceSpecifier);
    };
    EXPECT_DEATH(constructing_from_non_existing_specifier(), ".*");
}

using GeneratedSkeletonCreationInstanceIdentifierTestFixture = SkeletonCreationFixture;
TEST_F(GeneratedSkeletonCreationInstanceIdentifierTestFixture, ConstructingFromExistingValidSpecifierCreatesSkeleton)
{
    RecordProperty("Verifies", "SCR-18447605");
    RecordProperty("Description", "Checks exception-less creation of skeleton");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto skeleton_binding_mock_ptr = std::make_unique<mock_binding::SkeletonFacade>(skeleton_binding_mock_);
    auto skeleton_event_binding_mock_ptr =
        std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_event_binding_mock_);
    auto skeleton_field_binding_mock_ptr =
        std::make_unique<mock_binding::SkeletonEventFacade<TestSampleType>>(skeleton_field_binding_mock_);

    // Expecting that valid bindings are created for the Skeleton, SkeletonEvent and SkeletonField
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(identifier_with_valid_binding_))
        .WillOnce(Return(ByMove(std::move(skeleton_binding_mock_ptr))));
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(identifier_with_valid_binding_, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(identifier_with_valid_binding_, _, kFieldName))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));

    // When constructing a skeleton with an InstanceIdentifier
    const auto unit = MySkeleton::Create(identifier_with_valid_binding_);

    // Then it is possible to construct a skeleton
    ASSERT_TRUE(unit.has_value());
}

TEST_F(GeneratedSkeletonCreationInstanceIdentifierTestFixture, ConstructingFromInvalidSkeletonReturnsError)
{
    RecordProperty("Verifies", "SCR-18447605");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the SkeletonBindingFactory returns an invalid binding.
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(identifier_with_valid_binding_))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceIdentifier
    const auto unit = MySkeleton::Create(identifier_with_valid_binding_);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedSkeletonCreationInstanceIdentifierTestFixture, ConstructingFromInvalidSkeletonEventReturnsError)
{
    RecordProperty("Verifies", "SCR-18447605");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the
    // SkeletonEventBindingFactory returns an invalid binding for the event.
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard_.factory_mock_,
                Create(identifier_with_valid_binding_, _, kEventName))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceIdentifier
    const auto unit = MySkeleton::Create(identifier_with_valid_binding_);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedSkeletonCreationInstanceIdentifierTestFixture, ConstructingFromInvalidSkeletonFieldReturnsError)
{
    RecordProperty("Verifies", "SCR-18447605");
    RecordProperty("Description",
                   "Checks that exception-less creation of skeleton returns a kBindingFailure on failure to create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the Create call on the SkeletonFieldBindingFactory returns an invalid binding for the field.
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard_.factory_mock_,
                CreateEventBinding(identifier_with_valid_binding_, _, kFieldName))
        .WillOnce(Return(ByMove(nullptr)));

    // When constructing a skeleton with an InstanceIdentifier
    const auto unit = MySkeleton::Create(identifier_with_valid_binding_);

    // Then it is _not_ possible to construct a skeleton
    ASSERT_FALSE(unit.has_value());
    ASSERT_EQ(unit.error(), ComErrc::kBindingFailure);
}

TEST_F(GeneratedSkeletonCreationInstanceIdentifierTestFixture, CanInterpretAsSkeleton)
{
    const TestSampleType field_value{10};
    const TestSampleType event_value{20};

    // Expect that GetBindingType is called on the event binding once for the event and once for the field
    EXPECT_CALL(skeleton_event_binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));
    EXPECT_CALL(skeleton_field_binding_mock_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that Send is called on the event binding once for the event and once for the field
    EXPECT_CALL(skeleton_event_binding_mock_, Send(event_value, _));
    EXPECT_CALL(skeleton_field_binding_mock_, Send(field_value, _));

    // And that PrepareOffer is called on the skeleton binding and event / field
    EXPECT_CALL(skeleton_binding_mock_, PrepareOffer(_, _, _))
        .WillOnce(
            Invoke([](SkeletonBinding::SkeletonEventBindings& events,
                      SkeletonBinding::SkeletonFieldBindings& fields,
                      std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> trace_callback) -> ResultBlank {
                EXPECT_EQ(events.size(), 1);
                const auto& event_it = events.begin();
                EXPECT_EQ(event_it->first, kEventName);

                EXPECT_EQ(fields.size(), 1);
                const auto& field_it = fields.begin();
                EXPECT_EQ(field_it->first, kFieldName);

                EXPECT_FALSE(trace_callback.has_value());

                return {};
            }));
    EXPECT_CALL(skeleton_event_binding_mock_, PrepareOffer());
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareOffer());

    // And that PrepareStopOffer is called on the skeleton binding and event / field on destruction
    EXPECT_CALL(skeleton_binding_mock_, PrepareStopOffer(_));
    EXPECT_CALL(skeleton_event_binding_mock_, PrepareStopOffer());
    EXPECT_CALL(skeleton_field_binding_mock_, PrepareStopOffer());

    // When creating a skeleton from a valid instance identifier
    auto skeleton_result = MySkeleton::Create(identifier_with_valid_binding_);
    ASSERT_TRUE(skeleton_result.has_value());
    auto& unit = skeleton_result.value();

    // and updating the field value
    unit.some_field.Update(field_value);

    // and offering the service
    const auto result = unit.OfferService();
    EXPECT_TRUE(result.has_value());

    // and sending a new event value
    unit.some_event.Send(event_value);

    // Then we don't crash
}

}  // namespace
}  // namespace score::mw::com::impl
