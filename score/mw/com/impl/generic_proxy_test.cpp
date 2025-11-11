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
/// This file contains unit tests for functionality that is unique to GenericProxy.
/// There is additional test functionality in the following locations:
///     - score/mw/com/impl/proxy_base_test.cpp contains unit tests which contain paramaterised tests to re-use
///     the Proxy tests for testing functionality common between Proxy and GenericProxy.

#include "score/mw/com/impl/generic_proxy.h"

#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{

namespace test
{

class GenericProxyAttorney
{
  public:
    GenericProxyAttorney(GenericProxy& generic_proxy) noexcept : generic_proxy_{generic_proxy} {}

    void FillEventMap(const std::vector<std::string_view>& event_names) noexcept
    {
        generic_proxy_.FillEventMap(event_names);
    }

  private:
    GenericProxy& generic_proxy_;
};

}  // namespace test

namespace
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

using std::string_view_literals::operator""sv;

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
constexpr auto kServiceTypeName = "/bmw/ncar/services/TirePressureService"sv;
const auto kServiceIdentifierType = make_ServiceIdentifierType(std::string{kServiceTypeName}, 13, 37);
const LolaServiceInstanceId kInstanceId{23U};
const std::uint16_t kServiceId{34U};

const std::string kEventName1{"DummyEvent1"};
const std::string kEventName2{"DummyEvent2"};
const std::string kEventName3{"DummyEvent3"};

ServiceInstanceDeployment CreateServiceInstanceDeploymentWithLolaBinding()
{
    return ServiceInstanceDeployment{
        kServiceIdentifierType, LolaServiceInstanceDeployment{kInstanceId}, QualityType::kASIL_B, kInstanceSpecifier};
}

ServiceTypeDeployment CreateServiceTypeDeploymentWithLolaBinding(const std::vector<std::string>& event_names)
{
    LolaServiceTypeDeployment lola_service_type_deployment{kServiceId};
    for (LolaServiceId id = 0; id < event_names.size(); ++id)
    {
        lola_service_type_deployment.events_.emplace(std::make_pair(event_names[id], id));
    }
    return ServiceTypeDeployment{lola_service_type_deployment};
}

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        Runtime::InjectMock(&runtime_mock_);
    }
    ~RuntimeMockGuard()
    {
        Runtime::InjectMock(nullptr);
    }

    RuntimeMock runtime_mock_;
};

TEST(GenericProxyTest, NotCopyable)
{
    RecordProperty("Verifies", "SCR-17292012");
    RecordProperty("Description", "Checks copy semantics for GenericProxies");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(!std::is_copy_constructible<GenericProxy>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<GenericProxy>::value, "Is wrongly copyable");
}

TEST(GenericProxyTest, IsMoveable)
{
    RecordProperty("Verifies", "SCR-17292032");
    RecordProperty("Description", "Checks move semantics for GenericProxies");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_move_constructible<GenericProxy>::value, "Is not moveable");
    static_assert(std::is_move_assignable<GenericProxy>::value, "Is not moveable");
}

TEST(GenericProxyTest, ServiceElementsAreIndexedUsingElementFqId)
{
    RecordProperty("Verifies", "SCR-21205130");
    RecordProperty(
        "Description",
        "Checks that GenericProxyEvents are stored in a std::map within GenericProxy. A std::map is provided by the "
        "standard library which is ASIL-B certified. The standard requires that inserting elements e.g. via insert() "
        "or emplace will only insert the element if the key does not exist. It will not _mistakenly_ overwrite the "
        "value of an element with a different key. Using operator[] would allowing overwriting the value corresponding "
        "to an existing key so another test will ensure that trying to fill the event map multiple times with the same "
        "key will abort.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ActualEventMapType = GenericProxy::EventMap::map_type;
    using ExpectedEventMapType = std::map<std::string_view, GenericProxyEvent>;
    static_assert(std::is_same_v<ActualEventMapType, ExpectedEventMapType>, "GenericProxy Event map is not a std::map");
}

class GenericProxyFixture : public ::testing::Test
{
  public:
    GenericProxyFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
            .WillByDefault(ReturnRef(service_discovery_mock_));

        // We want the factory_mock_ to return the proxy_binding_mock_ptr by default. However, we want to make sure that
        // it's not called more than once, as we move the proxy_binding_mock_ptr in the first invocation. Therefore, we
        // use an EXPECT_CALL instead of an ON_CALL.
        auto proxy_binding_mock_ptr{std::make_unique<mock_binding::Proxy>()};
        SetProxyBindingMock(proxy_binding_mock_ptr);
        EXPECT_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(_))
            .Times(AtMost(1))
            .WillRepeatedly(Return(ByMove(std::move(proxy_binding_mock_ptr))));

        // This ideally would be an ON_CALL as we want Create to create a mock binding and return it by default.
        // However, we get a "unknown file: Failure" while running the test in that case. Changing it to an EXPECT_CALL
        // solves the problem.
        EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, _))
            .Times(AnyNumber())
            .WillRepeatedly([]() {
                return std::make_unique<mock_binding::GenericProxyEvent>();
            });

        ON_CALL(*proxy_binding_mock_, IsEventProvided(_)).WillByDefault(Return(true));
    }

    GenericProxyFixture& CreateAHandle(const std::vector<std::string>& event_names)
    {
        service_instance_deployment_ =
            std::make_unique<ServiceInstanceDeployment>(CreateServiceInstanceDeploymentWithLolaBinding());
        service_type_deployment_ =
            std::make_unique<ServiceTypeDeployment>(CreateServiceTypeDeploymentWithLolaBinding(event_names));
        const auto service_instance_deployment{CreateServiceInstanceDeploymentWithLolaBinding()};
        const auto instance_identifier =
            make_InstanceIdentifier(*service_instance_deployment_, *service_type_deployment_);
        handle_ = std::make_unique<HandleType>(make_HandleType(instance_identifier));
        return *this;
    }

    void SetProxyBindingMock(std::unique_ptr<mock_binding::Proxy>& proxy_binding_mock_ptr)
    {
        proxy_binding_mock_ = proxy_binding_mock_ptr.get();
    }

    std::unique_ptr<ServiceInstanceDeployment> service_instance_deployment_{nullptr};
    std::unique_ptr<ServiceTypeDeployment> service_type_deployment_{nullptr};
    std::unique_ptr<HandleType> handle_{nullptr};

    RuntimeMockGuard runtime_mock_guard_{};
    ServiceDiscoveryMock service_discovery_mock_{};
    ProxyBindingFactoryMockGuard proxy_binding_factory_mock_guard_{};
    GenericProxyEventBindingFactoryMockGuard generic_proxy_event_binding_guard_{};

    mock_binding::Proxy* proxy_binding_mock_{nullptr};
};

TEST_F(GenericProxyFixture, CanSetupFixture) {}

TEST_F(GenericProxyFixture, CreatingGenericProxyWithValidProxyBindingReturnsValidResult)
{
    RecordProperty("Verifies", "SCR-14005969");
    RecordProperty("Description",
                   "Checks that a valid GenericProxy can be created from a valid HandleType and binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // When constructing the generic proxy from the handle
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // Then a valid GenericProxy will be created
    EXPECT_TRUE(generic_proxy_result.has_value());
}

TEST_F(GenericProxyFixture, CreatingGenericProxyWithConfigurationContainingBlankBindingReturnsValidResult)
{
    // Given a handle created from a configuration containing a blank binding
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder{};
    const auto instance_identifier_with_blank_binding =
        dummy_instance_identifier_builder.CreateBlankBindingInstanceIdentifier();
    const auto handle_with_blank_binding =
        make_HandleType(instance_identifier_with_blank_binding, ServiceInstanceId{kInstanceId});

    // When constructing the generic proxy from the handle
    auto generic_proxy_result = GenericProxy::Create(handle_with_blank_binding);

    // Then a valid GenericProxy will be created
    EXPECT_TRUE(generic_proxy_result.has_value());
}

TEST_F(GenericProxyFixture, CreatingGenericProxyWithNoGenericProxyBindingReturnsError)
{
    RecordProperty("Verifies", "SCR-14005969");
    RecordProperty(
        "Description",
        "Checks that creating a GenericProxy returns an error if the GenericProxy binding cannot be created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    // and that the Create call on the ProxyBindingFactory returns a nullptr.
    CreateAHandle({kEventName1, kEventName2, kEventName3});
    EXPECT_CALL(proxy_binding_factory_mock_guard_.factory_mock_, Create(*handle_)).WillOnce(Return(ByMove(nullptr)));

    // When creating a GenericProxy
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // Then the result should contain an error
    ASSERT_FALSE(generic_proxy_result.has_value());
    EXPECT_EQ(generic_proxy_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericProxyFixture, CreatingGenericProxyWithNoGenericProxyEventBindingReturnsError)
{
    RecordProperty("Verifies", "SCR-14005969");
    RecordProperty(
        "Description",
        "Checks that creating a GenericProxy returns an error if the GenericProxyEvent binding cannot be created.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    // and that the Create call on the ProxyEventBindingFactory returns a nullptr.
    CreateAHandle({kEventName1, kEventName2, kEventName3});
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName1}))
        .WillRepeatedly(Return(ByMove(nullptr)));

    // When creating a GenericProxy
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // Then the result should contain an error
    ASSERT_FALSE(generic_proxy_result.has_value());
    EXPECT_EQ(generic_proxy_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericProxyFixture, GenericProxyWillCreateEventBindingsSpecifiedInHandleType)
{
    RecordProperty("Verifies", "SCR-15600146");
    RecordProperty("Description",
                   "Checks that the GenericProxy will create a GenericProxyEvent binding for each event listed in the "
                   "HandleType used to "
                   "create the GenericProxy.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // Then bindings are created for the provided events
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName1}))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericProxyEvent>())));
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName2}))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericProxyEvent>())));
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName3}))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericProxyEvent>())));

    // When constructing the generic proxy from the handle
    score::cpp::ignore = GenericProxy::Create(*handle_);
}

TEST_F(GenericProxyFixture, GenericProxyWillContainEventsSpecifiedInHandleType)
{
    RecordProperty("Verifies", "SCR-15600146, SCR-14006006");
    RecordProperty(
        "Description",
        "Checks that the GenericProxy will contain a GenericProxyEvent for each event listed in the HandleType used to "
        "create the GenericProxy (SCR-15600146). GetEvents will return the events contained in the GenericProxy "
        "(SCR-14006006).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // When constructing the generic proxy from the handle
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // Then the GenericProxy will contain the events specified in the handle
    ASSERT_TRUE(generic_proxy_result.has_value());
    ASSERT_EQ(generic_proxy_result.value().GetEvents().size(), 3);
}

TEST_F(GenericProxyFixture, GenericProxyWillOnlyCreateEventBindingsForEventsProvidedInSharedMemory)
{
    RecordProperty("Verifies", "SCR-15603826");
    RecordProperty("Description",
                   "Checks that the GenericProxy will only create a GenericProxyEvent binding for events that are "
                   "provided in shared memory.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // When only 2 of the events are provided in shared memory
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName1})).WillByDefault(Return(true));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName2})).WillByDefault(Return(false));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName3})).WillByDefault(Return(true));

    // Then bindings are only created for the provided events
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName1}))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericProxyEvent>())));
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName2})).Times(0);
    EXPECT_CALL(generic_proxy_event_binding_guard_.factory_mock_, Create(_, std::string_view{kEventName3}))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericProxyEvent>())));

    // When constructing the generic proxy
    score::cpp::ignore = GenericProxy::Create(*handle_);
}

TEST_F(GenericProxyFixture, GenericProxyWillContainEventsForEventsProvidedInSharedMemory)
{
    RecordProperty("Verifies", "SCR-15603826, SCR-14006006");
    RecordProperty(
        "Description",
        "Checks that the GenericProxy will only contain a GenericProxyEvent for events that are provided in shared "
        "memory (SCR-15603826). GetEvents will return the events contained in the GenericProxy (SCR-14006006).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // When only 2 of the events are provided in shared memory
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName1})).WillByDefault(Return(true));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName2})).WillByDefault(Return(false));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName3})).WillByDefault(Return(true));

    // When constructing the generic proxy
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // Then the GenericProxy will only contain the events that were provided in shared memory
    ASSERT_TRUE(generic_proxy_result.has_value());
    ASSERT_EQ(generic_proxy_result.value().GetEvents().size(), 2);
}

TEST_F(GenericProxyFixture, GenericProxyWillLogErrorMessageForEventsProvidedInConfigurationButNotInSharedMemory)
{
    RecordProperty("Verifies", "SCR-15603826");
    RecordProperty("Description",
                   "Checks that the GenericProxy will log an error message if an event is provided in the "
                   "configuration but not in shared memory.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a handle created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});

    // When only 2 of the events are provided in shared memory
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName1})).WillByDefault(Return(true));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName2})).WillByDefault(Return(false));
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName3})).WillByDefault(Return(true));

    // capture stdout output during Parse() call.
    testing::internal::CaptureStdout();

    // When constructing the generic proxy
    auto generic_proxy_result = GenericProxy::Create(*handle_);

    // stop capture and get captured data.
    std::string log_output = testing::internal::GetCapturedStdout();

    // Then the an error message should be logged
    const char text_snippet[] =
        "log error verbose 1 GenericProxy: Event provided in the ServiceTypeDeployment could not be found in shared "
        "memory. This is likely a configuration error.";
    auto text_location = log_output.find(text_snippet);
    EXPECT_TRUE(text_location != log_output.npos);
    EXPECT_TRUE(log_output.find(text_snippet, text_location) != log_output.npos);
}

using GenericProxyDeathTest = GenericProxyFixture;
TEST_F(GenericProxyDeathTest, FillingEventMapWithDuplicateEventNamesWillTerminate)
{
    RecordProperty("Verifies", "SCR-15603826");
    RecordProperty("Description",
                   "Checks that the function used to add GenericProxyEvents to the event map of GenericProxy i.e. "
                   "FillEventMap() will terminate if an event list containing duplicate event names is provided.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a GenericProxy created from valid instance and type deployments
    CreateAHandle({kEventName1, kEventName2, kEventName3});
    auto generic_proxy = GenericProxy::Create(*handle_).value();

    // and the event is provided in shared memory
    ON_CALL(*proxy_binding_mock_, IsEventProvided(std::string_view{kEventName1})).WillByDefault(Return(true));

    // When trying to fill the event map with duplicate event names
    // Then the process should terminate
    EXPECT_DEATH(test::GenericProxyAttorney{generic_proxy}.FillEventMap({kEventName1, kEventName1}), ".*");
}

TEST(GenericProxyHandleTest, GenericProxyUsesProxyBaseGetHandle)
{
    RecordProperty("Verifies", "SCR-14030261");
    RecordProperty("Description", "Checks that GenericProxy uses GetHandle in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(&ProxyBase::GetHandle == &GenericProxy::GetHandle, "GenericProxy not using ProxyBase::GetHandle");
}

TEST(GenericProxyHandleTest, GenericProxyContainsPublicHandleTypeAlias)
{
    RecordProperty("Verifies", "SCR-14030385");
    RecordProperty("Description", "A GenericProxy contains a public alias to our implementation of HandleType.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<typename GenericProxy::HandleType, impl::HandleType>::value, "Incorrect HandleType.");
}

TEST(GenericProxyFindServiceTest, GenericProxyUsesProxyBaseFindServiceWithInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-14005977");
    RecordProperty("Description", "Checks that GenericProxy uses FindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceSpecifier)>(&ProxyBase::FindService);
    constexpr auto generic_proxy_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceSpecifier)>(
            &GenericProxy::FindService);
    static_assert(proxy_base_find_service == generic_proxy_find_service,
                  "GenericProxy not using ProxyBase::FindService");
}

TEST(GenericProxyFindServiceTest, GenericProxyUsesProxyBaseFindServiceWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-14005991");
    RecordProperty("Description", "Checks that GenericProxy uses FindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceIdentifier)>(&ProxyBase::FindService);
    constexpr auto generic_proxy_find_service =
        static_cast<score::Result<ServiceHandleContainer<HandleType>> (*)(InstanceIdentifier)>(
            &GenericProxy::FindService);
    static_assert(proxy_base_find_service == generic_proxy_find_service,
                  "GenericProxy not using ProxyBase::FindService");
}

TEST(GenericProxyStartFindServiceTest, GeneratedProxyUsesProxyBaseStartFindServiceWithInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-21788695");
    RecordProperty("Description",
                   "Checks that a generic proxy uses StartFindService with InstanceSpecifier in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_start_find_service =
        static_cast<score::Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceSpecifier)>(
            &ProxyBase::StartFindService);
    constexpr auto generic_proxy_start_find_service =
        static_cast<Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceSpecifier)>(
            &GenericProxy::StartFindService);
    static_assert(proxy_base_start_find_service == generic_proxy_start_find_service,
                  "Generic Proxy not using ProxyBase::StartFindService with InstanceSpecifer");
}

TEST(GenericProxyStartFindServiceTest, GeneratedProxyUsesProxyBaseStartFindServiceWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-21790264");
    RecordProperty("Description",
                   "Checks that a generic proxy uses StartFindService with InstanceIdentifier in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto proxy_base_start_find_service =
        static_cast<score::Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceIdentifier)>(
            &ProxyBase::StartFindService);
    constexpr auto generic_proxy_start_find_service =
        static_cast<Result<FindServiceHandle> (*)(FindServiceHandler<HandleType>, InstanceIdentifier)>(
            &GenericProxy::StartFindService);
    static_assert(proxy_base_start_find_service == generic_proxy_start_find_service,
                  "Generic Proxy not using ProxyBase::StartFindService with InstanceIdentifier");
}

TEST(GenericProxyStopFindServiceTest, GeneratedProxyUsesProxyBaseStopFindService)
{
    RecordProperty("Verifies", "SCR-21790756");
    RecordProperty("Description", "Checks that a generic proxy uses StopFindService in ProxyBase");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr auto* const proxy_base_stop_find_service = &ProxyBase::StopFindService;
    constexpr auto* const generic_proxy_stop_find_service = &GenericProxy::StopFindService;
    static_assert(proxy_base_stop_find_service == generic_proxy_stop_find_service,
                  "Generic Proxy not using ProxyBase::StopFindService");
}

TEST(GenericProxyEventMapTest, GenericProxyContainsEventMapClass)
{
    RecordProperty("Verifies", "SCR-14031112");
    RecordProperty("Description", "Checks that GenericProxy contains a public EventMap class");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_class<GenericProxy::EventMap>::value, "GenericProxy does not contain public EventMap class");
}

TEST(GenericProxyEventMapTest, CheckEventMapClassInterface)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description",
                   "Checks that the EventMap class adheres to the required interface and that EventMap is a "
                   "ServiceElementMap. ServiceElementMap unit tests check that EventMap behaves like std::map.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<GenericProxy::EventMap, ServiceElementMap<GenericProxyEvent>>::value,
                  "EventMap type is incorrect");
    using EventMapValueType = GenericProxy::EventMap::value_type;
    static_assert(std::is_same<EventMapValueType::first_type, const std::string_view>::value,
                  "EventMap key type is incorrect");
    static_assert(std::is_same<EventMapValueType::second_type, GenericProxyEvent>::value,
                  "EventMap value type is incorrect");

    // Check that GenericProxy::EventMap contains the required functions
    GenericProxy::EventMap event_map{};
    score::cpp::ignore = event_map.cbegin();
    score::cpp::ignore = event_map.cend();
    score::cpp::ignore = event_map.find(std::string_view{""});
    score::cpp::ignore = event_map.size();
    score::cpp::ignore = event_map.empty();
}

}  // namespace
}  // namespace score::mw::com::impl
