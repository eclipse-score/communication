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
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"

#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

using namespace ::testing;

const std::string kShmControlPathPrefix{"/lola-ctl-"};
const std::string kShmDataPathPrefix{"/lola-data-"};

const auto service = make_ServiceIdentifierType("foo");
const ServiceTypeDeployment kServiceTypeDeployment{LolaServiceTypeDeployment{0x1234}};
const LolaServiceInstanceId kLolaServiceInstanceId{0x5678};
const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const auto kServiceInstanceDeployment =
    ServiceInstanceDeployment{service,
                              LolaServiceInstanceDeployment{LolaServiceInstanceId{kLolaServiceInstanceId}},
                              QualityType::kASIL_QM,
                              kInstanceSpecifier};

const ElementFqId kDummyElementFqId{0xcdef, 0x5, 0x10, ServiceElementType::EVENT};
const std::size_t kMaxNumSlots{5U};
const std::uint8_t kMaxSubscribers{10U};

const auto kDummyFindServiceHandle = make_FindServiceHandle(10U);

constexpr std::string_view kDummyEventName{"my_dummy_event"};

class ProxyEventBindingFixture : public ProxyMockedMemoryFixture
{
  public:
    ProxyEventBindingFixture& WhichCapturesFindServiceHandler(const InstanceIdentifier& instance_identifier)
    {
        EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{instance_identifier}))
            .WillOnce(Invoke(WithArg<0>([this](auto find_service_handler) {
                find_service_handler_promise_.set_value(std::move(find_service_handler));
                return kDummyFindServiceHandle;
            })));
        return *this;
    }

    std::promise<FindServiceHandler<HandleType>> find_service_handler_promise_{};
};

using ProxyCreationFixture = ProxyMockedMemoryFixture;
TEST_F(ProxyCreationFixture, ProxyCreationReturnsAValidProxy)
{
    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);

    // Then a valid proxy is created
    EXPECT_NE(proxy_, nullptr);
}

TEST_F(ProxyCreationFixture, ProxyCreationOpensSharedMemoryWithoutProvidersIfNotSpecifiedInConfiguration)
{
    // Expecting that the shared memory control and data regions will be opened with empty provider lists
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kShmControlPathPrefix), true, _))
        .WillOnce(WithArg<2>(
            Invoke([this](const auto& provider_list) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_FALSE(provider_list.has_value());
                return fake_data_.control_memory;
            })));
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kShmDataPathPrefix), false, _))
        .WillOnce(WithArg<2>(
            Invoke([this](const auto& provider_list) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                EXPECT_FALSE(provider_list.has_value());
                return fake_data_.data_memory;
            })));

    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);
}

TEST_F(ProxyCreationFixture, ProxyCreationOpensSharedMemoryWithProvidersFromConfiguration)
{
    // Given a valid deployment information which contains a list of allowed providers
    std::vector<uid_t> allowed_qm_providers{10U, 20U};
    LolaServiceInstanceDeployment lola_service_instance_deployment{LolaServiceInstanceId{kLolaServiceInstanceId}};
    lola_service_instance_deployment.allowed_provider_.insert({QualityType::kASIL_QM, allowed_qm_providers});

    const auto service_instance_deployment_with_allowed_providers =
        ServiceInstanceDeployment{service, lola_service_instance_deployment, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto identifier =
        make_InstanceIdentifier(service_instance_deployment_with_allowed_providers, kServiceTypeDeployment);

    // Expecting that the shared memory control and data regions will be opened with provider lists containing providers
    // specified in the configuration
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kShmControlPathPrefix), true, _))
        .WillOnce(WithArg<2>(Invoke([this, &allowed_qm_providers](const auto& provider_list)
                                        -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
            EXPECT_TRUE(provider_list.has_value());
            EXPECT_THAT(provider_list.value(), Contains(allowed_qm_providers[0]));
            EXPECT_THAT(provider_list.value(), Contains(allowed_qm_providers[1]));
            return fake_data_.control_memory;
        })));
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kShmDataPathPrefix), false, _))
        .WillOnce(WithArg<2>(Invoke([this, &allowed_qm_providers](const auto& provider_list)
                                        -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
            EXPECT_TRUE(provider_list.has_value());
            EXPECT_THAT(provider_list.value(), Contains(allowed_qm_providers[0]));
            EXPECT_THAT(provider_list.value(), Contains(allowed_qm_providers[1]));
            return fake_data_.data_memory;
        })));

    // When creating a proxy
    InitialiseProxyWithCreate(identifier);
}

TEST_F(ProxyCreationFixture, SharedMemoryFactoryOpenReturningNullPtrOnProxyCreationReturnsNullPtr)
{
    RecordProperty("Verifies", "SCR-5878624, SCR-32158442, SCR-33047276");
    RecordProperty("Description",
                   "Checks that the Lola Proxy binding returns a nullptr when the shared memory cannot be opened.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the SharedMemoryFactory Open call will return a nullptr
    ON_CALL(shared_memory_factory_mock_guard_.mock_, Open(_, _, _)).WillByDefault(Return(nullptr));

    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_, nullptr);
}

TEST_F(ProxyCreationFixture, ProxyCreationReturnsNullPtrWhenFailedToOpenUsageMarkerFile)
{
    // Expecting that it fails to open service instance usage marker file
    EXPECT_CALL(*fcntl_mock_, open(_, _, _))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(::score::os::Error::createFromErrno(EACCES))));

    // When creating a proxy
    const auto proxy_result = Proxy::Create(make_HandleType(identifier_, ServiceInstanceId{kLolaServiceInstanceId}));

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_result, nullptr);
}

TEST_F(ProxyCreationFixture, ProxyCreationReturnsNullPtrWhenSharedLockOnUsageMarkerFileCannotBeAcquired)
{
    // Expecting that it fails even with retries to get shared lock on service instance usage marker file
    EXPECT_CALL(*fcntl_mock_, flock(_, _))
        .Times(3)
        .WillRepeatedly(::testing::Return(score::cpp::make_unexpected(::score::os::Error::createFromErrno(EWOULDBLOCK))));

    // When creating a proxy
    const auto proxy_result = Proxy::Create(make_HandleType(identifier_, ServiceInstanceId{kLolaServiceInstanceId}));

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_result, nullptr);
}

TEST_F(ProxyCreationFixture, ProxyCreationSucceedsWhenSharedLockOnUsageMarkerFileCanBeAcquiredInRetry)
{
    InSequence in_sequence{};

    // Expecting that flocking of usage marker file fails initially
    EXPECT_CALL(*fcntl_mock_, flock(_, _))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(::score::os::Error::createFromErrno(EWOULDBLOCK))));
    // but succeeds in later calls
    EXPECT_CALL(*fcntl_mock_, flock(_, _)).WillRepeatedly(::testing::Return(score::cpp::blank{}));

    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);

    // Then a valid proxy is created
    EXPECT_NE(proxy_, nullptr);
}

TEST_F(ProxyCreationFixture, CreatingProxyWithoutLolaInstanceDeploymentReturnsNullptr)
{
    // Given a deployment information which contains an instance deployment with blank binding
    const ServiceInstanceDeployment service_instance_deployment_with_blank_binding{
        service, score::cpp::blank{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const auto identifier =
        make_InstanceIdentifier(service_instance_deployment_with_blank_binding, kServiceTypeDeployment);

    // When creating a proxy
    const auto proxy_result = Proxy::Create(make_HandleType(identifier, ServiceInstanceId{kLolaServiceInstanceId}));

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_result, nullptr);
}

TEST_F(ProxyCreationFixture, CreatingProxyWithoutLolaTypeDeploymentReturnsNullptr)
{
    // Given a deployment information which contains a type deployment with blank binding
    const ServiceTypeDeployment service_type_deployment_with_blank_binding{score::cpp::blank{}};
    const auto identifier =
        make_InstanceIdentifier(kServiceInstanceDeployment, service_type_deployment_with_blank_binding);

    // When creating a proxy
    const auto proxy_result = Proxy::Create(make_HandleType(identifier, ServiceInstanceId{kLolaServiceInstanceId}));

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_result, nullptr);
}

TEST_F(ProxyCreationFixture, CreatingProxyWithoutLolaServiceInstanceIdReturnsNullptr)
{
    // Given a deployment information which contains a lola instance deployment which has no instance ID
    const ServiceInstanceDeployment service_instance_deployment_without_instance_id{
        service,
        LolaServiceInstanceDeployment{score::cpp::optional<LolaServiceInstanceId>{}},
        QualityType::kASIL_QM,
        kInstanceSpecifier};
    const auto identifier =
        make_InstanceIdentifier(service_instance_deployment_without_instance_id, kServiceTypeDeployment);

    // When creating a proxy with a handle which also does not contain a lola instance ID
    const auto proxy_result = Proxy::Create(make_HandleType(identifier, ServiceInstanceId{score::cpp::blank{}}));

    // Then the result will be a nullptr
    EXPECT_EQ(proxy_result, nullptr);
}

using ProxyCreationDeathTest = ProxyCreationFixture;
TEST_F(ProxyCreationDeathTest, GettingEventDataControlWithoutInitialisedEventDataControlTerminates)
{
    // Given a fake Skeleton which creates an empty ServiceDataControl

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);

    // Then trying to get the event data control for an event that was not registered in the ServiceDataStorage
    // Will terminate
    const ElementFqId uninitialised_element_fq_id{0xcdef, 0x5, 0x10, ServiceElementType::EVENT};
    EXPECT_DEATH(proxy_->GetEventControl(uninitialised_element_fq_id), ".*");
}

TEST_F(ProxyCreationDeathTest, GettingRawDataStorageWithoutInitialisedEventDataStorageTerminates)
{
    // Given a fake Skeleton which creates an empty ServiceDataStorage

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);

    // Then trying to get the event data storage for an event that was not registered in the ServiceDataStorage
    // Will terminate
    const ElementFqId uninitialised_element_fq_id{0xcdef, 0x5, 0x10, ServiceElementType::EVENT};
    EXPECT_DEATH(proxy_->GetEventDataStorage<SampleType>(uninitialised_element_fq_id), ".*");
}

using ProxyAutoReconnectFixture = ProxyMockedMemoryFixture;
TEST_F(ProxyAutoReconnectFixture, StartFindServiceIsCalledWhenProxyCreateSucceeds)
{
    const auto valid_find_service_handle = make_FindServiceHandle(10U);

    // Expecting that StartFindService is called
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
        .WillOnce(Return(valid_find_service_handle));

    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);
    EXPECT_NE(proxy_, nullptr);
}

TEST_F(ProxyAutoReconnectFixture, StartFindServiceIsNotCalledWhenProxyCreateFails)
{
    // Expecting that the SharedMemoryFactory Open call will return a nullptr
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Open(_, _, _))
        .Times(2)
        .WillRepeatedly(::testing::Return(nullptr));

    // Then expecting that StartFindService will not be called
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_})).Times(0);

    // When creating a proxy
    InitialiseProxyWithCreate(identifier_);

    // and the result will be a nullptr
    EXPECT_EQ(proxy_, nullptr);
}

TEST_F(ProxyAutoReconnectFixture, StopFindServiceIsCalledOnProxyDestruction)
{
    const auto valid_find_service_handle = make_FindServiceHandle(10U);

    // Expecting that StartFindService is called
    ON_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
        .WillByDefault(Return(valid_find_service_handle));

    // Given that StopFindService is called on destruction of the Proxy
    EXPECT_CALL(service_discovery_mock_, StopFindService(valid_find_service_handle));

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);
}

TEST_F(ProxyAutoReconnectFixture, WhenStopFindServiceReturnsErrorOnProxyDestructionProgramDoesNotTerminate)
{
    // Given that StopFindService is called on destruction of the Proxy which returns an error
    ON_CALL(service_discovery_mock_, StopFindService(_))
        .WillByDefault(Return(MakeUnexpected(ComErrc::kServiceNotOffered)));

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);

    // Then the program does not terminate
}

TEST_F(ProxyAutoReconnectFixture, RegisterEventBindingCallsNotifyOnEventWithFalseWhenProviderInitiallyDoesNotExist)
{
    const auto valid_find_service_handle = make_FindServiceHandle(10U);
    mock_binding::ProxyEvent<std::uint8_t> proxy_event{};

    // Expecting that StartFindService is called but the handler is not called since the provider does not exist
    ON_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
        .WillByDefault(Return(valid_find_service_handle));

    // Then expecting that NotifyServiceInstanceChangedAvailability is called on the event with is_available false
    const bool is_available{false};
    EXPECT_CALL(proxy_event, NotifyServiceInstanceChangedAvailability(is_available, _));

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);

    // and the ProxyEvent registers itself with the Proxy
    proxy_->RegisterEventBinding("Event0", proxy_event);
}

TEST_F(ProxyAutoReconnectFixture, RegisterEventBindingCallsNotifyOnEventWithTrueWhenProviderInitiallyExists)
{
    const auto valid_find_service_handle = make_FindServiceHandle(10U);
    mock_binding::ProxyEvent<std::uint8_t> proxy_event{};

    // Expecting that StartFindService is called and synchronously calls handler since provider exists
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
        .WillOnce(WithArg<0>(Invoke([valid_find_service_handle, this](auto find_service_handler) noexcept {
            ServiceHandleContainer<HandleType> service_handle_container{make_HandleType(identifier_)};
            find_service_handler(service_handle_container, valid_find_service_handle);
            return valid_find_service_handle;
        })));

    // and expecting that NotifyServiceInstanceChangedAvailability is called on the event with is_available true
    const bool is_available{true};
    EXPECT_CALL(proxy_event, NotifyServiceInstanceChangedAvailability(is_available, _));

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);

    // and the ProxyEvent registers itself with the Proxy
    proxy_->RegisterEventBinding("Event0", proxy_event);
}

TEST_F(ProxyAutoReconnectFixture, RegisterEventBindingCallsNotifyOnEventWithLatestValueFromFindServiceHandler)
{
    const auto valid_find_service_handle = make_FindServiceHandle(10U);
    mock_binding::ProxyEvent<std::uint8_t> proxy_event_0{};
    mock_binding::ProxyEvent<std::uint8_t> proxy_event_1{};

    // Expecting that StartFindService is called and synchronously calls handler since provider exists
    FindServiceHandler<HandleType> saved_find_service_handler{};
    EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
        .WillOnce(WithArg<0>(
            Invoke([&saved_find_service_handler, valid_find_service_handle, this](auto find_service_handler) noexcept {
                ServiceHandleContainer<HandleType> service_handle_container{make_HandleType(identifier_)};
                find_service_handler(service_handle_container, valid_find_service_handle);
                saved_find_service_handler = std::move(find_service_handler);
                return valid_find_service_handle;
            })));

    // Then expecting that NotifyServiceInstanceChangedAvailability is called on the first event with is_available true
    // during registration
    // Note. We use Expectations and .After instead of an InSequence test since the events are stored in an
    // unordered_map, so the final expectations on proxy_event_0 and proxy_event_1 can occur in any order, although
    // after all of the preceeding calls.
    const bool initial_is_available{true};
    Expectation event_0_initial_notify =
        EXPECT_CALL(proxy_event_0, NotifyServiceInstanceChangedAvailability(initial_is_available, _));

    // and then NotifyServiceInstanceChangedAvailability is called on the first event by the find service handler
    const bool first_find_service_is_available{false};
    Expectation event_0_find_service_notify =
        EXPECT_CALL(proxy_event_0, NotifyServiceInstanceChangedAvailability(first_find_service_is_available, _))
            .After(event_0_initial_notify);

    // Then expecting that NotifyServiceInstanceChangedAvailability is called on the second event with is_available
    // false during registration
    Expectation event_1_find_service_notify =
        EXPECT_CALL(proxy_event_1, NotifyServiceInstanceChangedAvailability(first_find_service_is_available, _))
            .After(event_0_initial_notify, event_0_find_service_notify);

    // and then NotifyServiceInstanceChangedAvailability is called on both events by the second find service handler
    const bool second_find_service_is_available{true};
    EXPECT_CALL(proxy_event_0, NotifyServiceInstanceChangedAvailability(second_find_service_is_available, _))
        .After(event_0_initial_notify, event_0_find_service_notify, event_1_find_service_notify);
    EXPECT_CALL(proxy_event_1, NotifyServiceInstanceChangedAvailability(second_find_service_is_available, _))
        .After(event_0_initial_notify, event_0_find_service_notify, event_1_find_service_notify);

    // When creating a proxy
    InitialiseProxyWithConstructor(identifier_);
    EXPECT_NE(proxy_, nullptr);

    // and the first ProxyEvent registers itself with the Proxy
    proxy_->RegisterEventBinding("Event0", proxy_event_0);

    // And then the FindService handler is called with an empty service handle container
    ServiceHandleContainer<HandleType> empty_service_handle_container{};
    saved_find_service_handler(empty_service_handle_container, valid_find_service_handle);

    // and the second ProxyEvent registers itself with the Proxy
    proxy_->RegisterEventBinding("Event1", proxy_event_1);

    // And then the FindService handler is called again with a non-empty service handle container
    ServiceHandleContainer<HandleType> filled_service_handle_container{make_HandleType(identifier_)};
    saved_find_service_handler(filled_service_handle_container, valid_find_service_handle);
}

using ProxyAutoReconnectDeathTest = ProxyAutoReconnectFixture;
TEST_F(ProxyAutoReconnectDeathTest, ProxyCreateWillTerminateIfStartFindServiceReturnsError)
{
    auto start_find_service_fails = [this] {
        // Expecting that StartFindService is called and returns an error
        EXPECT_CALL(service_discovery_mock_, StartFindService(_, EnrichedInstanceIdentifier{identifier_}))
            .WillOnce(Return(MakeUnexpected(ComErrc::kServiceNotOffered)));

        // Then when creating a proxy
        // We terminate
        InitialiseProxyWithConstructor(identifier_);
    };
    EXPECT_DEATH(start_find_service_fails(), ".*");
}

TEST_F(ProxyEventBindingFixture, RegisteringEventBindingWillCallNotifyServiceInstanceChangedAvailabilityOnBinding)
{
    mock_binding::ProxyEventBase mock_proxy_event_base_binding{};

    // Given a constructed Proxy
    InitialiseProxyWithCreate(identifier_);

    // Expecting that NotifyServiceInstanceChangedAvailability will be called on the binding
    EXPECT_CALL(mock_proxy_event_base_binding, NotifyServiceInstanceChangedAvailability(_, _));

    // When calling RegisterEventBinding
    proxy_->RegisterEventBinding(kDummyEventName, mock_proxy_event_base_binding);
}

TEST_F(ProxyEventBindingFixture,
       CallingFindServiceHandlerWillCallNotifyServiceInstanceChangedAvailabilityOnAllRegisteredBindings)
{
    mock_binding::ProxyEventBase mock_proxy_event_base_binding{};
    mock_binding::ProxyEventBase mock_proxy_event_base_binding_2{};

    // Expecting that NotifyServiceInstanceChangedAvailability will be called on all registered bindings once during
    // registration and again when the find service handler is called
    EXPECT_CALL(mock_proxy_event_base_binding, NotifyServiceInstanceChangedAvailability(_, _)).Times(2);
    EXPECT_CALL(mock_proxy_event_base_binding_2, NotifyServiceInstanceChangedAvailability(_, _)).Times(2);

    // Given a constructed Proxy
    WhichCapturesFindServiceHandler(identifier_);
    InitialiseProxyWithCreate(identifier_);

    // and that two bindings are registered
    proxy_->RegisterEventBinding(kDummyEventName, mock_proxy_event_base_binding);
    proxy_->RegisterEventBinding("some_other_event", mock_proxy_event_base_binding_2);

    // When the find service handler is called
    const auto find_service_handler = find_service_handler_promise_.get_future().get();
    find_service_handler(ServiceHandleContainer<HandleType>{}, kDummyFindServiceHandle);
}

TEST_F(ProxyEventBindingFixture,
       UnregisteringEventBindingAfterRegisteringWillMakeBindingUnavailableToServiceAvailabilityChangeHandler)
{
    mock_binding::ProxyEventBase mock_proxy_event_base_binding{};

    // Expecting that NotifyServiceInstanceChangedAvailability will only be called on the binding once when it's
    // registered and not again when the find service handler is called
    EXPECT_CALL(mock_proxy_event_base_binding, NotifyServiceInstanceChangedAvailability(_, _));

    // Given a constructed Proxy which registered an event binding
    WhichCapturesFindServiceHandler(identifier_);
    InitialiseProxyWithCreate(identifier_);
    proxy_->RegisterEventBinding(kDummyEventName, mock_proxy_event_base_binding);

    // and then unregistered the event binding
    proxy_->UnregisterEventBinding(kDummyEventName);

    // When the find service handler is called
    const auto find_service_handler = find_service_handler_promise_.get_future().get();
    find_service_handler(ServiceHandleContainer<HandleType>{}, kDummyFindServiceHandle);
}

TEST_F(ProxyEventBindingFixture, UnregisteringEventBindingBeforeRegisteringWillNotTerminate)
{
    // Given a constructed Proxy
    InitialiseProxyWithCreate(identifier_);

    // When calling UnregisterEventBinding when RegisterEventBinding was never called
    proxy_->UnregisterEventBinding(kDummyEventName);

    // Then we don't terminate
}

using ProxyGetEventMetaInfoFixture = ProxyMockedMemoryFixture;
TEST_F(ProxyGetEventMetaInfoFixture, GetEventMetaInfoWillReturnDataForEventThatWasCreatedBySkeleton)
{
    // Given a dummy SkeletonEvent which creates the EventMeta Info
    InitialiseDummySkeletonEvent(kDummyElementFqId, SkeletonEventProperties{kMaxNumSlots, kMaxSubscribers, true});

    // and a constructed Proxy
    InitialiseProxyWithCreate(identifier_);

    // When getting the EventMetaInfo
    const auto event_meta_info = proxy_->GetEventMetaInfo(kDummyElementFqId);

    // Then the EventMetaInfo will contain the meta info of the SkeletonEvent type
    EXPECT_EQ(event_meta_info.data_type_info_.size_of_, sizeof(ProxyMockedMemoryFixture::SampleType));
    EXPECT_EQ(event_meta_info.data_type_info_.align_of_, alignof(ProxyMockedMemoryFixture::SampleType));
}

using ProxyGetEventMetaInfoDeathTest = ProxyGetEventMetaInfoFixture;
TEST_F(ProxyGetEventMetaInfoDeathTest, CallingGetEventMetaInfoWhenSkeletonEventDoesNotExistInSharedMemoryWillTerminate)
{
    // Given a constructed Proxy with no corresponding SkeletonEvent
    InitialiseProxyWithCreate(identifier_);

    // When getting the EventMetaInfo for a random element fq id
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = proxy_->GetEventMetaInfo(kDummyElementFqId), ".*");
}

TEST_F(ProxyGetEventMetaInfoDeathTest, CallingGetEventMetaInfoWhenGettingDataSectionBaseAddressReturnsNullptrTerminates)
{
    // Given a dummy SkeletonEvent which creates the EventMeta Info
    InitialiseDummySkeletonEvent(kDummyElementFqId, SkeletonEventProperties{kMaxNumSlots, kMaxSubscribers, true});

    // and a constructed Proxy
    InitialiseProxyWithCreate(identifier_);

    // and that getting the usable base address (from which we read the EventMetaInfo) returns a nullptr
    ON_CALL(*(fake_data_.data_memory), getUsableBaseAddress()).WillByDefault(Return(nullptr));

    // When getting the EventMetaInfo for a random element fq id
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = proxy_->GetEventMetaInfo(kDummyElementFqId), ".*");
}

class ProxyUidPidRegistrationFixture : public ProxyMockedMemoryFixture
{
  protected:
    ProxyUidPidRegistrationFixture() noexcept {}

    void AddApplicationIdPidMapping(std::uint32_t application_id, pid_t pid) noexcept
    {
        auto result = fake_data_.data_control->application_id_pid_mapping_.RegisterPid(application_id, pid);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), pid);
    }

    const InstanceIdentifier instance_identifier_{
        make_InstanceIdentifier(kServiceInstanceDeployment, kServiceTypeDeployment)};
};

TEST_F(ProxyUidPidRegistrationFixture, NoOutdatedPidNotificationWillBeSent)
{
    // Given a fake Skeleton which sets up ServiceDataControl with an initial empty UidPidMapping

    // Expect that GetApplicationId is called once, but we don't care about the return value for this test
    EXPECT_CALL(binding_runtime_, GetApplicationId()).WillOnce(Return(123));

    // we expect that IMessagePassingService::NotifyOutdatedNodeId() will NOT get called!
    EXPECT_CALL(*mock_service_, NotifyOutdatedNodeId(_, _, _)).Times(0);

    // When creating a proxy
    InitialiseProxyWithCreate(instance_identifier_);
    EXPECT_NE(proxy_, nullptr);
}

TEST_F(ProxyUidPidRegistrationFixture, OutdatedPidNotificationWillBeSent)
{
    std::uint32_t our_application_id{22};
    pid_t old_pid{1};
    pid_t new_pid{2};

    // Given a fake Skeleton which sets up ServiceDataControl with an UidPidMapping, which contains an "old pid" for
    // our uid
    AddApplicationIdPidMapping(our_application_id, old_pid);

    // expect, that the LoLa runtime returns our application id (simulating fallback to uid) and new pid
    EXPECT_CALL(binding_runtime_, GetApplicationId()).WillOnce(Return(our_application_id));
    EXPECT_CALL(binding_runtime_, GetPid()).WillRepeatedly(Return(new_pid));

    // we expect that IMessagePassingService::NotifyOutdatedNodeId() will get called to notify about an outdated pid!
    EXPECT_CALL(*mock_service_, NotifyOutdatedNodeId(_, old_pid, _)).Times(1);

    // When creating a proxy
    InitialiseProxyWithCreate(instance_identifier_);
    EXPECT_NE(proxy_, nullptr);
}

class ProxyTransactionLogRollbackFixture : public ProxyMockedMemoryFixture
{
  protected:
    ProxyTransactionLogRollbackFixture() noexcept
    {
        InitialiseDummySkeletonEvent(kDummyElementFqId, SkeletonEventProperties{kMaxNumSlots, kMaxSubscribers, true});
    }

    static constexpr std::uint32_t kDummyApplicationId{665U};
    TransactionLogId transaction_log_id_{kDummyApplicationId};
    const InstanceIdentifier instance_identifier_{
        make_InstanceIdentifier(kServiceInstanceDeployment, kServiceTypeDeployment)};

    const TransactionLog::MaxSampleCountType subscription_max_sample_count_{5U};
};

TEST_F(ProxyTransactionLogRollbackFixture, RollbackWillBeCalledOnExistingTransactionLogOnCreation)
{
    // Given a fake Skeleton and SkeletonEvent which sets up an EventDataControl containing a TransactionLogSet

    // When inserting a TransactionLog into the created TransactionLogSet which contains valid transactions
    InsertProxyTransactionLogWithValidTransactions(
        *event_control_, subscription_max_sample_count_, transaction_log_id_);
    EXPECT_TRUE(IsProxyTransactionLogIdRegistered(*event_control_, transaction_log_id_));

    EXPECT_CALL(binding_runtime_, GetApplicationId()).WillOnce(Return(transaction_log_id_));

    // When creating a proxy
    InitialiseProxyWithCreate(instance_identifier_);
    EXPECT_NE(proxy_, nullptr);

    // Then the TransactionLog should be rollbacked during construction and removed
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(*event_control_, transaction_log_id_));
}

TEST_F(ProxyTransactionLogRollbackFixture, RollbackWillBeNotBeCalledOnNonExistingTransactionLogOnCreation)
{
    // Given a fake Skeleton and SkeletonEvent which sets up an EventDataControl containing a TransactionLogSet

    // When no TransactionLog exists in the created TransactionLogSet
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(*event_control_, transaction_log_id_));

    // Given a valid deployment information

    // When creating a proxy with the same TransactionLogId
    InitialiseProxyWithCreate(instance_identifier_);
    EXPECT_NE(proxy_, nullptr);

    // Then there should still be no transaction log and we shouldn't crash
    EXPECT_FALSE(IsProxyTransactionLogIdRegistered(*event_control_, transaction_log_id_));
}

TEST_F(ProxyTransactionLogRollbackFixture, FailureInRollingBackExistingTransactionLogWillReturnEmptyProxyBinding)
{
    RecordProperty("Verifies", "SCR-31295722");
    RecordProperty(
        "Description",
        "error, which is represented by a nullptr, shall be returned if a transaction rollback is not possible.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    // Given a fake Skeleton and SkeletonEvent which sets up an EventDataControl containing a TransactionLogSet

    // When inserting a TransactionLog into the created TransactionLogSet which contains invalid transactions
    InsertProxyTransactionLogWithInvalidTransactions(
        *event_control_, subscription_max_sample_count_, transaction_log_id_);
    EXPECT_TRUE(IsProxyTransactionLogIdRegistered(*event_control_, transaction_log_id_));

    EXPECT_CALL(binding_runtime_, GetApplicationId()).WillOnce(Return(transaction_log_id_));

    // Given a valid deployment information

    // When creating a proxy
    InitialiseProxyWithCreate(instance_identifier_);

    // Then the Proxy binding will not be created.
    EXPECT_EQ(proxy_, nullptr);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
